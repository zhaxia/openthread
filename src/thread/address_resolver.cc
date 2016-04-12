/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <coap/coap_header.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <mac/mac_frame.h>
#include <thread/address_resolver.h>
#include <thread/mesh_forwarder.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>
#include <thread/thread_tlvs.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

AddressResolver::AddressResolver(ThreadNetif &netif) :
    m_address_error("a/ae", &HandleAddressError, this),
    m_address_query("a/aq", &HandleAddressQuery, this),
    m_address_notification("a/an", &HandleAddressNotification, this),
    m_icmp6_handler(&HandleDstUnreach, this),
    m_socket(&HandleUdpReceive, this),
    m_timer(&HandleTimer, this)
{
    memset(&m_cache, 0, sizeof(m_cache));
    m_mesh_forwarder = netif.GetMeshForwarder();
    m_mle = netif.GetMle();
    m_netif = &netif;

    m_coap_server = netif.GetCoapServer();
    m_coap_server->AddResource(m_address_error);
    m_coap_server->AddResource(m_address_query);
    m_coap_server->AddResource(m_address_notification);
    m_coap_message_id = Random::Get();

    Icmp6::RegisterCallbacks(m_icmp6_handler);
}

ThreadError AddressResolver::Clear()
{
    memset(&m_cache, 0, sizeof(m_cache));
    return kThreadError_None;
}

ThreadError AddressResolver::Remove(uint8_t router_id)
{
    for (int i = 0; i < kCacheEntries; i++)
    {
        if ((m_cache[i].rloc >> 10) == router_id)
        {
            m_cache[i].state = Cache::kStateInvalid;
        }
    }

    return kThreadError_None;
}

ThreadError AddressResolver::Resolve(const Ip6Address &dst, Mac::Address16 &rloc)
{
    ThreadError error = kThreadError_None;
    Cache *entry = NULL;

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (m_cache[i].state != Cache::kStateInvalid)
        {
            if (memcmp(&m_cache[i].target, &dst, sizeof(m_cache[i].target)) == 0)
            {
                entry = &m_cache[i];
                break;
            }
        }
        else if (entry == NULL)
        {
            entry = &m_cache[i];
        }
    }

    VerifyOrExit(entry != NULL, error = kThreadError_NoBufs);

    switch (entry->state)
    {
    case Cache::kStateInvalid:
        memcpy(&entry->target, &dst, sizeof(entry->target));
        entry->state = Cache::kStateDiscover;
        entry->timeout = kDiscoverTimeout;
        m_timer.Start(1000);
        SendAddressQuery(dst);
        error = kThreadError_LeaseQuery;
        break;

    case Cache::kStateDiscover:
    case Cache::kStateRetry:
        error = kThreadError_LeaseQuery;
        break;

    case Cache::kStateValid:
        rloc = entry->rloc;
        break;
    }

exit:
    return error;
}

ThreadError AddressResolver::SendAddressQuery(const Ip6Address &eid)
{
    ThreadError error;
    struct sockaddr_in6 sockaddr;
    Message *message;
    Coap::Header header;
    ThreadTargetTlv target_tlv;
    Ip6MessageInfo message_info;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin6_port = kCoapUdpPort;
    m_socket.Bind(&sockaddr);

    for (size_t i = 0; i < sizeof(m_coap_token); i++)
    {
        m_coap_token[i] = Random::Get();
    }

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeNonConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++m_coap_message_id);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions("a/aq");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    target_tlv.Init();
    target_tlv.SetTarget(eid);
    SuccessOrExit(error = message->Append(&target_tlv, sizeof(target_tlv)));

    memset(&message_info, 0, sizeof(message_info));
    message_info.peer_addr.addr16[0] = HostSwap16(0xff03);
    message_info.peer_addr.addr16[7] = HostSwap16(0x0002);
    message_info.peer_port = kCoapUdpPort;
    message_info.interface_id = m_netif->GetInterfaceId();

    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent address query\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void AddressResolver::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
}

void AddressResolver::HandleAddressNotification(void *context, Coap::Header &header, Message &message,
                                                const Ip6MessageInfo &message_info)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleAddressNotification(header, message, message_info);
}

void AddressResolver::HandleAddressNotification(Coap::Header &header, Message &message,
                                                const Ip6MessageInfo &message_info)
{
    ThreadTargetTlv target_tlv;
    ThreadMeshLocalIidTlv ml_iid_tlv;
    ThreadRlocTlv rloc_tlv;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address notification from %04x\n", HostSwap16(message_info.peer_addr.addr16[7]));

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kTarget, sizeof(target_tlv), target_tlv));
    VerifyOrExit(target_tlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kMeshLocalIid, sizeof(ml_iid_tlv), ml_iid_tlv));
    VerifyOrExit(ml_iid_tlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rloc_tlv), rloc_tlv));
    VerifyOrExit(rloc_tlv.IsValid(), ;);

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (memcmp(&m_cache[i].target, target_tlv.GetTarget(), sizeof(m_cache[i].target)) == 0)
        {
            if (m_cache[i].state != Cache::kStateValid ||
                memcmp(m_cache[i].iid, ml_iid_tlv.GetIid(), sizeof(m_cache[i].iid)) == 0)
            {
                memcpy(m_cache[i].iid, ml_iid_tlv.GetIid(), sizeof(m_cache[i].iid));
                m_cache[i].rloc = rloc_tlv.GetRloc16();
                m_cache[i].timeout = 0;
                m_cache[i].failure_count = 0;
                m_cache[i].state = Cache::kStateValid;
                SendAddressNotificationResponse(header, message_info);
                m_mesh_forwarder->HandleResolved(*target_tlv.GetTarget());
            }
            else
            {
                SendAddressError(target_tlv, ml_iid_tlv, NULL);
            }

            ExitNow();
        }
    }

    ExitNow();

exit:
    {}
}

void AddressResolver::SendAddressNotificationResponse(const Coap::Header &request_header,
                                                      const Ip6MessageInfo &request_info)
{
    ThreadError error;
    Message *message;
    Coap::Header response_header;
    Ip6MessageInfo response_info;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    response_header.Init();
    response_header.SetVersion(1);
    response_header.SetType(Coap::Header::kTypeAcknowledgment);
    response_header.SetCode(Coap::Header::kCodeChanged);
    response_header.SetMessageId(request_header.GetMessageId());
    response_header.SetToken(request_header.GetToken(), request_header.GetTokenLength());
    response_header.Finalize();
    SuccessOrExit(error = message->Append(response_header.GetBytes(), response_header.GetLength()));

    memcpy(&response_info, &request_info, sizeof(response_info));
    memset(&response_info.sock_addr, 0, sizeof(response_info.sock_addr));
    SuccessOrExit(error = m_coap_server->SendMessage(*message, response_info));

    dprintf("Sent address notification acknowledgment\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError AddressResolver::SendAddressError(const ThreadTargetTlv &target, const ThreadMeshLocalIidTlv &eid,
                                              const Ip6Address *destination)
{
    ThreadError error;
    Message *message;
    Coap::Header header;
    Ip6MessageInfo message_info;
    struct sockaddr_in6 sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin6_port = kCoapUdpPort;
    m_socket.Bind(&sockaddr);

    for (size_t i = 0; i < sizeof(m_coap_token); i++)
    {
        m_coap_token[i] = Random::Get();
    }

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeNonConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++m_coap_message_id);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions("a/ae");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));
    SuccessOrExit(error = message->Append(&target, sizeof(target)));
    SuccessOrExit(error = message->Append(&eid, sizeof(eid)));

    memset(&message_info, 0, sizeof(message_info));

    if (destination == NULL)
    {
        message_info.peer_addr.addr16[0] = HostSwap16(0xff03);
        message_info.peer_addr.addr16[7] = HostSwap16(0x0002);
    }
    else
    {
        memcpy(&message_info.peer_addr, destination, sizeof(message_info.peer_addr));
    }

    message_info.peer_port = kCoapUdpPort;
    message_info.interface_id = m_netif->GetInterfaceId();

    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent address error\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void AddressResolver::HandleAddressError(void *context, Coap::Header &header,
                                         Message &message, const Ip6MessageInfo &message_info)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleAddressError(header, message, message_info);
}

void AddressResolver::HandleAddressError(Coap::Header &header, Message &message,
                                         const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    ThreadTargetTlv target_tlv;
    ThreadMeshLocalIidTlv ml_iid_tlv;
    Child *children;
    uint8_t num_children;
    Mac::Address64 mac_addr;
    Ip6Address destination;

    VerifyOrExit(header.GetCode() == Coap::Header::kCodePost, error = kThreadError_Drop);

    dprintf("Received address error notification\n");

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kTarget, sizeof(target_tlv), target_tlv));
    VerifyOrExit(target_tlv.IsValid(), error = kThreadError_Parse);

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kMeshLocalIid, sizeof(ml_iid_tlv), ml_iid_tlv));
    VerifyOrExit(ml_iid_tlv.IsValid(), error = kThreadError_Parse);

    for (const NetifUnicastAddress *address = m_netif->GetUnicastAddresses(); address; address = address->GetNext())
    {
        if (memcmp(&address->address, target_tlv.GetTarget(), sizeof(address->address)) == 0 &&
            memcmp(m_mle->GetMeshLocal64()->addr8 + 8, ml_iid_tlv.GetIid(), 8))
        {
            // Target EID matches address and Mesh Local EID differs
            m_netif->RemoveUnicastAddress(*address);
            ExitNow();
        }
    }

    children = m_mle->GetChildren(&num_children);

    memcpy(&mac_addr, ml_iid_tlv.GetIid(), sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    for (int i = 0; i < Mle::kMaxChildren; i++)
    {
        if (children[i].state != Neighbor::kStateValid || (children[i].mode & Mle::kModeFFD) != 0)
        {
            continue;
        }

        for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++)
        {
            if (memcmp(&children[i].ip6_address[j], target_tlv.GetTarget(), sizeof(children[i].ip6_address[j])) == 0 &&
                memcmp(&children[i].mac_addr, &mac_addr, sizeof(children[i].mac_addr)))
            {
                // Target EID matches child address and Mesh Local EID differs on child
                memset(&children[i].ip6_address[j], 0, sizeof(children[i].ip6_address[j]));

                memset(&destination, 0, sizeof(destination));
                destination.addr16[0] = HostSwap16(0xfe80);
                memcpy(destination.addr8 + 8, &children[i].mac_addr, 8);
                destination.addr8[8] ^= 0x2;

                SendAddressError(target_tlv, ml_iid_tlv, &destination);
                ExitNow();
            }
        }
    }

exit:
    {}
}

void AddressResolver::HandleAddressQuery(void *context, Coap::Header &header, Message &message,
                                         const Ip6MessageInfo &message_info)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleAddressQuery(header, message, message_info);
}

void AddressResolver::HandleAddressQuery(Coap::Header &header, Message &message,
                                         const Ip6MessageInfo &message_info)
{
    ThreadTargetTlv target_tlv;
    ThreadMeshLocalIidTlv ml_iid_tlv;
    ThreadLastTransactionTimeTlv last_transaction_time_tlv;
    Child *children;
    uint8_t num_children;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeNonConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address query from %04x\n", HostSwap16(message_info.peer_addr.addr16[7]));

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kTarget, sizeof(target_tlv), target_tlv));
    VerifyOrExit(target_tlv.IsValid(), ;);

    ml_iid_tlv.Init();

    last_transaction_time_tlv.Init();

    if (m_netif->IsUnicastAddress(*target_tlv.GetTarget()))
    {
        ml_iid_tlv.SetIid(m_mle->GetMeshLocal64()->addr8 + 8);
        SendAddressQueryResponse(target_tlv, ml_iid_tlv, NULL, message_info.peer_addr);
        ExitNow();
    }

    children = m_mle->GetChildren(&num_children);

    for (int i = 0; i < Mle::kMaxChildren; i++)
    {
        if (children[i].state != Neighbor::kStateValid || (children[i].mode & Mle::kModeFFD) != 0)
        {
            continue;
        }

        for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++)
        {
            if (memcmp(&children[i].ip6_address[j], target_tlv.GetTarget(), sizeof(children[i].ip6_address[j])))
            {
                continue;
            }

            children[i].mac_addr.bytes[0] ^= 0x2;
            ml_iid_tlv.SetIid(children[i].mac_addr.bytes);
            children[i].mac_addr.bytes[0] ^= 0x2;
            last_transaction_time_tlv.SetTime(Timer::GetNow() - children[i].last_heard);
            SendAddressQueryResponse(target_tlv, ml_iid_tlv, &last_transaction_time_tlv, message_info.peer_addr);
            ExitNow();
        }
    }

exit:
    {}
}

void AddressResolver::SendAddressQueryResponse(const ThreadTargetTlv &target_tlv,
                                               const ThreadMeshLocalIidTlv &ml_iid_tlv,
                                               const ThreadLastTransactionTimeTlv *last_transaction_time_tlv,
                                               const Ip6Address &destination)
{
    ThreadError error;
    Message *message;
    Coap::Header header;
    ThreadRlocTlv rloc_tlv;
    Ip6MessageInfo message_info;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.Init();
    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++m_coap_message_id);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions("a/an");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    SuccessOrExit(error = message->Append(&target_tlv, sizeof(target_tlv)));
    SuccessOrExit(error = message->Append(&ml_iid_tlv, sizeof(ml_iid_tlv)));

    rloc_tlv.Init();
    rloc_tlv.SetRloc16(m_mle->GetRloc16());
    SuccessOrExit(error = message->Append(&rloc_tlv, sizeof(rloc_tlv)));

    if (last_transaction_time_tlv != NULL)
    {
        SuccessOrExit(error = message->Append(&last_transaction_time_tlv, sizeof(*last_transaction_time_tlv)));
    }

    memset(&message_info, 0, sizeof(message_info));
    memcpy(&message_info.peer_addr, &destination, sizeof(message_info.peer_addr));
    message_info.interface_id = message_info.interface_id;
    message_info.peer_port = kCoapUdpPort;

    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent address notification\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

void AddressResolver::HandleTimer(void *context)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleTimer();
}

void AddressResolver::HandleTimer()
{
    bool continue_timer = false;

    for (int i = 0; i < kCacheEntries; i++)
    {
        switch (m_cache[i].state)
        {
        case Cache::kStateDiscover:
            m_cache[i].timeout--;

            if (m_cache[i].timeout == 0)
            {
                m_cache[i].state = Cache::kStateInvalid;
            }
            else
            {
                continue_timer = true;
            }

            break;

        default:
            break;
        }
    }

    if (continue_timer)
    {
        m_timer.Start(1000);
    }
}

const AddressResolver::Cache *AddressResolver::GetCacheEntries(uint16_t *num_entries) const
{
    if (num_entries)
    {
        *num_entries = kCacheEntries;
    }

    return m_cache;
}

void AddressResolver::HandleDstUnreach(void *context, Message &message, const Ip6MessageInfo &message_info,
                                       const Icmp6Header &icmp6_header)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleDstUnreach(message, message_info, icmp6_header);
}

void AddressResolver::HandleDstUnreach(Message &message, const Ip6MessageInfo &message_info,
                                       const Icmp6Header &icmp6_header)
{
    VerifyOrExit(icmp6_header.GetCode() == Icmp6Header::kCodeDstUnreachNoRoute, ;);

    Ip6Header ip6_header;
    VerifyOrExit(message.Read(message.GetOffset(), sizeof(ip6_header), &ip6_header) == sizeof(ip6_header), ;);

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (m_cache[i].state != Cache::kStateInvalid &&
            memcmp(&m_cache[i].target, ip6_header.GetDestination(), sizeof(m_cache[i].target)) == 0)
        {
            m_cache[i].state = Cache::kStateInvalid;
            dprintf("cache entry removed!\n");
            break;
        }
    }

exit:
    {}
}

}  // namespace Thread
