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

#include <thread/mle.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/random.h>
#include <crypto/aes_ccm.h>
#include <mac/mac_frame.h>
#include <net/netif.h>
#include <net/udp6.h>
#include <thread/address_resolver.h>
#include <thread/key_manager.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>
#include <assert.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Mle {

Mle::Mle(ThreadNetif &netif) :
    m_netif_handler(&HandleUnicastAddressesChanged, this),
    m_parent_request_timer(&HandleParentRequestTimer, this),
    m_socket(&HandleUdpReceive, this)
{
    m_netif = &netif;
    m_address_resolver = netif.GetAddressResolver();
    m_key_manager = netif.GetKeyManager();
    m_mesh = netif.GetMeshForwarder();
    m_mle_router = netif.GetMle();
    m_network_data = netif.GetNetworkDataLeader();
}

ThreadError Mle::Init()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_device_state == kDeviceStateDisabled, error = kThreadError_Busy);

    memset(&m_leader_data, 0, sizeof(m_leader_data));
    memset(&m_parent, 0, sizeof(m_parent));
    memset(&m_child_id_request, 0, sizeof(m_child_id_request));
    memset(&m_link_local_64, 0, sizeof(m_link_local_64));
    memset(&m_link_local_16, 0, sizeof(m_link_local_16));
    memset(&m_mesh_local_64, 0, sizeof(m_mesh_local_64));
    memset(&m_mesh_local_16, 0, sizeof(m_mesh_local_16));
    memset(&m_link_local_all_thread_nodes, 0, sizeof(m_link_local_all_thread_nodes));
    memset(&m_realm_local_all_thread_nodes, 0, sizeof(m_realm_local_all_thread_nodes));

    // link-local 64
    memset(&m_link_local_64, 0, sizeof(m_link_local_64));
    m_link_local_64.address.addr16[0] = HostSwap16(0xfe80);
    memcpy(m_link_local_64.address.addr8 + 8, m_mesh->GetAddress64(), 8);
    m_link_local_64.address.addr8[8] ^= 2;
    m_link_local_64.prefix_length = 64;
    m_link_local_64.preferred_lifetime = 0xffffffff;
    m_link_local_64.valid_lifetime = 0xffffffff;
    m_netif->AddUnicastAddress(m_link_local_64);

    // link-local 16
    memset(&m_link_local_16, 0, sizeof(m_link_local_16));
    m_link_local_16.address.addr16[0] = HostSwap16(0xfe80);
    m_link_local_16.address.addr16[5] = HostSwap16(0x00ff);
    m_link_local_16.address.addr16[6] = HostSwap16(0xfe00);
    m_link_local_16.prefix_length = 64;
    m_link_local_16.preferred_lifetime = 0xffffffff;
    m_link_local_16.valid_lifetime = 0xffffffff;

    // mesh-local 64
    for (int i = 8; i < 16; i++)
    {
        m_mesh_local_64.address.addr8[i] = Random::Get();
    }

    m_mesh_local_64.prefix_length = 64;
    m_mesh_local_64.preferred_lifetime = 0xffffffff;
    m_mesh_local_64.valid_lifetime = 0xffffffff;
    m_netif->AddUnicastAddress(m_mesh_local_64);

    // mesh-local 16
    m_mesh_local_16.address.addr16[4] = HostSwap16(0x0000);
    m_mesh_local_16.address.addr16[5] = HostSwap16(0x00ff);
    m_mesh_local_16.address.addr16[6] = HostSwap16(0xfe00);
    m_mesh_local_16.prefix_length = 64;
    m_mesh_local_16.preferred_lifetime = 0xffffffff;
    m_mesh_local_16.valid_lifetime = 0xffffffff;

    // link-local all thread nodes
    m_link_local_all_thread_nodes.address.addr16[0] = HostSwap16(0xff32);
    m_link_local_all_thread_nodes.address.addr16[6] = HostSwap16(0x0000);
    m_link_local_all_thread_nodes.address.addr16[7] = HostSwap16(0x0001);
    m_netif->SubscribeMulticast(m_link_local_all_thread_nodes);

    // realm-local all thread nodes
    m_realm_local_all_thread_nodes.address.addr16[0] = HostSwap16(0xff33);
    m_realm_local_all_thread_nodes.address.addr16[6] = HostSwap16(0x0000);
    m_realm_local_all_thread_nodes.address.addr16[7] = HostSwap16(0x0001);
    m_netif->SubscribeMulticast(m_realm_local_all_thread_nodes);

    m_netif->RegisterHandler(m_netif_handler);

exit:
    return error;
}

ThreadError Mle::Start()
{
    ThreadError error = kThreadError_None;
    struct sockaddr_in6 sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    // memcpy(&sockaddr.sin6_addr, &m_link_local_64.address, sizeof(sockaddr.sin6_addr));
    sockaddr.sin6_port = kUdpPort;
    SuccessOrExit(error = m_socket.Bind(&sockaddr));

    m_device_state = kDeviceStateDetached;
    SetStateDetached();

    if (GetRloc16() == Mac::kShortAddrInvalid)
    {
        BecomeChild(kJoinAnyPartition);
    }
    else if (GetChildId(GetRloc16()) == 0)
    {
        m_mle_router->BecomeRouter();
    }
    else
    {
        SendChildUpdateRequest();
        m_parent_request_state = kParentSynchronize;
        m_parent_request_timer.Start(1000);
    }

exit:
    return error;
}

ThreadError Mle::Stop()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_device_state != kDeviceStateDisabled, error = kThreadError_Busy);

    SetStateDetached();
    m_socket.Close();
    m_netif->RemoveUnicastAddress(m_link_local_16);
    m_netif->RemoveUnicastAddress(m_mesh_local_16);
    m_device_state = kDeviceStateDisabled;

exit:
    return error;
}

ThreadError Mle::BecomeDetached()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_device_state != kDeviceStateDisabled, error = kThreadError_Busy);

    SetStateDetached();
    SetRloc16(Mac::kShortAddrInvalid);
    BecomeChild(kJoinAnyPartition);

exit:
    return error;
}

ThreadError Mle::BecomeChild(JoinMode mode)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_device_state != kDeviceStateDisabled &&
                 m_parent_request_state == kParentIdle, error = kThreadError_Busy);

    m_parent_request_state = kParentRequestStart;
    m_parent_request_mode = mode;
    memset(&m_parent, 0, sizeof(m_parent));

    if (mode == kJoinAnyPartition)
    {
        m_parent.state = Neighbor::kStateInvalid;
    }

    m_parent_request_timer.Start(1000);

exit:
    return error;
}

DeviceState Mle::GetDeviceState() const
{
    return m_device_state;
}

ThreadError Mle::SetStateDetached()
{
    m_address_resolver->Clear();
    m_device_state = kDeviceStateDetached;
    m_parent_request_state = kParentIdle;
    m_parent_request_timer.Stop();
    m_mesh->SetRxOnWhenIdle(true);
    m_mle_router->HandleDetachStart();
    dprintf("Mode -> Detached\n");
    return kThreadError_None;
}

ThreadError Mle::SetStateChild(uint16_t rloc16)
{
    SetRloc16(rloc16);
    m_device_state = kDeviceStateChild;
    m_parent_request_state = kParentIdle;

    if ((m_device_mode & kModeRxOnWhenIdle) != 0)
    {
        m_parent_request_timer.Start((m_timeout / 2) * 1000U);
    }

    if ((m_device_mode & kModeFFD) != 0)
    {
        m_mle_router->HandleChildStart(m_parent_request_mode);
    }

    dprintf("Mode -> Child\n");
    return kThreadError_None;
}

uint32_t Mle::GetTimeout() const
{
    return m_timeout;
}

ThreadError Mle::SetTimeout(uint32_t timeout)
{
    if (timeout < 2)
    {
        timeout = 2;
    }

    m_timeout = timeout;

    if (m_device_state == kDeviceStateChild)
    {
        SendChildUpdateRequest();

        if ((m_device_mode & kModeRxOnWhenIdle) != 0)
        {
            m_parent_request_timer.Start((m_timeout / 2) * 1000U);
        }
    }

    return kThreadError_None;
}

uint8_t Mle::GetDeviceMode() const
{
    return m_device_mode;
}

ThreadError Mle::SetDeviceMode(uint8_t device_mode)
{
    ThreadError error = kThreadError_None;
    uint8_t old_mode = m_device_mode;

    VerifyOrExit((device_mode & kModeFFD) == 0 || (device_mode & kModeRxOnWhenIdle) != 0,
                 error = kThreadError_InvalidArgs);

    m_device_mode = device_mode;

    switch (m_device_state)
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        break;

    case kDeviceStateChild:
        SetStateChild(GetRloc16());
        SendChildUpdateRequest();
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        if ((old_mode & kModeFFD) != 0 && (device_mode & kModeFFD) == 0)
        {
            BecomeDetached();
        }

        break;
    }

exit:
    return error;
}

const uint8_t *Mle::GetMeshLocalPrefix() const
{
    return m_mesh_local_16.address.addr8;
}

ThreadError Mle::SetMeshLocalPrefix(const uint8_t *xpanid)
{
    m_mesh_local_64.address.addr8[0] = 0xfd;
    memcpy(m_mesh_local_64.address.addr8 + 1, xpanid, 5);
    m_mesh_local_64.address.addr8[6] = 0x00;
    m_mesh_local_64.address.addr8[7] = 0x00;

    memcpy(&m_mesh_local_16.address, &m_mesh_local_64.address, 8);

    m_link_local_all_thread_nodes.address.addr8[3] = 64;
    memcpy(m_link_local_all_thread_nodes.address.addr8 + 4, &m_mesh_local_64.address, 8);

    m_realm_local_all_thread_nodes.address.addr8[3] = 64;
    memcpy(m_realm_local_all_thread_nodes.address.addr8 + 4, &m_mesh_local_64.address, 8);

    return kThreadError_None;
}

const uint8_t Mle::GetChildId(uint16_t rloc16) const
{
    return rloc16 & kChildIdMask;
}

const uint8_t Mle::GetRouterId(uint16_t rloc16) const
{
    return rloc16 >> kRouterIdOffset;
}

const uint16_t Mle::GetRloc16(uint8_t router_id) const
{
    return static_cast<uint16_t>(router_id) << kRouterIdOffset;
}

const Ip6Address *Mle::GetLinkLocalAllThreadNodesAddress() const
{
    return &m_link_local_all_thread_nodes.address;
}

const Ip6Address *Mle::GetRealmLocalAllThreadNodesAddress() const
{
    return &m_realm_local_all_thread_nodes.address;
}

uint16_t Mle::GetRloc16() const
{
    return m_mesh->GetAddress16();
}

ThreadError Mle::SetRloc16(uint16_t rloc16)
{
    if (rloc16 != Mac::kShortAddrInvalid)
    {
        // link-local 16
        m_link_local_16.address.addr16[7] = HostSwap16(rloc16);
        m_netif->AddUnicastAddress(m_link_local_16);

        // mesh-local 16
        m_mesh_local_16.address.addr16[7] = HostSwap16(rloc16);
        m_netif->AddUnicastAddress(m_mesh_local_16);
    }
    else
    {
        m_netif->RemoveUnicastAddress(m_link_local_16);
        m_netif->RemoveUnicastAddress(m_mesh_local_16);
    }

    m_mesh->SetAddress16(rloc16);

    return kThreadError_None;
}

uint8_t Mle::GetLeaderId() const
{
    return m_leader_data.GetRouterId();
}

const Ip6Address *Mle::GetMeshLocal16() const
{
    return &m_mesh_local_16.address;
}

const Ip6Address *Mle::GetMeshLocal64() const
{
    return &m_mesh_local_64.address;
}

ThreadError Mle::GetLeaderAddress(Ip6Address &address) const
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(GetRloc16() != Mac::kShortAddrInvalid, error = kThreadError_Error);

    memcpy(&address, &m_mesh_local_16.address, 8);
    address.addr16[4] = HostSwap16(0x0000);
    address.addr16[5] = HostSwap16(0x00ff);
    address.addr16[6] = HostSwap16(0xfe00);
    address.addr16[7] = HostSwap16(GetRloc16(m_leader_data.GetRouterId()));

exit:
    return error;
}

const LeaderDataTlv *Mle::GetLeaderDataTlv()
{
    m_leader_data.SetDataVersion(m_network_data->GetVersion());
    m_leader_data.SetStableDataVersion(m_network_data->GetStableVersion());
    return &m_leader_data;
}

void Mle::GenerateNonce(const Mac::Address64 &mac_addr, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce)
{
    // source address
    for (int i = 0; i < 8; i++)
    {
        nonce[i] = mac_addr.bytes[i];
    }

    nonce += 8;

    // frame counter
    nonce[0] = frame_counter >> 24;
    nonce[1] = frame_counter >> 16;
    nonce[2] = frame_counter >> 8;
    nonce[3] = frame_counter >> 0;
    nonce += 4;

    // security level
    nonce[0] = security_level;
}

ThreadError Mle::AppendSecureHeader(Message &message, Header::Command command)
{
    ThreadError error = kThreadError_None;
    Header header;

    header.Init();

    if (command == Header::kCommandAdvertisement ||
        command == Header::kCommandChildIdRequest ||
        command == Header::kCommandLinkReject ||
        command == Header::kCommandParentRequest ||
        command == Header::kCommandParentResponse)
    {
        header.SetKeyIdMode2();
    }
    else
    {
        header.SetKeyIdMode1();
    }

    header.SetCommand(command);

    SuccessOrExit(error = message.Append(&header, header.GetLength()));

exit:
    return error;
}

ThreadError Mle::AppendSourceAddress(Message &message)
{
    SourceAddressTlv tlv;

    tlv.Init();
    tlv.SetRloc16(GetRloc16());

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendStatus(Message &message, StatusTlv::Status status)
{
    StatusTlv tlv;

    tlv.Init();
    tlv.SetStatus(status);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendMode(Message &message, uint8_t mode)
{
    ModeTlv tlv;

    tlv.Init();
    tlv.SetMode(mode);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendTimeout(Message &message, uint32_t timeout)
{
    TimeoutTlv tlv;

    tlv.Init();
    tlv.SetTimeout(timeout);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendChallenge(Message &message, const uint8_t *challenge, uint8_t challenge_length)
{
    ThreadError error;
    Tlv tlv;

    tlv.SetType(Tlv::kChallenge);
    tlv.SetLength(challenge_length);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));
    SuccessOrExit(error = message.Append(challenge, challenge_length));
exit:
    return error;
}

ThreadError Mle::AppendResponse(Message &message, const uint8_t *response, uint8_t response_len)
{
    ThreadError error;
    Tlv tlv;

    tlv.SetType(Tlv::kResponse);
    tlv.SetLength(response_len);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));
    SuccessOrExit(error = message.Append(response, response_len));

exit:
    return error;
}

ThreadError Mle::AppendLinkFrameCounter(Message &message)
{
    LinkFrameCounterTlv tlv;

    tlv.Init();
    tlv.SetFrameCounter(m_key_manager->GetMacFrameCounter());

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendMleFrameCounter(Message &message)
{
    MleFrameCounterTlv tlv;

    tlv.Init();
    tlv.SetFrameCounter(m_key_manager->GetMleFrameCounter());

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendAddress16(Message &message, uint16_t rloc16)
{
    Address16Tlv tlv;

    tlv.Init();
    tlv.SetRloc16(rloc16);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendLeaderData(Message &message)
{
    m_leader_data.Init();
    m_leader_data.SetDataVersion(m_network_data->GetVersion());
    m_leader_data.SetStableDataVersion(m_network_data->GetStableVersion());

    return message.Append(&m_leader_data, sizeof(m_leader_data));
}

ThreadError Mle::AppendNetworkData(Message &message, bool stable_only)
{
    ThreadError error = kThreadError_None;
    NetworkDataTlv tlv;
    uint8_t length;

    tlv.Init();
    SuccessOrExit(error = m_network_data->GetNetworkData(stable_only, tlv.GetNetworkData(), length));
    tlv.SetLength(length);

    SuccessOrExit(error = message.Append(&tlv, sizeof(Tlv) + tlv.GetLength()));

exit:
    return error;
}

ThreadError Mle::AppendTlvRequest(Message &message, const uint8_t *tlvs, uint8_t tlvs_length)
{
    ThreadError error;
    Tlv tlv;

    tlv.SetType(Tlv::kTlvRequest);
    tlv.SetLength(tlvs_length);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));
    SuccessOrExit(error = message.Append(tlvs, tlvs_length));

exit:
    return error;
}

ThreadError Mle::AppendScanMask(Message &message, uint8_t scan_mask)
{
    ScanMaskTlv tlv;

    tlv.Init();
    tlv.SetMask(scan_mask);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendLinkMargin(Message &message, uint8_t link_margin)
{
    LinkMarginTlv tlv;

    tlv.Init();
    tlv.SetLinkMargin(link_margin);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendVersion(Message &message)
{
    VersionTlv tlv;

    tlv.Init();
    tlv.SetVersion(kVersion);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendIp6Address(Message &message)
{
    ThreadError error;
    Tlv tlv;
    AddressRegistrationEntry entry;
    Context context;
    uint8_t length = 0;

    tlv.SetType(Tlv::kAddressRegistration);

    // compute size of TLV
    for (const NetifUnicastAddress *addr = m_netif->GetUnicastAddresses(); addr; addr = addr->GetNext())
    {
        if (addr->address.IsLinkLocal() || addr->address == m_mesh_local_16.address)
        {
            continue;
        }

        if (m_network_data->GetContext(addr->address, context) == kThreadError_None)
        {
            // compressed entry
            length += 9;
        }
        else
        {
            // uncompressed entry
            length += 17;
        }
    }

    tlv.SetLength(length);
    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));

    // write entries to message
    for (const NetifUnicastAddress *addr = m_netif->GetUnicastAddresses(); addr; addr = addr->GetNext())
    {
        if (addr->address.IsLinkLocal() || addr->address == m_mesh_local_16.address)
        {
            continue;
        }

        if (m_network_data->GetContext(addr->address, context) == kThreadError_None)
        {
            // compressed entry
            entry.SetContextId(context.context_id);
            entry.SetIid(addr->address.addr8 + 8);
            length = 9;
        }
        else
        {
            // uncompressed entry
            entry.SetUncompressed();
            entry.SetIp6Address(addr->address);
            length = 17;
        }

        SuccessOrExit(error = message.Append(&entry, length));
    }

exit:
    return error;
}

void Mle::HandleUnicastAddressesChanged(void *context)
{
    Mle *obj = reinterpret_cast<Mle *>(context);
    obj->HandleUnicastAddressesChanged();
}

void Mle::HandleUnicastAddressesChanged()
{
    if (!m_netif->IsUnicastAddress(m_mesh_local_64.address))
    {
        // Mesh Local EID was removed, choose a new one and add it back
        for (int i = 8; i < 16; i++)
        {
            m_mesh_local_64.address.addr8[i] = Random::Get();
        }

        m_netif->AddUnicastAddress(m_mesh_local_64);
    }

    switch (m_device_state)
    {
    case kDeviceStateChild:
        SendChildUpdateRequest();
        break;

    default:
        break;
    }
}

void Mle::HandleParentRequestTimer(void *context)
{
    Mle *obj = reinterpret_cast<Mle *>(context);
    obj->HandleParentRequestTimer();
}

void Mle::HandleParentRequestTimer()
{
    switch (m_parent_request_state)
    {
    case kParentIdle:
        if (m_parent.state == Neighbor::kStateValid)
        {
            if (m_device_mode & kModeRxOnWhenIdle)
            {
                SendChildUpdateRequest();
                m_parent_request_timer.Start((m_timeout / 2) * 1000U);
            }
        }
        else
        {
            BecomeDetached();
        }

        break;

    case kParentSynchronize:
        m_parent_request_state = kParentIdle;
        BecomeChild(kJoinAnyPartition);
        break;

    case kParentRequestStart:
        m_parent_request_state = kParentRequestRouter;
        m_parent.state = Neighbor::kStateInvalid;
        SendParentRequest();
        m_parent_request_timer.Start(kParentRequestRouterTimeout);
        break;

    case kParentRequestRouter:
        m_parent_request_state = kParentRequestChild;

        if (m_parent.state == Neighbor::kStateValid)
        {
            SendChildIdRequest();
            m_parent_request_state = kChildIdRequest;
        }
        else
        {
            SendParentRequest();
        }

        m_parent_request_timer.Start(kParentRequestChildTimeout);
        break;

    case kParentRequestChild:
        m_parent_request_state = kParentRequestChild;

        if (m_parent.state == Neighbor::kStateValid)
        {
            SendChildIdRequest();
            m_parent_request_state = kChildIdRequest;
            m_parent_request_timer.Start(kParentRequestChildTimeout);
        }
        else
        {
            switch (m_parent_request_mode)
            {
            case kJoinAnyPartition:
                if (m_device_mode & kModeFFD)
                {
                    m_mle_router->BecomeLeader();
                }
                else
                {
                    m_parent_request_state = kParentIdle;
                    BecomeDetached();
                }

                break;

            case kJoinSamePartition:
                m_parent_request_state = kParentIdle;
                BecomeChild(kJoinAnyPartition);
                break;

            case kJoinBetterPartition:
                m_parent_request_state = kParentIdle;
                break;
            }
        }

        break;

    case kChildIdRequest:
        m_parent_request_state = kParentIdle;

        if (m_device_state != kDeviceStateRouter && m_device_state != kDeviceStateLeader)
        {
            BecomeDetached();
        }

        break;
    }
}

ThreadError Mle::SendParentRequest()
{
    ThreadError error = kThreadError_None;
    Message *message;
    uint8_t scan_mask = 0;
    Ip6Address destination;

    for (uint8_t i = 0; i < sizeof(m_parent_request.challenge); i++)
    {
        m_parent_request.challenge[i] = Random::Get();
    }

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandParentRequest));
    SuccessOrExit(error = AppendMode(*message, m_device_mode));
    SuccessOrExit(error = AppendChallenge(*message, m_parent_request.challenge, sizeof(m_parent_request.challenge)));

    switch (m_parent_request_state)
    {
    case kParentRequestRouter:
        scan_mask = ScanMaskTlv::kRouterFlag;
        break;

    case kParentRequestChild:
        scan_mask = ScanMaskTlv::kRouterFlag | ScanMaskTlv::kChildFlag;
        break;

    default:
        assert(false);
        break;
    }

    SuccessOrExit(error = AppendScanMask(*message, scan_mask));
    SuccessOrExit(error = AppendVersion(*message));

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xff02);
    destination.addr16[7] = HostSwap16(0x0002);
    SuccessOrExit(error = SendMessage(*message, destination));

    switch (m_parent_request_state)
    {
    case kParentRequestRouter:
        dprintf("Sent parent request to routers\n");
        break;

    case kParentRequestChild:
        dprintf("Sent parent request to all devices\n");
        break;

    default:
        assert(false);
        break;
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

ThreadError Mle::SendChildIdRequest()
{
    ThreadError error = kThreadError_None;
    uint8_t tlvs[] = {Tlv::kAddress16, Tlv::kNetworkData, Tlv::kRoute};
    Message *message;
    Ip6Address destination;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildIdRequest));
    SuccessOrExit(error = AppendResponse(*message, m_child_id_request.challenge, m_child_id_request.challenge_length));
    SuccessOrExit(error = AppendLinkFrameCounter(*message));
    SuccessOrExit(error = AppendMleFrameCounter(*message));
    SuccessOrExit(error = AppendMode(*message, m_device_mode));
    SuccessOrExit(error = AppendTimeout(*message, m_timeout));
    SuccessOrExit(error = AppendVersion(*message));

    if ((m_device_mode & kModeFFD) == 0)
    {
        SuccessOrExit(error = AppendIp6Address(*message));
    }

    SuccessOrExit(error = AppendTlvRequest(*message, tlvs, sizeof(tlvs)));

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xfe80);
    memcpy(destination.addr8 + 8, &m_parent.mac_addr, sizeof(m_parent.mac_addr));
    destination.addr8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));
    dprintf("Sent Child ID Request\n");

    if ((m_device_mode & kModeRxOnWhenIdle) == 0)
    {
        m_mesh->SetPollPeriod(100);
        m_mesh->SetRxOnWhenIdle(false);
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendDataRequest(const Ip6Address &destination, const uint8_t *tlvs, uint8_t tlvs_length)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandDataRequest));
    SuccessOrExit(error = AppendTlvRequest(*message, tlvs, tlvs_length));

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Data Request\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendDataResponse(const Ip6Address &destination, const uint8_t *tlvs, uint8_t tlvs_length)
{
    ThreadError error = kThreadError_None;
    Message *message;
    Neighbor *neighbor;
    bool stable_only;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandDataResponse));

    neighbor = m_mle_router->GetNeighbor(destination);

    for (int i = 0; i < tlvs_length; i++)
    {
        switch (tlvs[i])
        {
        case Tlv::kLeaderData:
            SuccessOrExit(error = AppendLeaderData(*message));
            break;

        case Tlv::kNetworkData:
            stable_only = neighbor != NULL ? (neighbor->mode & kModeFullNetworkData) == 0 : false;
            SuccessOrExit(error = AppendNetworkData(*message, stable_only));
            break;
        }
    }

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Data Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendChildUpdateRequest()
{
    ThreadError error = kThreadError_None;
    Ip6Address destination;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildUpdateRequest));
    SuccessOrExit(error = AppendMode(*message, m_device_mode));

    if ((m_device_mode & kModeFFD) == 0)
    {
        SuccessOrExit(error = AppendIp6Address(*message));
    }

    switch (m_device_state)
    {
    case kDeviceStateDetached:
        for (uint8_t i = 0; i < sizeof(m_parent_request.challenge); i++)
        {
            m_parent_request.challenge[i] = Random::Get();
        }

        SuccessOrExit(error = AppendChallenge(*message, m_parent_request.challenge,
                                              sizeof(m_parent_request.challenge)));
        break;

    case kDeviceStateChild:
        SuccessOrExit(error = AppendSourceAddress(*message));
        SuccessOrExit(error = AppendLeaderData(*message));
        SuccessOrExit(error = AppendTimeout(*message, m_timeout));
        break;

    case kDeviceStateDisabled:
    case kDeviceStateRouter:
    case kDeviceStateLeader:
        assert(false);
        break;
    }

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xfe80);
    memcpy(destination.addr8 + 8, &m_parent.mac_addr, sizeof(m_parent.mac_addr));
    destination.addr8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Child Update Request\n");

    if ((m_device_mode & kModeRxOnWhenIdle) == 0)
    {
        m_mesh->SetPollPeriod(100);
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendMessage(Message &message, const Ip6Address &destination)
{
    ThreadError error = kThreadError_None;
    Header header;
    uint32_t key_sequence;
    uint8_t nonce[13];
    uint8_t tag[4];
    uint8_t tag_length;
    Crypto::AesEcb aes_ecb;
    Crypto::AesCcm aes_ccm;
    uint8_t buf[64];
    int length;
    Ip6MessageInfo message_info;

    message.Read(0, sizeof(header), &header);
    header.SetFrameCounter(m_key_manager->GetMleFrameCounter());

    key_sequence = m_key_manager->GetCurrentKeySequence();
    header.SetKeyId(key_sequence);

    message.Write(0, header.GetLength(), &header);

    GenerateNonce(*m_mesh->GetAddress64(), m_key_manager->GetMleFrameCounter(), Mac::Frame::kSecEncMic32, nonce);

    aes_ecb.SetKey(m_key_manager->GetCurrentMleKey(), 16);
    aes_ccm.Init(aes_ecb, 16 + 16 + header.GetHeaderLength(), message.GetLength() - (header.GetLength() - 1),
                 sizeof(tag), nonce, sizeof(nonce));

    aes_ccm.Header(&m_link_local_64.address, sizeof(m_link_local_64.address));
    aes_ccm.Header(&destination, sizeof(destination));
    aes_ccm.Header(header.GetBytes() + 1, header.GetHeaderLength());

    message.SetOffset(header.GetLength() - 1);

    while (message.GetOffset() < message.GetLength())
    {
        length = message.Read(message.GetOffset(), sizeof(buf), buf);
        aes_ccm.Payload(buf, buf, length, true);
        message.Write(message.GetOffset(), length, buf);
        message.MoveOffset(length);
    }

    tag_length = sizeof(tag);
    aes_ccm.Finalize(tag, &tag_length);
    SuccessOrExit(message.Append(tag, tag_length));

    memset(&message_info, 0, sizeof(message_info));
    memcpy(&message_info.peer_addr, &destination, sizeof(message_info.peer_addr));
    memcpy(&message_info.sock_addr, &m_link_local_64.address, sizeof(message_info.sock_addr));
    message_info.peer_port = kUdpPort;
    message_info.interface_id = m_netif->GetInterfaceId();
    message_info.hop_limit = 255;

    m_key_manager->IncrementMleFrameCounter();

    SuccessOrExit(error = m_socket.SendTo(message, message_info));

exit:
    return error;
}

void Mle::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    Mle *obj = reinterpret_cast<Mle *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void Mle::HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info)
{
    Header header;
    uint32_t key_sequence;
    const uint8_t *mle_key;
    uint8_t keyid;
    uint32_t frame_counter;
    uint8_t message_tag[4];
    uint8_t message_tag_length;
    uint8_t nonce[13];
    Mac::Address64 mac_addr;
    Crypto::AesEcb aes_ecb;
    Crypto::AesCcm aes_ccm;
    uint16_t mle_offset;
    uint8_t buf[64];
    int length;
    uint8_t tag[4];
    uint8_t tag_length;
    uint8_t command;
    Neighbor *neighbor;

    message.Read(message.GetOffset(), sizeof(header), &header);
    VerifyOrExit(header.IsValid(),);

    if (header.IsKeyIdMode1())
    {
        keyid = header.GetKeyId();

        if (keyid == (m_key_manager->GetCurrentKeySequence() & 0x7f))
        {
            key_sequence = m_key_manager->GetCurrentKeySequence();
            mle_key = m_key_manager->GetCurrentMleKey();
        }
        else if (m_key_manager->IsPreviousKeyValid() &&
                 keyid == (m_key_manager->GetPreviousKeySequence() & 0x7f))
        {
            key_sequence = m_key_manager->GetPreviousKeySequence();
            mle_key = m_key_manager->GetPreviousMleKey();
        }
        else
        {
            key_sequence = (m_key_manager->GetCurrentKeySequence() & ~0x7f) | keyid;

            if (key_sequence < m_key_manager->GetCurrentKeySequence())
            {
                key_sequence += 128;
            }

            mle_key = m_key_manager->GetTemporaryMleKey(key_sequence);
        }
    }
    else
    {
        key_sequence = header.GetKeyId();

        if (key_sequence == m_key_manager->GetCurrentKeySequence())
        {
            mle_key = m_key_manager->GetCurrentMleKey();
        }
        else if (m_key_manager->IsPreviousKeyValid() &&
                 key_sequence == m_key_manager->GetPreviousKeySequence())
        {
            mle_key = m_key_manager->GetPreviousMleKey();
        }
        else
        {
            mle_key = m_key_manager->GetTemporaryMleKey(key_sequence);
        }
    }

    message.MoveOffset(header.GetLength() - 1);

    frame_counter = header.GetFrameCounter();

    message_tag_length = message.Read(message.GetLength() - sizeof(message_tag), sizeof(message_tag), message_tag);
    VerifyOrExit(message_tag_length == sizeof(message_tag), ;);
    SuccessOrExit(message.SetLength(message.GetLength() - sizeof(message_tag)));

    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;
    GenerateNonce(mac_addr, frame_counter, Mac::Frame::kSecEncMic32, nonce);

    aes_ecb.SetKey(mle_key, 16);
    aes_ccm.Init(aes_ecb, sizeof(message_info.peer_addr) + sizeof(message_info.sock_addr) + header.GetHeaderLength(),
                 message.GetLength() - message.GetOffset(), sizeof(message_tag), nonce, sizeof(nonce));
    aes_ccm.Header(&message_info.peer_addr, sizeof(message_info.peer_addr));
    aes_ccm.Header(&message_info.sock_addr, sizeof(message_info.sock_addr));
    aes_ccm.Header(header.GetBytes() + 1, header.GetHeaderLength());

    mle_offset = message.GetOffset();

    while (message.GetOffset() < message.GetLength())
    {
        length = message.Read(message.GetOffset(), sizeof(buf), buf);
        aes_ccm.Payload(buf, buf, length, false);
        message.Write(message.GetOffset(), length, buf);
        message.MoveOffset(length);
    }

    tag_length = sizeof(tag);
    aes_ccm.Finalize(tag, &tag_length);
    VerifyOrExit(message_tag_length == tag_length && memcmp(message_tag, tag, tag_length) == 0, ;);

    if (key_sequence > m_key_manager->GetCurrentKeySequence())
    {
        m_key_manager->SetCurrentKeySequence(key_sequence);
    }

    message.SetOffset(mle_offset);

    message.Read(message.GetOffset(), sizeof(command), &command);
    message.MoveOffset(sizeof(command));

    switch (m_device_state)
    {
    case kDeviceStateDetached:
    case kDeviceStateChild:
        neighbor = GetNeighbor(mac_addr);
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        if (command == Header::kCommandChildIdResponse)
        {
            neighbor = GetNeighbor(mac_addr);
        }
        else
        {
            neighbor = m_mle_router->GetNeighbor(mac_addr);
        }

        break;

    default:
        neighbor = NULL;
        break;
    }

    if (neighbor != NULL && neighbor->state == Neighbor::kStateValid)
    {
        if (key_sequence == m_key_manager->GetCurrentKeySequence())
            VerifyOrExit(neighbor->previous_key == true || frame_counter >= neighbor->valid.mle_frame_counter,
                         dprintf("mle frame counter reject 1\n"));
        else if (key_sequence == m_key_manager->GetPreviousKeySequence())
            VerifyOrExit(neighbor->previous_key == true && frame_counter >= neighbor->valid.mle_frame_counter,
                         dprintf("mle frame counter reject 2\n"));
        else
        {
            assert(false);
        }

        neighbor->valid.mle_frame_counter = frame_counter + 1;
    }
    else
    {
        VerifyOrExit(command == Header::kCommandLinkRequest ||
                     command == Header::kCommandLinkAccept ||
                     command == Header::kCommandLinkAcceptAndRequest ||
                     command == Header::kCommandAdvertisement ||
                     command == Header::kCommandParentRequest ||
                     command == Header::kCommandParentResponse ||
                     command == Header::kCommandChildIdRequest ||
                     command == Header::kCommandChildUpdateRequest, dprintf("mle sequence unknown! %d\n", command));
    }

    switch (command)
    {
    case Header::kCommandLinkRequest:
        m_mle_router->HandleLinkRequest(message, message_info);
        break;

    case Header::kCommandLinkAccept:
        m_mle_router->HandleLinkAccept(message, message_info, key_sequence);
        break;

    case Header::kCommandLinkAcceptAndRequest:
        m_mle_router->HandleLinkAcceptAndRequest(message, message_info, key_sequence);
        break;

    case Header::kCommandLinkReject:
        m_mle_router->HandleLinkReject(message, message_info);
        break;

    case Header::kCommandAdvertisement:
        HandleAdvertisement(message, message_info);
        break;

    case Header::kCommandDataRequest:
        HandleDataRequest(message, message_info);
        break;

    case Header::kCommandDataResponse:
        HandleDataResponse(message, message_info);
        break;

    case Header::kCommandParentRequest:
        m_mle_router->HandleParentRequest(message, message_info);
        break;

    case Header::kCommandParentResponse:
        HandleParentResponse(message, message_info, key_sequence);
        break;

    case Header::kCommandChildIdRequest:
        m_mle_router->HandleChildIdRequest(message, message_info, key_sequence);
        break;

    case Header::kCommandChildIdResponse:
        HandleChildIdResponse(message, message_info);
        break;

    case Header::kCommandChildUpdateRequest:
        m_mle_router->HandleChildUpdateRequest(message, message_info);
        break;

    case Header::kCommandChildUpdateResponse:
        HandleChildUpdateResponse(message, message_info);
        break;
    }

exit:
    {}
}

ThreadError Mle::HandleAdvertisement(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Mac::Address64 mac_addr;
    bool is_neighbor;
    Neighbor *neighbor;
    LeaderDataTlv leader_data;
    uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};

    if (m_device_state != kDeviceStateDetached)
    {
        SuccessOrExit(error = m_mle_router->HandleAdvertisement(message, message_info));
    }

    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    is_neighbor = false;

    switch (m_device_state)
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        break;

    case kDeviceStateChild:
        if (memcmp(&m_parent.mac_addr, &mac_addr, sizeof(m_parent.mac_addr)))
        {
            break;
        }

        is_neighbor = true;
        m_parent.last_heard = m_parent_request_timer.GetNow();
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        if ((neighbor = m_mle_router->GetNeighbor(mac_addr)) != NULL &&
            neighbor->state == Neighbor::kStateValid)
        {
            is_neighbor = true;
        }

        break;
    }

    if (is_neighbor)
    {
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
        VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);

        if (static_cast<int8_t>(leader_data.GetDataVersion() - m_network_data->GetVersion()) > 0)
        {
            SendDataRequest(message_info.peer_addr, tlvs, sizeof(tlvs));
        }
    }

exit:
    return error;
}

ThreadError Mle::HandleDataRequest(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    TlvRequestTlv tlv_request;

    dprintf("Received Data Request\n");

    // TLV Request
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlv_request), tlv_request));
    VerifyOrExit(tlv_request.IsValid(), error = kThreadError_Parse);

    SendDataResponse(message_info.peer_addr, tlv_request.GetTlvs(), tlv_request.GetLength());

exit:
    return error;
}

ThreadError Mle::HandleDataResponse(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    LeaderDataTlv leader_data;
    NetworkDataTlv network_data;
    int8_t diff;

    dprintf("Received Data Response\n");

    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kNetworkData, sizeof(network_data), network_data));
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
    VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);

    diff = leader_data.GetDataVersion() - m_network_data->GetVersion();
    VerifyOrExit(diff > 0, ;);

    SuccessOrExit(error = m_network_data->SetNetworkData(leader_data.GetDataVersion(),
                                                         leader_data.GetStableDataVersion(),
                                                         (m_device_mode & kModeFullNetworkData) == 0,
                                                         network_data.GetNetworkData(), network_data.GetLength()));

exit:
    return error;
}

uint8_t Mle::LinkMarginToQuality(uint8_t link_margin)
{
    if (link_margin > 20)
    {
        return 3;
    }
    else if (link_margin > 10)
    {
        return 2;
    }
    else if (link_margin > 2)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

ThreadError Mle::HandleParentResponse(const Message &message, const Ip6MessageInfo &message_info,
                                      uint32_t key_sequence)
{
    ThreadError error = kThreadError_None;
    ResponseTlv response;
    SourceAddressTlv source_address;
    LeaderDataTlv leader_data;
    uint32_t peer_partition_id;
    LinkMarginTlv link_margin_tlv;
    uint8_t link_margin;
    uint8_t link_quality;
    ConnectivityTlv connectivity;
    uint32_t connectivity_metric;
    LinkFrameCounterTlv link_frame_counter;
    MleFrameCounterTlv mle_frame_counter;
    ChallengeTlv challenge;
    int8_t diff;

    dprintf("Received Parent Response\n");

    // Response
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
    VerifyOrExit(response.IsValid() &&
                 memcmp(response.GetResponse(), m_parent_request.challenge, response.GetLength()) == 0,
                 error = kThreadError_Parse);

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(source_address), source_address));
    VerifyOrExit(source_address.IsValid(), error = kThreadError_Parse);

    // Leader Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
    VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);

    // Weight
    VerifyOrExit(leader_data.GetWeighting() >= m_mle_router->GetLeaderWeight(), ;);

    // Partition ID
    peer_partition_id = leader_data.GetPartitionId();

    if (m_device_state != kDeviceStateDetached)
    {
        switch (m_parent_request_mode)
        {
        case kJoinAnyPartition:
            break;

        case kJoinSamePartition:
            if (peer_partition_id != m_leader_data.GetPartitionId())
            {
                ExitNow();
            }

            break;

        case kJoinBetterPartition:
            dprintf("partition info  %d %d %d %d\n",
                    leader_data.GetWeighting(), peer_partition_id,
                    m_leader_data.GetWeighting(), m_leader_data.GetPartitionId());

            if ((leader_data.GetWeighting() < m_leader_data.GetWeighting()) ||
                (leader_data.GetWeighting() == m_leader_data.GetWeighting() &&
                 peer_partition_id <= m_leader_data.GetPartitionId()))
            {
                ExitNow(dprintf("ignore parent response\n"));
            }

            break;
        }
    }

    // Link Quality
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkMargin, sizeof(link_margin_tlv), link_margin_tlv));
    VerifyOrExit(link_margin_tlv.IsValid(), error = kThreadError_Parse);

    link_margin = reinterpret_cast<const ThreadMessageInfo *>(message_info.link_info)->link_margin;

    if (link_margin > link_margin_tlv.GetLinkMargin())
    {
        link_margin = link_margin_tlv.GetLinkMargin();
    }

    link_quality = LinkMarginToQuality(link_margin);

    VerifyOrExit(m_parent_request_state != kParentRequestRouter || link_quality == 3, ;);

    // Connectivity
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kConnectivity, sizeof(connectivity), connectivity));
    VerifyOrExit(connectivity.IsValid(), error = kThreadError_Parse);

    if (peer_partition_id == m_leader_data.GetPartitionId())
    {
        diff = connectivity.GetRouterIdSequence() - m_mle_router->GetRouterIdSequence();
        VerifyOrExit(diff > 0 || (diff == 0 && m_mle_router->GetLeaderAge() < m_mle_router->GetNetworkIdTimeout()), ;);
    }

    connectivity_metric =
        (static_cast<uint32_t>(link_quality) << 24) |
        (static_cast<uint32_t>(connectivity.GetLinkQuality3()) << 16) |
        (static_cast<uint32_t>(connectivity.GetLinkQuality2()) << 8) |
        (static_cast<uint32_t>(connectivity.GetLinkQuality1()));

    if (m_parent.state == Neighbor::kStateValid)
    {
        VerifyOrExit(connectivity_metric > m_parent_connectivity, ;);
    }

    // Link Frame Counter
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkFrameCounter, sizeof(link_frame_counter),
                                      link_frame_counter));
    VerifyOrExit(link_frame_counter.IsValid(), error = kThreadError_Parse);

    // Mle Frame Counter
    if (Tlv::GetTlv(message, Tlv::kMleFrameCounter, sizeof(mle_frame_counter), mle_frame_counter) ==
        kThreadError_None)
    {
        VerifyOrExit(mle_frame_counter.IsValid(), ;);
    }
    else
    {
        mle_frame_counter.SetFrameCounter(link_frame_counter.GetFrameCounter());
    }

    // Challenge
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
    VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);
    memcpy(m_child_id_request.challenge, challenge.GetChallenge(), challenge.GetLength());
    m_child_id_request.challenge_length = challenge.GetLength();

    memcpy(&m_parent.mac_addr, message_info.peer_addr.addr8 + 8, sizeof(m_parent.mac_addr));
    m_parent.mac_addr.bytes[0] ^= 0x2;
    m_parent.valid.rloc16 = source_address.GetRloc16();
    m_parent.valid.link_frame_counter = link_frame_counter.GetFrameCounter();
    m_parent.valid.mle_frame_counter = mle_frame_counter.GetFrameCounter();
    m_parent.mode = kModeFFD | kModeRxOnWhenIdle | kModeFullNetworkData;
    m_parent.state = Neighbor::kStateValid;
    assert(key_sequence == m_key_manager->GetCurrentKeySequence() ||
           key_sequence == m_key_manager->GetPreviousKeySequence());
    m_parent.previous_key = key_sequence == m_key_manager->GetPreviousKeySequence();
    m_parent_connectivity = connectivity_metric;

exit:
    return error;
}

ThreadError Mle::HandleChildIdResponse(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    LeaderDataTlv leader_data;
    SourceAddressTlv source_address;
    Address16Tlv address16;
    NetworkDataTlv network_data;
    RouteTlv route;
    uint8_t num_routers;

    dprintf("Received Child ID Response\n");

    VerifyOrExit(m_parent_request_state == kChildIdRequest, ;);

    // Leader Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
    VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(source_address), source_address));
    VerifyOrExit(source_address.IsValid(), error = kThreadError_Parse);

    // Address16
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kAddress16, sizeof(address16), address16));
    VerifyOrExit(address16.IsValid(), error = kThreadError_Parse);

    // Network Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kNetworkData, sizeof(network_data), network_data));
    SuccessOrExit(error = m_network_data->SetNetworkData(leader_data.GetDataVersion(),
                                                         leader_data.GetStableDataVersion(),
                                                         (m_device_mode & kModeFullNetworkData) == 0,
                                                         network_data.GetNetworkData(), network_data.GetLength()));

    // Parent Attach Success
    m_parent_request_timer.Stop();

    m_leader_data.SetPartitionId(leader_data.GetPartitionId());
    m_leader_data.SetWeighting(leader_data.GetWeighting());
    m_leader_data.SetRouterId(leader_data.GetRouterId());

    if ((m_device_mode & kModeRxOnWhenIdle) == 0)
    {
        m_mesh->SetPollPeriod((m_timeout / 2) * 1000U);
        m_mesh->SetRxOnWhenIdle(false);
    }
    else
    {
        m_mesh->SetRxOnWhenIdle(true);
    }

    m_parent.valid.rloc16 = source_address.GetRloc16();
    SuccessOrExit(error = SetStateChild(address16.GetRloc16()));

    // Route
    if (Tlv::GetTlv(message, Tlv::kRoute, sizeof(route), route) == kThreadError_None)
    {
        num_routers = 0;

        for (int i = 0; i < kMaxRouterId; i++)
        {
            num_routers += route.IsRouterIdSet(i);
        }

        if (num_routers < m_mle_router->GetRouterUpgradeThreshold())
        {
            m_mle_router->BecomeRouter();
        }
    }

exit:
    return error;
}

ThreadError Mle::HandleChildUpdateResponse(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    StatusTlv status;
    ModeTlv mode;
    ResponseTlv response;
    LeaderDataTlv leader_data;
    SourceAddressTlv source_address;
    TimeoutTlv timeout;
    uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};

    dprintf("Received Child Update Response\n");

    // Status
    if (Tlv::GetTlv(message, Tlv::kStatus, sizeof(status), status) == kThreadError_None)
    {
        BecomeDetached();
        ExitNow();
    }

    // Mode
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kMode, sizeof(mode), mode));
    VerifyOrExit(mode.IsValid(), error = kThreadError_Parse);
    VerifyOrExit(mode.GetMode() == m_device_mode, error = kThreadError_Drop);

    switch (m_device_state)
    {
    case kDeviceStateDetached:
        // Response
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
        VerifyOrExit(response.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(memcmp(response.GetResponse(), m_parent_request.challenge,
                            sizeof(m_parent_request.challenge)) == 0,
                     error = kThreadError_Drop);

        SetStateChild(GetRloc16());
        break;

    case kDeviceStateChild:
        // Leader Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
        VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);

        if (static_cast<int8_t>(leader_data.GetDataVersion() - m_network_data->GetVersion()) > 0)
        {
            SendDataRequest(message_info.peer_addr, tlvs, sizeof(tlvs));
        }

        // Source Address
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(source_address), source_address));
        VerifyOrExit(source_address.IsValid(), error = kThreadError_Parse);

        if (GetRouterId(source_address.GetRloc16()) != GetRouterId(GetRloc16()))
        {
            BecomeDetached();
            ExitNow();
        }

        // Timeout
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTimeout, sizeof(timeout), timeout));
        VerifyOrExit(timeout.IsValid(), error = kThreadError_Parse);

        m_timeout = timeout.GetTimeout();

        if ((mode.GetMode() & kModeRxOnWhenIdle) == 0)
        {
            m_mesh->SetPollPeriod((m_timeout / 2) * 1000U);
            m_mesh->SetRxOnWhenIdle(false);
        }
        else
        {
            m_mesh->SetRxOnWhenIdle(true);
        }

        break;

    default:
        assert(false);
        break;
    }

exit:
    return error;
}

Neighbor *Mle::GetNeighbor(uint16_t address)
{
    return (m_parent.state == Neighbor::kStateValid && m_parent.valid.rloc16 == address) ? &m_parent : NULL;
}

Neighbor *Mle::GetNeighbor(const Mac::Address64 &address)
{
    return (m_parent.state == Neighbor::kStateValid &&
            memcmp(&m_parent.mac_addr, &address, sizeof(m_parent.mac_addr)) == 0) ? &m_parent : NULL;
}

Neighbor *Mle::GetNeighbor(const Mac::Address &address)
{
    Neighbor *neighbor = NULL;

    switch (address.length)
    {
    case 2:
        neighbor = GetNeighbor(address.address16);
        break;

    case 8:
        neighbor = GetNeighbor(address.address64);
        break;
    }

    return neighbor;
}

Neighbor *Mle::GetNeighbor(const Ip6Address &address)
{
    return NULL;
}

uint16_t Mle::GetNextHop(uint16_t destination) const
{
    return (m_parent.state == Neighbor::kStateValid) ? m_parent.valid.rloc16 : Mac::kShortAddrInvalid;
}

bool Mle::IsRoutingLocator(const Ip6Address &address) const
{
    return memcmp(&m_mesh_local_16, &address, 14) == 0;
}

Router *Mle::GetParent()
{
    return &m_parent;
}

ThreadError Mle::CheckReachability(Mac::Address16 meshsrc, Mac::Address16 meshdst, Ip6Header &ip6_header)
{
    ThreadError error = kThreadError_Drop;
    Ip6Address dst;

    if (meshdst != GetRloc16())
    {
        ExitNow(error = kThreadError_None);
    }

    if (m_netif->IsUnicastAddress(*ip6_header.GetDestination()))
    {
        ExitNow(error = kThreadError_None);
    }

    memcpy(&dst, GetMeshLocal16(), 14);
    dst.addr16[7] = HostSwap16(meshsrc);
    Icmp6::SendError(dst, Icmp6Header::kTypeDstUnreach, Icmp6Header::kCodeDstUnreachNoRoute, ip6_header);

exit:
    return error;
}

ThreadError Mle::HandleNetworkDataUpdate()
{
    if (m_device_mode & kModeFFD)
    {
        m_mle_router->HandleNetworkDataUpdateRouter();
    }

    switch (m_device_state)
    {
    case kDeviceStateChild:
        SendChildUpdateRequest();
        break;

    default:
        break;
    }

    return kThreadError_None;
}

}  // namespace Mle
}  // namespace Thread
