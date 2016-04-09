/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#include <thread/network_data_local.h>
#include <coap/coap_header.h>
#include <common/code_utils.h>
#include <common/random.h>
#include <thread/thread_netif.h>
#include <thread/thread_tlvs.h>

namespace Thread {
namespace NetworkData {

Local::Local(ThreadNetif &netif):
    m_socket(&HandleUdpReceive, this)
{
    m_mle = netif.GetMle();
}

ThreadError Local::AddOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length, int8_t prf,
                                   uint8_t flags, bool stable)
{
    PrefixTlv *prefix_tlv;
    BorderRouterTlv *br_tlv;

    RemoveOnMeshPrefix(prefix, prefix_length);

    prefix_tlv = reinterpret_cast<PrefixTlv *>(m_tlvs + m_length);
    Insert(reinterpret_cast<uint8_t *>(prefix_tlv),
           sizeof(PrefixTlv) + (prefix_length + 7) / 8 + sizeof(BorderRouterTlv) + sizeof(BorderRouterEntry));
    prefix_tlv->Init(0, prefix_length, prefix);
    prefix_tlv->SetSubTlvsLength(sizeof(BorderRouterTlv) + sizeof(BorderRouterEntry));

    br_tlv = reinterpret_cast<BorderRouterTlv *>(prefix_tlv->GetSubTlvs());
    br_tlv->Init();
    br_tlv->SetLength(br_tlv->GetLength() + sizeof(BorderRouterEntry));
    br_tlv->GetEntry(0)->Init();
    br_tlv->GetEntry(0)->SetPreference(prf);
    br_tlv->GetEntry(0)->SetFlags(flags);

    if (stable)
    {
        prefix_tlv->SetStable();
        br_tlv->SetStable();
    }

    dump("add prefix done", m_tlvs, m_length);
    return kThreadError_None;
}

ThreadError Local::RemoveOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *tlv;

    VerifyOrExit((tlv = FindPrefix(prefix, prefix_length)) != NULL, error = kThreadError_Error);
    VerifyOrExit(FindBorderRouter(*tlv) != NULL, error = kThreadError_Error);
    Remove(reinterpret_cast<uint8_t *>(tlv), sizeof(NetworkDataTlv) + tlv->GetLength());

exit:
    dump("remove done", m_tlvs, m_length);
    return error;
}

ThreadError Local::AddHasRoutePrefix(const uint8_t *prefix, uint8_t prefix_length, int8_t prf, bool stable)
{
    PrefixTlv *prefix_tlv;
    HasRouteTlv *has_route_tlv;

    RemoveHasRoutePrefix(prefix, prefix_length);

    prefix_tlv = reinterpret_cast<PrefixTlv *>(m_tlvs + m_length);
    Insert(reinterpret_cast<uint8_t *>(prefix_tlv),
           sizeof(PrefixTlv) + (prefix_length + 7) / 8 + sizeof(HasRouteTlv) + sizeof(HasRouteEntry));
    prefix_tlv->Init(0, prefix_length, prefix);
    prefix_tlv->SetSubTlvsLength(sizeof(HasRouteTlv) + sizeof(HasRouteEntry));

    has_route_tlv = reinterpret_cast<HasRouteTlv *>(prefix_tlv->GetSubTlvs());
    has_route_tlv->Init();
    has_route_tlv->SetLength(has_route_tlv->GetLength() + sizeof(HasRouteEntry));
    has_route_tlv->GetEntry(0)->Init();
    has_route_tlv->GetEntry(0)->SetPreference(prf);

    if (stable)
    {
        prefix_tlv->SetStable();
        has_route_tlv->SetStable();
    }

    dump("add route done", m_tlvs, m_length);
    return kThreadError_None;
}

ThreadError Local::RemoveHasRoutePrefix(const uint8_t *prefix, uint8_t prefix_length)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *tlv;

    VerifyOrExit((tlv = FindPrefix(prefix, prefix_length)) != NULL, error = kThreadError_Error);
    VerifyOrExit(FindHasRoute(*tlv) != NULL, error = kThreadError_Error);
    Remove(reinterpret_cast<uint8_t *>(tlv), sizeof(NetworkDataTlv) + tlv->GetLength());

exit:
    dump("remove done", m_tlvs, m_length);
    return error;
}

ThreadError Local::UpdateRloc()
{
    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
         cur = cur->GetNext())
    {
        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
            UpdateRloc(*reinterpret_cast<PrefixTlv *>(cur));
            break;

        default:
            assert(false);
            break;
        }
    }

    return kThreadError_None;
}

ThreadError Local::UpdateRloc(PrefixTlv &prefix)
{
    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
         cur < reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());
         cur = cur->GetNext())
    {
        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypeHasRoute:
            UpdateRloc(*reinterpret_cast<HasRouteTlv *>(cur));
            break;

        case NetworkDataTlv::kTypeBorderRouter:
            UpdateRloc(*reinterpret_cast<BorderRouterTlv *>(cur));
            break;

        default:
            assert(false);
            break;
        }
    }

    return kThreadError_None;
}

ThreadError Local::UpdateRloc(HasRouteTlv &has_route)
{
    HasRouteEntry *entry = has_route.GetEntry(0);
    entry->SetRloc(m_mle->GetRloc16());
    return kThreadError_None;
}

ThreadError Local::UpdateRloc(BorderRouterTlv &border_router)
{
    BorderRouterEntry *entry = border_router.GetEntry(0);
    entry->SetRloc(m_mle->GetRloc16());
    return kThreadError_None;
}

ThreadError Local::Register(const Ip6Address &destination)
{
    ThreadError error = kThreadError_None;
    Coap::Header header;
    Message *message;
    Ip6MessageInfo message_info;

    UpdateRloc();
    m_socket.Bind(NULL);

    for (size_t i = 0; i < sizeof(m_coap_token); i++)
    {
        m_coap_token[i] = Random::Get();
    }

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++m_coap_message_id);
    header.SetToken(m_coap_token, sizeof(m_coap_token));
    header.AppendUriPathOptions("n/sd");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));
    SuccessOrExit(error = message->Append(m_tlvs, m_length));

    memset(&message_info, 0, sizeof(message_info));
    memcpy(&message_info.peer_addr, &destination, sizeof(message_info.peer_addr));
    message_info.peer_port = kCoapUdpPort;
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent network data registration\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void Local::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    Local *obj = reinterpret_cast<Local *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void Local::HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info)
{
    Coap::Header header;

    SuccessOrExit(header.FromMessage(message));
    VerifyOrExit(header.GetType() == Coap::Header::kTypeAcknowledgment &&
                 header.GetCode() == Coap::Header::kCodeChanged &&
                 header.GetMessageId() == m_coap_message_id &&
                 header.GetTokenLength() == sizeof(m_coap_token) &&
                 memcmp(m_coap_token, header.GetToken(), sizeof(m_coap_token)) == 0, ;);

    dprintf("Network data registration acknowledged\n");

exit:
    {}
}

}  // namespace NetworkData
}  // namespace Thread

