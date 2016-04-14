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

#include <assert.h>

#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/random.hpp>
#include <mac/mac_frame.hpp>
#include <net/icmp6.hpp>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>
#include <thread/thread_tlvs.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Mle {

#define LQI_TO_COST(x) (kLqiToCost[(x)])

static const uint8_t kLqiToCost[] =
{
    16, 6, 2, 1,
};

MleRouter::MleRouter(ThreadNetif &netif):
    Mle(netif),
    m_advertise_timer(&HandleAdvertiseTimer, this),
    m_state_update_timer(&HandleStateUpdateTimer, this),
    m_socket(&HandleUdpReceive, this),
    m_address_solicit("a/as", &HandleAddressSolicit, this),
    m_address_release("a/ar", &HandleAddressRelease, this)
{
    m_next_child_id = 1;
    m_router_id_sequence = 0;
    memset(m_children, 0, sizeof(m_children));
    memset(m_routers, 0, sizeof(m_routers));
    m_coap_server = netif.GetCoapServer();
    m_coap_message_id = Random::Get();
}

int MleRouter::AllocateRouterId()
{
    int rval = -1;

    // count available router ids
    uint8_t num_available = 0;
    uint8_t num_allocated = 0;

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].allocated)
        {
            num_allocated++;
        }
        else if (m_routers[i].reclaim_delay == false)
        {
            num_available++;
        }
    }

    VerifyOrExit(num_allocated < kMaxRouters && num_available > 0, rval = -1);

    // choose available router id at random
    uint8_t free_bit;
    // free_bit = Random::Get() % num_available;
    free_bit = 0;

    // allocate router id
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].allocated || m_routers[i].reclaim_delay)
        {
            continue;
        }

        if (free_bit == 0)
        {
            rval = AllocateRouterId(i);
            ExitNow();
        }

        free_bit--;
    }

exit:
    return rval;
}

int MleRouter::AllocateRouterId(uint8_t router_id)
{
    int rval = -1;

    VerifyOrExit(!m_routers[router_id].allocated, rval = -1);

    // init router state
    m_routers[router_id].allocated = true;
    m_routers[router_id].last_heard = Timer::GetNow();
    memset(&m_routers[router_id].mac_addr, 0, sizeof(m_routers[router_id].mac_addr));

    // bump sequence number
    m_router_id_sequence++;
    m_routers[m_router_id].last_heard = Timer::GetNow();
    rval = router_id;

    dprintf("add router id %d\n", router_id);

exit:
    return rval;
}

ThreadError MleRouter::ReleaseRouterId(uint8_t router_id)
{
    dprintf("delete router id %d\n", router_id);
    m_routers[router_id].allocated = false;
    m_routers[router_id].reclaim_delay = true;
    m_routers[router_id].state = Neighbor::kStateInvalid;
    m_routers[router_id].nexthop = kMaxRouterId;
    m_router_id_sequence++;
    m_address_resolver->Remove(router_id);
    m_network_data->RemoveBorderRouter(GetRloc16(router_id));
    ResetAdvertiseInterval();
    return kThreadError_None;
}

uint32_t MleRouter::GetLeaderAge() const
{
    return (Timer::GetNow() - m_routers[GetLeaderId()].last_heard) / 1000;
}

ThreadError MleRouter::BecomeRouter()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_device_state == kDeviceStateDetached || m_device_state == kDeviceStateChild,
                 error = kThreadError_Busy);
    VerifyOrExit(m_device_mode & kModeFFD, ;);

    for (int i = 0; i < kMaxRouterId; i++)
    {
        m_routers[i].allocated = false;
        m_routers[i].reclaim_delay = false;
        m_routers[i].state = Neighbor::kStateInvalid;
        m_routers[i].nexthop = kMaxRouterId;
    }

    m_advertise_timer.Stop();
    m_address_resolver->Clear();

    switch (m_device_state)
    {
    case kDeviceStateDetached:
        SuccessOrExit(error = SendLinkRequest(NULL));
        m_state_update_timer.Start(1000);
        break;

    case kDeviceStateChild:
        SuccessOrExit(error = SendAddressSolicit());
        break;

    default:
        assert(false);
        break;
    }

exit:
    return error;
}

ThreadError MleRouter::BecomeLeader()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_device_state != kDeviceStateDisabled && m_device_state != kDeviceStateLeader,
                 error = kThreadError_Busy);

    for (int i = 0; i < kMaxRouterId; i++)
    {
        m_routers[i].allocated = false;
        m_routers[i].reclaim_delay = false;
        m_routers[i].state = Neighbor::kStateInvalid;
        m_routers[i].nexthop = kMaxRouterId;
    }

    m_advertise_timer.Stop();
    ResetAdvertiseInterval();
    m_state_update_timer.Start(1000);
    m_address_resolver->Clear();

    m_router_id = (m_previous_router_id != kMaxRouterId) ? AllocateRouterId(m_previous_router_id) : AllocateRouterId();
    VerifyOrExit(m_router_id >= 0, error = kThreadError_NoBufs);

    memcpy(&m_routers[m_router_id].mac_addr, m_mesh->GetAddress64(), sizeof(m_routers[m_router_id].mac_addr));

    m_leader_data.SetPartitionId(Random::Get());
    m_leader_data.SetWeighting(m_leader_weight);
    m_leader_data.SetRouterId(m_router_id);

    m_network_data->Init();

    SuccessOrExit(error = SetStateLeader(m_router_id << 10));

exit:
    return error;
}

ThreadError MleRouter::HandleDetachStart()
{
    ThreadError error = kThreadError_None;

    for (int i = 0; i < kMaxRouterId; i++)
    {
        m_routers[i].state = Neighbor::kStateInvalid;
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        m_children[i].state = Neighbor::kStateInvalid;
    }

    m_advertise_timer.Stop();
    m_state_update_timer.Stop();
    m_network_data->Stop();
    m_netif->UnsubscribeAllRoutersMulticast();

    return error;
}

ThreadError MleRouter::HandleChildStart(JoinMode mode)
{
    uint32_t advertise_delay;

    m_routers[GetLeaderId()].last_heard = Timer::GetNow();

    m_advertise_timer.Stop();
    m_state_update_timer.Start(1000);
    m_network_data->Stop();

    switch (mode)
    {
    case kJoinAnyPartition:
        break;

    case kJoinSamePartition:
        SendAddressRelease();
        break;

    case kJoinBetterPartition:
        // BecomeRouter();
        break;
    }

    if (m_device_mode & kModeFFD)
    {
        advertise_delay = (kReedAdvertiseInterval + (Random::Get() % kReedAdvertiseJitter)) * 1000U;
        m_advertise_timer.Start(advertise_delay);
        m_netif->SubscribeAllRoutersMulticast();
    }
    else
    {
        m_netif->UnsubscribeAllRoutersMulticast();
    }

    return kThreadError_None;
}

ThreadError MleRouter::SetStateRouter(uint16_t rloc16)
{
    SetRloc16(rloc16);
    m_device_state = kDeviceStateRouter;
    m_parent_request_state = kParentIdle;
    m_parent_request_timer.Stop();

    m_netif->SubscribeAllRoutersMulticast();
    m_routers[m_router_id].nexthop = m_router_id;
    m_routers[GetLeaderId()].last_heard = Timer::GetNow();
    m_network_data->Stop();
    m_state_update_timer.Start(1000);

    dprintf("Mode -> Router\n");
    return kThreadError_None;
}

ThreadError MleRouter::SetStateLeader(uint16_t rloc16)
{
    SetRloc16(rloc16);
    m_device_state = kDeviceStateLeader;
    m_parent_request_state = kParentIdle;
    m_parent_request_timer.Stop();

    m_netif->SubscribeAllRoutersMulticast();
    m_routers[m_router_id].nexthop = m_router_id;
    m_routers[m_router_id].last_heard = Timer::GetNow();

    m_network_data->Start();
    m_coap_server->AddResource(m_address_solicit);
    m_coap_server->AddResource(m_address_release);

    dprintf("Mode -> Leader %d\n", m_leader_data.GetPartitionId());
    return kThreadError_None;
}

uint8_t MleRouter::GetNetworkIdTimeout() const
{
    return m_network_id_timeout;
}

ThreadError MleRouter::SetNetworkIdTimeout(uint8_t timeout)
{
    m_network_id_timeout = timeout;
    return kThreadError_None;
}

uint8_t MleRouter::GetRouterUpgradeThreshold() const
{
    return m_router_upgrade_threshold;
}

ThreadError MleRouter::SetRouterUpgradeThreshold(uint8_t threshold)
{
    m_router_upgrade_threshold = threshold;
    return kThreadError_None;
}

void MleRouter::HandleAdvertiseTimer(void *context)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleAdvertiseTimer();
}

void MleRouter::HandleAdvertiseTimer()
{
    uint32_t advertise_delay;

    if ((m_device_mode & kModeFFD) == 0)
    {
        return;
    }

    SendAdvertisement();

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        advertise_delay = (kReedAdvertiseInterval + (Random::Get() % kReedAdvertiseJitter)) * 1000U;
        m_advertise_timer.Start(advertise_delay);
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        m_advertise_interval *= 2;

        if (m_advertise_interval > kAdvertiseIntervalMax)
        {
            m_advertise_interval = kAdvertiseIntervalMax;
        }

        advertise_delay = (m_advertise_interval * 1000U) / 2;
        advertise_delay += Random::Get() % (advertise_delay);
        m_advertise_timer.Start(advertise_delay);
        break;
    }
}

ThreadError MleRouter::ResetAdvertiseInterval()
{
    uint32_t advertise_delay;

    VerifyOrExit(m_advertise_interval != kAdvertiseIntervalMin || !m_advertise_timer.IsRunning(), ;);

    m_advertise_interval = kAdvertiseIntervalMin;

    advertise_delay = (m_advertise_interval * 1000U) / 2;
    advertise_delay += Random::Get() % advertise_delay;
    m_advertise_timer.Start(advertise_delay);

    dprintf("reset advertise interval\n");

exit:
    return kThreadError_None;
}

ThreadError MleRouter::SendAdvertisement()
{
    ThreadError error = kThreadError_None;
    Ip6Address destination;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandAdvertisement));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendLeaderData(*message));

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        SuccessOrExit(error = AppendRoute(*message));
        break;
    }

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xff02);
    destination.addr16[7] = HostSwap16(0x0001);
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent advertisement\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::SendLinkRequest(Neighbor *neighbor)
{
    static const uint8_t detached_tlvs[] = {Tlv::kNetworkData, Tlv::kAddress16, Tlv::kRoute};
    static const uint8_t router_tlvs[] = {Tlv::kLinkMargin};
    ThreadError error = kThreadError_None;
    Message *message;
    Ip6Address destination;

    memset(&destination, 0, sizeof(destination));

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandLinkRequest));
    SuccessOrExit(error = AppendVersion(*message));

    switch (m_device_state)
    {
    case kDeviceStateDisabled:
        assert(false);
        break;

    case kDeviceStateDetached:
        SuccessOrExit(error = AppendTlvRequest(*message, detached_tlvs, sizeof(detached_tlvs)));
        break;

    case kDeviceStateChild:
        SuccessOrExit(error = AppendSourceAddress(*message));
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        SuccessOrExit(error = AppendTlvRequest(*message, router_tlvs, sizeof(router_tlvs)));
        SuccessOrExit(error = AppendSourceAddress(*message));
        SuccessOrExit(error = AppendLeaderData(*message));
        break;
    }

    if (neighbor == NULL)
    {
        for (uint8_t i = 0; i < sizeof(m_challenge); i++)
        {
            m_challenge[i] = Random::Get();
        }

        SuccessOrExit(error = AppendChallenge(*message, m_challenge, sizeof(m_challenge)));
        destination.addr8[0] = 0xff;
        destination.addr8[1] = 0x02;
        destination.addr8[15] = 2;
    }
    else
    {
        for (uint8_t i = 0; i < sizeof(neighbor->pending.challenge); i++)
        {
            neighbor->pending.challenge[i] = Random::Get();
        }

        SuccessOrExit(error = AppendChallenge(*message, m_challenge, sizeof(m_challenge)));
        destination.addr16[0] = HostSwap16(0xfe80);
        memcpy(destination.addr8 + 8, &neighbor->mac_addr, sizeof(neighbor->mac_addr));
        destination.addr8[8] ^= 0x2;
    }

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent link request\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::HandleLinkRequest(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Neighbor *neighbor = NULL;
    Mac::Address64 mac_addr;
    ChallengeTlv challenge;
    VersionTlv version;
    LeaderDataTlv leader_data;
    SourceAddressTlv source_address;
    TlvRequestTlv tlv_request;
    uint16_t rloc16;

    dprintf("Received link request\n");

    VerifyOrExit(GetDeviceState() == kDeviceStateRouter ||
                 GetDeviceState() == kDeviceStateLeader, ;);

    VerifyOrExit(m_parent_request_state == kParentIdle, ;);

    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    // Challenge
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
    VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);

    // Version
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kVersion, sizeof(version), version));
    VerifyOrExit(version.IsValid() && version.GetVersion() == kVersion, error = kThreadError_Parse);

    // Leader Data
    if (Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data) == kThreadError_None)
    {
        VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(leader_data.GetPartitionId() == m_leader_data.GetPartitionId(), ;);
    }

    // Source Address
    if (Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(source_address), source_address) == kThreadError_None)
    {
        VerifyOrExit(source_address.IsValid(), error = kThreadError_Parse);

        rloc16 = source_address.GetRloc16();

        if ((neighbor = GetNeighbor(mac_addr)) != NULL && neighbor->valid.rloc16 != rloc16)
        {
            // remove stale neighbors
            neighbor->state = Neighbor::kStateInvalid;
            neighbor = NULL;
        }

        if (GetChildId(rloc16) == 0)
        {
            // source is a router
            neighbor = &m_routers[GetRouterId(rloc16)];

            if (neighbor->state != Neighbor::kStateValid)
            {
                memcpy(&neighbor->mac_addr, &mac_addr, sizeof(neighbor->mac_addr));
                neighbor->state = Neighbor::kStateLinkRequest;
            }
            else
            {
                VerifyOrExit(memcmp(&neighbor->mac_addr, &mac_addr, sizeof(neighbor->mac_addr)) == 0, ;);
            }
        }
    }
    else
    {
        // lack of source address indicates router coming out of reset
        VerifyOrExit((neighbor = GetNeighbor(mac_addr)) != NULL, error = kThreadError_Drop);
    }

    // TLV Request
    if (Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlv_request), tlv_request) == kThreadError_None)
    {
        VerifyOrExit(tlv_request.IsValid(), error = kThreadError_Parse);
    }
    else
    {
        tlv_request.SetLength(0);
    }

    SuccessOrExit(error = SendLinkAccept(message_info, neighbor, tlv_request, challenge));

exit:
    return error;
}

ThreadError MleRouter::SendLinkAccept(const Ip6MessageInfo &message_info, Neighbor *neighbor,
                                      const TlvRequestTlv &tlv_request, const ChallengeTlv &challenge)
{
    ThreadError error = kThreadError_None;
    Message *message;
    Header::Command command;

    command = (neighbor == NULL || neighbor->state == Neighbor::kStateValid) ?
              Header::kCommandLinkAccept : Header::kCommandLinkAcceptAndRequest;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, command));
    SuccessOrExit(error = AppendVersion(*message));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendResponse(*message, challenge.GetChallenge(), challenge.GetLength()));
    SuccessOrExit(error = AppendLinkFrameCounter(*message));
    SuccessOrExit(error = AppendMleFrameCounter(*message));

    if (neighbor != NULL && GetChildId(neighbor->valid.rloc16) == 0)
    {
        SuccessOrExit(error = AppendLeaderData(*message));
    }

    for (uint8_t i = 0; i < tlv_request.GetLength(); i++)
    {
        switch (tlv_request.GetTlvs()[i])
        {
        case Tlv::kRoute:
            SuccessOrExit(error = AppendRoute(*message));
            break;

        case Tlv::kAddress16:
            VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
            SuccessOrExit(error = AppendAddress16(*message, neighbor->valid.rloc16));
            break;

        case Tlv::kNetworkData:
            VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
            SuccessOrExit(error = AppendNetworkData(*message, (neighbor->mode & kModeFullNetworkData) == 0));
            break;

        case Tlv::kLinkMargin:
            VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
            SuccessOrExit(error = AppendLinkMargin(*message, neighbor->rssi));
            break;

        default:
            ExitNow(error = kThreadError_Drop);
        }
    }

    if (neighbor != NULL && neighbor->state != Neighbor::kStateValid)
    {
        for (uint8_t i = 0; i < sizeof(neighbor->pending.challenge); i++)
        {
            neighbor->pending.challenge[i] = Random::Get();
        }

        SuccessOrExit(error = AppendChallenge(*message, neighbor->pending.challenge,
                                              sizeof(neighbor->pending.challenge)));
        neighbor->state = Neighbor::kStateLinkRequest;
    }

    SuccessOrExit(error = SendMessage(*message, message_info.peer_addr));

    dprintf("Sent link accept\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::HandleLinkAccept(const Message &message, const Ip6MessageInfo &message_info,
                                        uint32_t key_sequence)
{
    dprintf("Received link accept\n");
    return HandleLinkAccept(message, message_info, key_sequence, false);
}

ThreadError MleRouter::HandleLinkAcceptAndRequest(const Message &message, const Ip6MessageInfo &message_info,
                                                  uint32_t key_sequence)
{
    dprintf("Received link accept and request\n");
    return HandleLinkAccept(message, message_info, key_sequence, true);
}

ThreadError MleRouter::HandleLinkAccept(const Message &message, const Ip6MessageInfo &message_info,
                                        uint32_t key_sequence, bool request)
{
    ThreadError error = kThreadError_None;
    Neighbor *neighbor = NULL;
    Mac::Address64 mac_addr;
    VersionTlv version;
    ResponseTlv response;
    SourceAddressTlv source_address;
    LinkFrameCounterTlv link_frame_counter;
    MleFrameCounterTlv mle_frame_counter;
    uint8_t router_id;
    Address16Tlv address16;
    RouteTlv route;
    LeaderDataTlv leader_data;
    NetworkDataTlv network_data;
    LinkMarginTlv link_margin;
    ChallengeTlv challenge;
    TlvRequestTlv tlv_request;

    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    // Version
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kVersion, sizeof(version), version));
    VerifyOrExit(version.IsValid(), error = kThreadError_Parse);

    // Response
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
    VerifyOrExit(response.IsValid(), error = kThreadError_Parse);

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(source_address), source_address));
    VerifyOrExit(source_address.IsValid(), error = kThreadError_Parse);

    // Remove stale neighbors
    if ((neighbor = GetNeighbor(mac_addr)) != NULL &&
        neighbor->valid.rloc16 != source_address.GetRloc16())
    {
        neighbor->state = Neighbor::kStateInvalid;
        neighbor = NULL;
    }

    // Link-Layer Frame Counter
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkFrameCounter, sizeof(link_frame_counter),
                                      link_frame_counter));
    VerifyOrExit(link_frame_counter.IsValid(), error = kThreadError_Parse);

    // MLE Frame Counter
    if (Tlv::GetTlv(message, Tlv::kMleFrameCounter, sizeof(mle_frame_counter), mle_frame_counter) ==
        kThreadError_None)
    {
        VerifyOrExit(mle_frame_counter.IsValid(), error = kThreadError_Parse);
    }
    else
    {
        mle_frame_counter.SetFrameCounter(link_frame_counter.GetFrameCounter());
    }

    router_id = GetRouterId(source_address.GetRloc16());

    if (router_id != m_router_id)
    {
        neighbor = &m_routers[router_id];
    }
    else
    {
        VerifyOrExit((neighbor = FindChild(mac_addr)) != NULL, error = kThreadError_Error);
    }

    // verify response
    VerifyOrExit(memcmp(m_challenge, response.GetResponse(), sizeof(m_challenge)) == 0 ||
                 memcmp(neighbor->pending.challenge, response.GetResponse(), sizeof(neighbor->pending.challenge)) == 0,
                 error = kThreadError_Error);

    switch (m_device_state)
    {
    case kDeviceStateDisabled:
        assert(false);
        break;

    case kDeviceStateDetached:
        // Address16
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kAddress16, sizeof(address16), address16));
        VerifyOrExit(address16.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(GetRloc16() == address16.GetRloc16(), error = kThreadError_Drop);

        // Route
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kRoute, sizeof(route), route));
        VerifyOrExit(route.IsValid(), error = kThreadError_Parse);
        SuccessOrExit(error = ProcessRouteTlv(route));

        // Leader Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
        VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);
        m_leader_data.SetPartitionId(leader_data.GetPartitionId());
        m_leader_data.SetWeighting(leader_data.GetWeighting());
        m_leader_data.SetRouterId(leader_data.GetRouterId());

        // Network Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kNetworkData, sizeof(network_data), network_data));
        SuccessOrExit(error = m_network_data->SetNetworkData(leader_data.GetDataVersion(),
                                                             leader_data.GetStableDataVersion(),
                                                             (m_device_mode & kModeFullNetworkData) == 0,
                                                             network_data.GetNetworkData(), network_data.GetLength()));

        if (m_leader_data.GetRouterId() == GetRouterId(GetRloc16()))
        {
            SetStateLeader(GetRloc16());
        }
        else
        {
            SetStateRouter(GetRloc16());
        }

        break;

    case kDeviceStateChild:
        m_routers[router_id].link_quality_out = 3;
        m_routers[router_id].link_quality_in = 3;
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        // Leader Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
        VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(leader_data.GetPartitionId() == m_leader_data.GetPartitionId(), ;);

        // Link Margin
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkMargin, sizeof(link_margin), link_margin));
        VerifyOrExit(link_margin.IsValid(), error = kThreadError_Parse);
        m_routers[router_id].link_quality_out = 3;
        m_routers[router_id].link_quality_in = 3;

        // update routing table
        if (router_id != m_router_id && m_routers[router_id].nexthop == kMaxRouterId)
        {
            m_routers[router_id].nexthop = router_id;
            ResetAdvertiseInterval();
        }

        break;
    }

    // finish link synchronization
    memcpy(&neighbor->mac_addr, &mac_addr, sizeof(neighbor->mac_addr));
    neighbor->valid.rloc16 = source_address.GetRloc16();
    neighbor->valid.link_frame_counter = link_frame_counter.GetFrameCounter();
    neighbor->valid.mle_frame_counter = mle_frame_counter.GetFrameCounter();
    neighbor->last_heard = Timer::GetNow();
    neighbor->mode = kModeFFD | kModeRxOnWhenIdle | kModeFullNetworkData;
    neighbor->state = Neighbor::kStateValid;
    assert(key_sequence == m_key_manager->GetCurrentKeySequence() ||
           key_sequence == m_key_manager->GetPreviousKeySequence());
    neighbor->previous_key = key_sequence == m_key_manager->GetPreviousKeySequence();

    if (request)
    {
        // Challenge
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
        VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);

        // TLV Request
        if (Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlv_request), tlv_request) == kThreadError_None)
        {
            VerifyOrExit(tlv_request.IsValid(), error = kThreadError_Parse);
        }
        else
        {
            tlv_request.SetLength(0);
        }

        SuccessOrExit(error = SendLinkAccept(message_info, neighbor, tlv_request, challenge));
    }

exit:
    return error;
}

ThreadError MleRouter::SendLinkReject(const Ip6Address &destination)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandLinkReject));
    SuccessOrExit(error = AppendStatus(*message, StatusTlv::kError));

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent link reject\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::HandleLinkReject(const Message &message, const Ip6MessageInfo &message_info)
{
    Mac::Address64 mac_addr;

    dprintf("Received link reject\n");

    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    return kThreadError_None;
}

Child *MleRouter::NewChild()
{
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state == Neighbor::kStateInvalid)
        {
            return &m_children[i];
        }
    }

    return NULL;
}

Child *MleRouter::FindChild(const Mac::Address64 &address)
{
    Child *rval = NULL;

    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state != Neighbor::kStateInvalid &&
            memcmp(&m_children[i].mac_addr, &address, sizeof(m_children[i].mac_addr)) == 0)
        {
            ExitNow(rval = &m_children[i]);
        }
    }

exit:
    return rval;
}

uint8_t MleRouter::GetLinkCost(uint8_t router_id)
{
    uint8_t rval;

    assert(router_id <= kMaxRouterId);

    VerifyOrExit(router_id != m_router_id &&
                 router_id != kMaxRouterId &&
                 m_routers[router_id].state == Neighbor::kStateValid,
                 rval = kMaxRouteCost);

    rval = m_routers[router_id].link_quality_in;

    if (rval > m_routers[router_id].link_quality_out)
    {
        rval = m_routers[router_id].link_quality_out;
    }

    rval = LQI_TO_COST(rval);

exit:
    return rval;
}

ThreadError MleRouter::ProcessRouteTlv(const RouteTlv &route)
{
    ThreadError error = kThreadError_None;
    int8_t diff = route.GetRouterIdSequence() - m_router_id_sequence;
    bool old;

    // check for newer route data
    if (diff > 0 || m_device_state == kDeviceStateDetached)
    {
        m_router_id_sequence = route.GetRouterIdSequence();

        for (int i = 0; i < kMaxRouterId; i++)
        {
            old = m_routers[i].allocated;
            m_routers[i].allocated = route.IsRouterIdSet(i);

            if (old && !m_routers[i].allocated)
            {
                m_routers[i].nexthop = kMaxRouterId;
                m_address_resolver->Remove(i);
            }
        }

        if (GetDeviceState() == kDeviceStateRouter && !m_routers[m_router_id].allocated)
        {
            BecomeDetached();
            ExitNow(error = kThreadError_NoRoute);
        }

        m_routers[GetLeaderId()].last_heard = Timer::GetNow();
    }

exit:
    return error;
}

ThreadError MleRouter::HandleAdvertisement(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Mac::Address64 mac_addr;
    SourceAddressTlv source_address;
    LeaderDataTlv leader_data;
    RouteTlv route;
    uint32_t peer_partition_id;
    Router *router;
    Neighbor *neighbor;
    uint8_t router_id;
    uint8_t router_count;

    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(source_address), source_address));
    VerifyOrExit(source_address.IsValid(), error = kThreadError_Parse);

    // Remove stale neighbors
    if ((neighbor = GetNeighbor(mac_addr)) != NULL &&
        neighbor->valid.rloc16 != source_address.GetRloc16())
    {
        neighbor->state = Neighbor::kStateInvalid;
    }

    // Leader Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data));
    VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);

    dprintf("Received advertisement from %04x\n", source_address.GetRloc16());

    peer_partition_id = leader_data.GetPartitionId();

    if (peer_partition_id != m_leader_data.GetPartitionId())
    {
        dprintf("different partition! %d %d %d %d\n",
                leader_data.GetWeighting(), peer_partition_id,
                m_leader_data.GetWeighting(), m_leader_data.GetPartitionId());

        if ((leader_data.GetWeighting() > m_leader_data.GetWeighting()) ||
            (leader_data.GetWeighting() == m_leader_data.GetWeighting() &&
             peer_partition_id > m_leader_data.GetPartitionId()))
        {
            dprintf("trying to migrate\n");
            BecomeChild(kJoinBetterPartition);
        }

        ExitNow(error = kThreadError_Drop);
    }
    else if (leader_data.GetRouterId() != GetLeaderId())
    {
        BecomeDetached();
        ExitNow(error = kThreadError_Drop);
    }

    VerifyOrExit(GetChildId(source_address.GetRloc16()) == 0, ;);

    // Route Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kRoute, sizeof(route), route));
    VerifyOrExit(route.IsValid(), error = kThreadError_Parse);

    if ((GetDeviceState() == kDeviceStateChild &&
         memcmp(&m_parent.mac_addr, &mac_addr, sizeof(m_parent.mac_addr)) == 0) ||
        GetDeviceState() == kDeviceStateRouter || GetDeviceState() == kDeviceStateLeader)
    {
        SuccessOrExit(error = ProcessRouteTlv(route));
    }

    router_id = GetRouterId(source_address.GetRloc16());
    router = NULL;

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        ExitNow();

    case kDeviceStateChild:
        router_count = 0;

        for (int i = 0; i < kMaxRouterId; i++)
        {
            router_count += m_routers[i].allocated;
        }

        if (router_count < m_router_upgrade_threshold)
        {
            BecomeRouter();
            ExitNow();
        }

        router = &m_parent;

        if (memcmp(&router->mac_addr, &mac_addr, sizeof(router->mac_addr)) == 0)
        {
            if (router->valid.rloc16 != source_address.GetRloc16())
            {
                SetStateDetached();
                ExitNow(error = kThreadError_NoRoute);
            }
        }
        else
        {
            router = &m_routers[router_id];

            if (router->state != Neighbor::kStateValid)
            {
                memcpy(&router->mac_addr, &mac_addr, sizeof(router->mac_addr));
                router->state = Neighbor::kStateLinkRequest;
                router->previous_key = false;
                SendLinkRequest(router);
                ExitNow(error = kThreadError_NoRoute);
            }
        }

        router->last_heard = Timer::GetNow();
        router->link_quality_in =
            LinkMarginToQuality(reinterpret_cast<const ThreadMessageInfo *>(message_info.link_info)->link_margin);

        ExitNow();

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        router = &m_routers[router_id];

        // router is not in list, reject
        if (!router->allocated)
        {
            ExitNow(error = kThreadError_NoRoute);
        }

        // Send link request if no link to router
        if (router->state != Neighbor::kStateValid)
        {
            memcpy(&router->mac_addr, &mac_addr, sizeof(router->mac_addr));
            router->state = Neighbor::kStateLinkRequest;
            router->frame_pending = false;
            router->data_request = false;
            router->previous_key = false;
            SendLinkRequest(router);
            ExitNow(error = kThreadError_NoRoute);
        }

        router->last_heard = Timer::GetNow();
        router->link_quality_in =
            LinkMarginToQuality(reinterpret_cast<const ThreadMessageInfo *>(message_info.link_info)->link_margin);
        break;
    }

    UpdateRoutes(route, router_id);

exit:
    return error;
}

void MleRouter::UpdateRoutes(const RouteTlv &route, uint8_t router_id)
{
    uint8_t cur_cost;
    uint8_t new_cost;
    uint8_t old_next_hop;
    uint8_t cost;
    uint8_t lqi;
    bool update;

    // update routes
    do
    {
        update = false;

        for (int i = 0, route_count = 0; i < kMaxRouterId; i++)
        {
            if (route.IsRouterIdSet(i) == false)
            {
                continue;
            }

            if (m_routers[i].allocated == false)
            {
                route_count++;
                continue;
            }

            if (i == m_router_id)
            {
                lqi = route.GetLinkQualityIn(route_count);

                if (m_routers[router_id].link_quality_out != lqi)
                {
                    m_routers[router_id].link_quality_out = lqi;
                    update = true;
                }
            }
            else
            {
                old_next_hop = m_routers[i].nexthop;

                if (i == router_id)
                {
                    cost = 0;
                }
                else
                {
                    cost = route.GetRouteCost(route_count);

                    if (cost == 0)
                    {
                        cost = kMaxRouteCost;
                    }
                }

                if (i != router_id && cost == 0 && m_routers[i].nexthop == router_id)
                {
                    // route nexthop is neighbor, but neighbor no longer has route
                    ResetAdvertiseInterval();
                    m_routers[i].nexthop = kMaxRouterId;
                    m_routers[i].cost = 0;
                    m_routers[i].last_heard = Timer::GetNow();
                }
                else if (m_routers[i].nexthop == kMaxRouterId || m_routers[i].nexthop == router_id)
                {
                    // route has no nexthop or nexthop is neighbor
                    new_cost = cost + GetLinkCost(router_id);

                    if (i == router_id)
                    {
                        if (m_routers[i].nexthop == kMaxRouterId)
                        {
                            ResetAdvertiseInterval();
                        }

                        m_routers[i].nexthop = router_id;
                        m_routers[i].cost = 0;
                    }
                    else if (new_cost <= kMaxRouteCost)
                    {
                        if (m_routers[i].nexthop == kMaxRouterId)
                        {
                            ResetAdvertiseInterval();
                        }

                        m_routers[i].nexthop = router_id;
                        m_routers[i].cost = cost;
                    }
                    else if (m_routers[i].nexthop != kMaxRouterId)
                    {
                        ResetAdvertiseInterval();
                        m_routers[i].nexthop = kMaxRouterId;
                        m_routers[i].cost = 0;
                        m_routers[i].last_heard = Timer::GetNow();
                    }
                }
                else
                {
                    cur_cost = m_routers[i].cost + GetLinkCost(m_routers[i].nexthop);
                    new_cost = cost + GetLinkCost(router_id);

                    if (new_cost < cur_cost || (new_cost == cur_cost && i == router_id))
                    {
                        m_routers[i].nexthop = router_id;
                        m_routers[i].cost = cost;
                    }
                }

                update |= m_routers[i].nexthop != old_next_hop;
            }

            route_count++;
        }
    }
    while (update);

#if 1

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].allocated == false || m_routers[i].nexthop == kMaxRouterId)
        {
            continue;
        }

        dprintf("%x: %x %d %d %d %d\n", GetRloc16(i), GetRloc16(m_routers[i].nexthop),
                m_routers[i].cost, GetLinkCost(i), m_routers[i].link_quality_in, m_routers[i].link_quality_out);
    }

#endif
}

ThreadError MleRouter::HandleParentRequest(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Mac::Address64 mac_addr;
    VersionTlv version;
    ScanMaskTlv scan_mask;
    ChallengeTlv challenge;
    Child *child;

    dprintf("Received parent request\n");

    // A Router MUST NOT send an MLE Parent Response if:

    // 1. It has no available Child capacity (if Max Child Count minus
    // Child Count would be equal to zero)
    // ==> verified below when allocating a child entry

    // 2. It is disconnected from its Partition (that is, it has not
    // received an updated ID sequence number within LEADER_TIMEOUT
    // seconds
    VerifyOrExit((Timer::GetNow() - m_routers[GetLeaderId()].last_heard) < (m_network_id_timeout * 1000U),
                 error = kThreadError_Drop);

    // 3. Its current routing path cost to the Leader is infinite.
    VerifyOrExit(m_routers[GetLeaderId()].nexthop != kMaxRouterId, error = kThreadError_Drop);

    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    // Version
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kVersion, sizeof(version), version));
    VerifyOrExit(version.IsValid() && version.GetVersion() == kVersion, error = kThreadError_Parse);

    // Scan Mask
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kScanMask, sizeof(scan_mask), scan_mask));
    VerifyOrExit(scan_mask.IsValid(), error = kThreadError_Parse);

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        ExitNow();

    case kDeviceStateChild:
        VerifyOrExit(scan_mask.IsChildFlagSet(), ;);
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        VerifyOrExit(scan_mask.IsRouterFlagSet(), ;);
        break;
    }

    VerifyOrExit((child = FindChild(mac_addr)) != NULL || (child = NewChild()) != NULL, ;);
    memset(child, 0, sizeof(*child));

    // Challenge
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
    VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);

    // MAC Address
    memcpy(&child->mac_addr, &mac_addr, sizeof(child->mac_addr));

    child->state = Neighbor::kStateParentRequest;
    child->frame_pending = false;
    child->data_request = false;
    child->previous_key = false;
    child->rssi = reinterpret_cast<const ThreadMessageInfo *>(message_info.link_info)->link_margin;
    child->timeout = 2 * kParentRequestChildTimeout * 1000U;
    SuccessOrExit(error = SendParentResponse(child, challenge));

exit:
    return error;
}

void MleRouter::HandleStateUpdateTimer(void *context)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleStateUpdateTimer();
}

void MleRouter::HandleStateUpdateTimer()
{
    uint8_t leader_id = GetLeaderId();

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
        assert(false);
        break;

    case kDeviceStateDetached:
        SetStateDetached();
        BecomeChild(kJoinAnyPartition);
        ExitNow();

    case kDeviceStateChild:
    case kDeviceStateRouter:
        // verify path to leader
        dprintf("network id timeout = %d\n", (Timer::GetNow() - m_routers[leader_id].last_heard) / 1000);

        if ((Timer::GetNow() - m_routers[leader_id].last_heard) >= (m_network_id_timeout * 1000U))
        {
            BecomeChild(kJoinSamePartition);
        }

        break;

    case kDeviceStateLeader:

        // update router id sequence
        if ((Timer::GetNow() - m_routers[leader_id].last_heard) >= (kRouterIdSequencePeriod * 1000U))
        {
            m_router_id_sequence++;
            m_routers[leader_id].last_heard = Timer::GetNow();
        }

        break;
    }

    // update children state
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state == Neighbor::kStateInvalid)
        {
            continue;
        }

        if ((Timer::GetNow() - m_children[i].last_heard) >= m_children[i].timeout * 1000U)
        {
            m_children[i].state = Neighbor::kStateInvalid;
        }
    }

    // update router state
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].state != Neighbor::kStateInvalid)
        {
            if ((Timer::GetNow() - m_routers[i].last_heard) >= kMaxNeighborAge * 1000U)
            {
                m_routers[i].state = Neighbor::kStateInvalid;
                m_routers[i].nexthop = kMaxRouterId;
                m_routers[i].link_quality_in = 0;
                m_routers[i].link_quality_out = 0;
                m_routers[i].last_heard = Timer::GetNow();
            }
        }

        if (GetDeviceState() == kDeviceStateLeader)
        {
            if (m_routers[i].allocated)
            {
                if (m_routers[i].nexthop == kMaxRouterId &&
                    (Timer::GetNow() - m_routers[i].last_heard) >= kMaxLeaderToRouterTimeout * 1000U)
                {
                    ReleaseRouterId(i);
                }
            }
            else if (m_routers[i].reclaim_delay)
            {
                if ((Timer::GetNow() - m_routers[i].last_heard) >= ((kMaxLeaderToRouterTimeout + kRouterIdReuseDelay) * 1000U))
                {
                    m_routers[i].reclaim_delay = false;
                }
            }
        }
    }

    m_state_update_timer.Start(1000);

exit:
    {}
}

ThreadError MleRouter::SendParentResponse(Child *child, const ChallengeTlv &challenge)
{
    ThreadError error = kThreadError_None;
    Ip6Address destination;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandParentResponse));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendLeaderData(*message));
    SuccessOrExit(error = AppendLinkFrameCounter(*message));
    SuccessOrExit(error = AppendMleFrameCounter(*message));
    SuccessOrExit(error = AppendResponse(*message, challenge.GetChallenge(), challenge.GetLength()));

    for (uint8_t i = 0; i < sizeof(child->pending.challenge); i++)
    {
        child->pending.challenge[i] = Random::Get();
    }

    SuccessOrExit(error = AppendChallenge(*message, child->pending.challenge, sizeof(child->pending.challenge)));
    SuccessOrExit(error = AppendLinkMargin(*message, child->rssi));
    SuccessOrExit(error = AppendConnectivity(*message));
    SuccessOrExit(error = AppendVersion(*message));

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xfe80);
    memcpy(destination.addr8 + 8, &child->mac_addr, sizeof(child->mac_addr));
    destination.addr8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Parent Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

ThreadError MleRouter::UpdateChildAddresses(const AddressRegistrationTlv &tlv, Child &child)
{
    const AddressRegistrationEntry *entry;
    Context context;

    memset(child.ip6_address, 0, sizeof(child.ip6_address));

    for (size_t count = 0; count < sizeof(child.ip6_address) / sizeof(child.ip6_address[0]); count++)
    {
        if ((entry = tlv.GetAddressEntry(count)) == NULL)
        {
            break;
        }

        if (entry->IsCompressed())
        {
            // xxx check if context id exists
            m_network_data->GetContext(entry->GetContextId(), context);
            memcpy(&child.ip6_address[count], context.prefix, (context.prefix_length + 7) / 8);
            memcpy(child.ip6_address[count].addr8 + 8, entry->GetIid(), 8);
        }
        else
        {
            memcpy(&child.ip6_address[count], entry->GetIp6Address(), sizeof(child.ip6_address[count]));
        }
    }

    return kThreadError_None;
}

ThreadError MleRouter::HandleChildIdRequest(const Message &message, const Ip6MessageInfo &message_info,
                                            uint32_t key_sequence)
{
    ThreadError error = kThreadError_None;
    Mac::Address64 mac_addr;
    ResponseTlv response;
    LinkFrameCounterTlv link_frame_counter;
    MleFrameCounterTlv mle_frame_counter;
    ModeTlv mode;
    TimeoutTlv timeout;
    AddressRegistrationTlv address;
    TlvRequestTlv tlv_request;
    Child *child;

    dprintf("Received Child ID Request\n");

    // Find Child
    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    VerifyOrExit((child = FindChild(mac_addr)) != NULL, ;);

    // Response
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
    VerifyOrExit(response.IsValid() &&
                 memcmp(response.GetResponse(), child->pending.challenge, sizeof(child->pending.challenge)) == 0, ;);

    // Link-Layer Frame Counter
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkFrameCounter, sizeof(link_frame_counter),
                                      link_frame_counter));
    VerifyOrExit(link_frame_counter.IsValid(), error = kThreadError_Parse);

    // MLE Frame Counter
    if (Tlv::GetTlv(message, Tlv::kMleFrameCounter, sizeof(mle_frame_counter), mle_frame_counter) ==
        kThreadError_None)
    {
        VerifyOrExit(mle_frame_counter.IsValid(), error = kThreadError_Parse);
    }
    else
    {
        mle_frame_counter.SetFrameCounter(link_frame_counter.GetFrameCounter());
    }

    // Mode
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kMode, sizeof(mode), mode));
    VerifyOrExit(mode.IsValid(), error = kThreadError_Parse);

    // Timeout
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTimeout, sizeof(timeout), timeout));
    VerifyOrExit(timeout.IsValid(), error = kThreadError_Parse);

    // Ip6 Address
    address.SetLength(0);

    if ((mode.GetMode() & kModeFFD) == 0)
    {
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kAddressRegistration, sizeof(address), address));
        VerifyOrExit(address.IsValid(), error = kThreadError_Parse);
    }

    // TLV Request
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlv_request), tlv_request));
    VerifyOrExit(tlv_request.IsValid(), error = kThreadError_Parse);

    // Remove from router table
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].state != Neighbor::kStateInvalid &&
            memcmp(&m_routers[i].mac_addr, &mac_addr, sizeof(m_routers[i].mac_addr)) == 0)
        {
            m_routers[i].state = Neighbor::kStateInvalid;
            break;
        }
    }

    child->state = Neighbor::kStateChildIdRequest;
    child->last_heard = Timer::GetNow();
    child->valid.link_frame_counter = link_frame_counter.GetFrameCounter();
    child->valid.mle_frame_counter = mle_frame_counter.GetFrameCounter();
    child->mode = mode.GetMode();
    child->timeout = timeout.GetTimeout();

    if (mode.GetMode() & kModeFullNetworkData)
    {
        child->network_data_version = m_leader_data.GetDataVersion();
    }
    else
    {
        child->network_data_version = m_leader_data.GetStableDataVersion();
    }

    UpdateChildAddresses(address, *child);

    assert(key_sequence == m_key_manager->GetCurrentKeySequence() ||
           key_sequence == m_key_manager->GetPreviousKeySequence());
    child->previous_key = key_sequence == m_key_manager->GetPreviousKeySequence();

    for (uint8_t i = 0; i < tlv_request.GetLength(); i++)
    {
        child->request_tlvs[i] = tlv_request.GetTlvs()[i];
    }

    for (uint8_t i = tlv_request.GetLength(); i < sizeof(child->request_tlvs); i++)
    {
        child->request_tlvs[i] = Tlv::kInvalid;
    }

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        BecomeRouter();
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        SuccessOrExit(error = SendChildIdResponse(child));
        break;
    }

exit:
    return error;
}

ThreadError MleRouter::HandleChildUpdateRequest(const Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Mac::Address64 mac_addr;
    ModeTlv mode;
    ChallengeTlv challenge;
    AddressRegistrationTlv address;
    LeaderDataTlv leader_data;
    TimeoutTlv timeout;
    Child *child;
    uint8_t tlvs[7];
    uint8_t tlvs_length = 0;

    dprintf("Received Child Update Request\n");

    // Find Child
    memcpy(&mac_addr, message_info.peer_addr.addr8 + 8, sizeof(mac_addr));
    mac_addr.bytes[0] ^= 0x2;

    child = FindChild(mac_addr);

    if (child == NULL)
    {
        tlvs[tlvs_length++] = Tlv::kStatus;
        SendChildUpdateResponse(NULL, message_info, tlvs, tlvs_length, NULL);
        ExitNow();
    }

    tlvs[tlvs_length++] = Tlv::kSourceAddress;
    tlvs[tlvs_length++] = Tlv::kLeaderData;

    // Mode
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kMode, sizeof(mode), mode));
    VerifyOrExit(mode.IsValid(), error = kThreadError_Parse);
    child->mode = mode.GetMode();
    tlvs[tlvs_length++] = Tlv::kMode;

    // Challenge
    if (Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge) == kThreadError_None)
    {
        VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);
        tlvs[tlvs_length++] = Tlv::kResponse;
    }

    // Ip6 Address TLV
    if (Tlv::GetTlv(message, Tlv::kAddressRegistration, sizeof(address), address) == kThreadError_None)
    {
        VerifyOrExit(address.IsValid(), error = kThreadError_Parse);
        UpdateChildAddresses(address, *child);
        tlvs[tlvs_length++] = Tlv::kAddressRegistration;
    }

    // Leader Data
    if (Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leader_data), leader_data) == kThreadError_None)
    {
        VerifyOrExit(leader_data.IsValid(), error = kThreadError_Parse);

        if (child->mode & kModeFullNetworkData)
        {
            child->network_data_version = leader_data.GetDataVersion();
        }
        else
        {
            child->network_data_version = leader_data.GetStableDataVersion();
        }
    }

    // Timeout
    if (Tlv::GetTlv(message, Tlv::kTimeout, sizeof(timeout), timeout) == kThreadError_None)
    {
        VerifyOrExit(timeout.IsValid(), error = kThreadError_Parse);
        child->timeout = timeout.GetTimeout();
        tlvs[tlvs_length++] = Tlv::kTimeout;
    }

    child->last_heard = Timer::GetNow();

    SendChildUpdateResponse(child, message_info, tlvs, tlvs_length, &challenge);

exit:
    return error;
}

ThreadError MleRouter::HandleNetworkDataUpdateRouter()
{
    static const uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};
    Ip6Address destination;

    VerifyOrExit(m_device_state == kDeviceStateRouter || m_device_state == kDeviceStateLeader, ;);

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xff02);
    destination.addr16[7] = HostSwap16(0x0001);

    SendDataResponse(destination, tlvs, sizeof(tlvs));

exit:
    return kThreadError_None;
}

ThreadError MleRouter::SendChildIdResponse(Child *child)
{
    ThreadError error = kThreadError_None;
    Ip6Address destination;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildIdResponse));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendLeaderData(*message));

    child->valid.rloc16 = m_mesh->GetAddress16() | m_next_child_id;

    m_next_child_id++;

    if (m_next_child_id >= 512)
    {
        m_next_child_id = 1;
    }

    SuccessOrExit(error = AppendAddress16(*message, child->valid.rloc16));

    for (uint8_t i = 0; i < sizeof(child->request_tlvs); i++)
    {
        switch (child->request_tlvs[i])
        {
        case Tlv::kNetworkData:
            SuccessOrExit(error = AppendNetworkData(*message, (child->mode & kModeFullNetworkData) == 0));
            break;

        case Tlv::kRoute:
            SuccessOrExit(error = AppendRoute(*message));
            break;
        }
    }

    if ((child->mode & kModeFFD) == 0)
    {
        SuccessOrExit(error = AppendChildAddresses(*message, *child));
    }

    child->state = Neighbor::kStateValid;

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xfe80);
    memcpy(destination.addr8 + 8, &child->mac_addr, sizeof(child->mac_addr));
    destination.addr8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Child ID Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

ThreadError MleRouter::SendChildUpdateResponse(Child *child, const Ip6MessageInfo &message_info,
                                               const uint8_t *tlvs, uint8_t tlvs_length,
                                               const ChallengeTlv *challenge)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildUpdateResponse));

    for (int i = 0; i < tlvs_length; i++)
    {
        switch (tlvs[i])
        {
        case Tlv::kStatus:
            SuccessOrExit(error = AppendStatus(*message, StatusTlv::kError));
            break;

        case Tlv::kAddressRegistration:
            SuccessOrExit(error = AppendChildAddresses(*message, *child));
            break;

        case Tlv::kLeaderData:
            SuccessOrExit(error = AppendLeaderData(*message));
            break;

        case Tlv::kMode:
            SuccessOrExit(error = AppendMode(*message, child->mode));
            break;

        case Tlv::kResponse:
            SuccessOrExit(error = AppendResponse(*message, challenge->GetChallenge(), challenge->GetLength()));
            break;

        case Tlv::kSourceAddress:
            SuccessOrExit(error = AppendSourceAddress(*message));
            break;

        case Tlv::kTimeout:
            SuccessOrExit(error = AppendTimeout(*message, child->timeout));
            break;
        }
    }

    SuccessOrExit(error = SendMessage(*message, message_info.peer_addr));

    dprintf("Sent Child Update Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

Child *MleRouter::GetChild(uint16_t address)
{
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state == Neighbor::kStateValid && m_children[i].valid.rloc16 == address)
        {
            return &m_children[i];
        }
    }

    return NULL;
}

Child *MleRouter::GetChild(const Mac::Address64 &address)
{
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state == Neighbor::kStateValid &&
            memcmp(&m_children[i].mac_addr, &address, sizeof(m_children[i].mac_addr)) == 0)
        {
            return &m_children[i];
        }
    }

    return NULL;
}

Child *MleRouter::GetChild(const Mac::Address &address)
{
    switch (address.length)
    {
    case 2:
        return GetChild(address.address16);

    case 8:
        return GetChild(address.address64);
    }

    return NULL;
}

int MleRouter::GetChildIndex(const Child &child)
{
    return &child - m_children;
}

Child *MleRouter::GetChildren(uint8_t *num_children)
{
    if (num_children != NULL)
    {
        *num_children = kMaxChildren;
    }

    return m_children;
}

Neighbor *MleRouter::GetNeighbor(uint16_t address)
{
    Neighbor *rval = NULL;

    if (address == Mac::kShortAddrBroadcast || address == Mac::kShortAddrInvalid)
    {
        ExitNow();
    }

    if (m_device_state == kDeviceStateChild && (rval = Mle::GetNeighbor(address)) != NULL)
    {
        ExitNow();
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state == Neighbor::kStateValid && m_children[i].valid.rloc16 == address)
        {
            ExitNow(rval = &m_children[i]);
        }
    }

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].state == Neighbor::kStateValid && m_routers[i].valid.rloc16 == address)
        {
            ExitNow(rval = &m_routers[i]);
        }
    }

exit:
    return rval;
}

Neighbor *MleRouter::GetNeighbor(const Mac::Address64 &address)
{
    Neighbor *rval = NULL;

    if (m_device_state == kDeviceStateChild && (rval = Mle::GetNeighbor(address)) != NULL)
    {
        ExitNow();
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state == Neighbor::kStateValid &&
            memcmp(&m_children[i].mac_addr, &address, sizeof(m_children[i].mac_addr)) == 0)
        {
            ExitNow(rval = &m_children[i]);
        }
    }

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].state == Neighbor::kStateValid &&
            memcmp(&m_routers[i].mac_addr, &address, sizeof(m_routers[i].mac_addr)) == 0)
        {
            ExitNow(rval = &m_routers[i]);
        }
    }

exit:
    return rval;
}

Neighbor *MleRouter::GetNeighbor(const Mac::Address &address)
{
    Neighbor *rval = NULL;

    switch (address.length)
    {
    case 2:
        rval = GetNeighbor(address.address16);
        break;

    case 8:
        rval = GetNeighbor(address.address64);
        break;

    default:
        break;
    }

    return rval;
}

Neighbor *MleRouter::GetNeighbor(const Ip6Address &address)
{
    Mac::Address macaddr;
    Context context;
    Child *child;
    Router *router;
    Neighbor *rval = NULL;

    if (address.IsLinkLocal())
    {
        if (address.addr16[4] == HostSwap16(0x0000) &&
            address.addr16[5] == HostSwap16(0x00ff) &&
            address.addr16[6] == HostSwap16(0xfe00))
        {
            macaddr.length = 2;
            macaddr.address16 = HostSwap16(address.addr16[7]);
        }
        else
        {
            macaddr.length = 8;
            memcpy(macaddr.address64.bytes, address.addr8 + 8, sizeof(macaddr.address64));
            macaddr.address64.bytes[0] ^= 0x02;
        }

        ExitNow(rval = GetNeighbor(macaddr));
    }

    if (m_network_data->GetContext(address, context) != kThreadError_None)
    {
        context.context_id = 0xff;
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        child = &m_children[i];

        if (child->state != Neighbor::kStateValid)
        {
            continue;
        }

        if (context.context_id == 0 &&
            address.addr16[4] == HostSwap16(0x0000) &&
            address.addr16[5] == HostSwap16(0x00ff) &&
            address.addr16[6] == HostSwap16(0xfe00) &&
            address.addr16[7] == HostSwap16(child->valid.rloc16))
        {
            ExitNow(rval = child);
        }

        for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++)
        {
            if (memcmp(&child->ip6_address[j], address.addr8, sizeof(child->ip6_address[j])) == 0)
            {
                ExitNow(rval = child);
            }
        }
    }

    VerifyOrExit(context.context_id == 0, rval = NULL);

    for (int i = 0; i < kMaxRouterId; i++)
    {
        router = &m_routers[i];

        if (router->state != Neighbor::kStateValid)
        {
            continue;
        }

        if (address.addr16[4] == HostSwap16(0x0000) &&
            address.addr16[5] == HostSwap16(0x00ff) &&
            address.addr16[6] == HostSwap16(0xfe00) &&
            address.addr16[7] == HostSwap16(router->valid.rloc16))
        {
            ExitNow(rval = router);
        }
    }

exit:
    return rval;
}

uint16_t MleRouter::GetNextHop(uint16_t destination) const
{
    uint8_t nexthop;

    if (m_device_state == kDeviceStateChild)
    {
        return Mle::GetNextHop(destination);
    }

    nexthop = m_routers[GetRouterId(destination)].nexthop;

    if (nexthop == kMaxRouterId || m_routers[nexthop].state == Neighbor::kStateInvalid)
    {
        return Mac::kShortAddrInvalid;
    }

    return GetRloc16(nexthop);
}

uint8_t MleRouter::GetRouteCost(uint16_t rloc) const
{
    uint8_t router_id = GetRouterId(rloc);
    uint8_t rval;

    VerifyOrExit(router_id < kMaxRouterId && m_routers[router_id].nexthop != kMaxRouterId, rval = kMaxRouteCost);

    rval = m_routers[router_id].cost;

exit:
    return rval;
}

uint8_t MleRouter::GetRouterIdSequence() const
{
    return m_router_id_sequence;
}

uint8_t MleRouter::GetLeaderWeight() const
{
    return m_leader_weight;
}

ThreadError MleRouter::SetLeaderWeight(uint8_t weight)
{
    m_leader_weight = weight;
    return kThreadError_None;
}

ThreadError MleRouter::HandleMacDataRequest(const Child &child)
{
    static const uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};
    Ip6Address destination;

    VerifyOrExit(child.state == Neighbor::kStateValid && (child.mode & kModeRxOnWhenIdle) == 0, ;);

    memset(&destination, 0, sizeof(destination));
    destination.addr16[0] = HostSwap16(0xfe80);
    memcpy(destination.addr8 + 8, &child.mac_addr, sizeof(child.mac_addr));
    destination.addr8[8] ^= 0x2;

    if (child.mode & kModeFullNetworkData)
    {
        if (child.network_data_version != m_network_data->GetVersion())
        {
            SendDataResponse(destination, tlvs, sizeof(tlvs));
        }
    }
    else
    {
        if (child.network_data_version != m_network_data->GetStableVersion())
        {
            SendDataResponse(destination, tlvs, sizeof(tlvs));
        }
    }

exit:
    return kThreadError_None;
}

Router *MleRouter::GetRouters(uint8_t *num_routers)
{
    if (num_routers != NULL)
    {
        *num_routers = kMaxRouterId;
    }

    return m_routers;
}

ThreadError MleRouter::CheckReachability(Mac::Address16 meshsrc, Mac::Address16 meshdst, Ip6Header &ip6_header)
{
    Ip6Address destination;

    if (m_device_state == kDeviceStateChild)
    {
        return Mle::CheckReachability(meshsrc, meshdst, ip6_header);
    }

    if (meshdst == m_mesh->GetAddress16())
    {
        // mesh destination is this device
        if (m_netif->IsUnicastAddress(*ip6_header.GetDestination()))
        {
            // IPv6 destination is this device
            return kThreadError_None;
        }
        else if (GetNeighbor(*ip6_header.GetDestination()) != NULL)
        {
            // IPv6 destination is an RFD child
            return kThreadError_None;
        }
    }
    else if (GetRouterId(meshdst) == m_router_id)
    {
        // mesh destination is a child of this device
        if (GetChild(meshdst))
        {
            return kThreadError_None;
        }
    }
    else if (GetNextHop(meshdst) != Mac::kShortAddrInvalid)
    {
        // forwarding to another router and route is known
        return kThreadError_None;
    }

    memcpy(&destination, GetMeshLocal16(), 14);
    destination.addr16[7] = HostSwap16(meshsrc);
    Icmp6::SendError(destination, Icmp6Header::kTypeDstUnreach, Icmp6Header::kCodeDstUnreachNoRoute, ip6_header);

    return kThreadError_Drop;
}

ThreadError MleRouter::SendAddressSolicit()
{
    ThreadError error = kThreadError_None;
    Coap::Header header;
    ThreadMacAddr64Tlv mac_addr64_tlv;
    ThreadRlocTlv rloc_tlv;
    Ip6MessageInfo message_info;
    Message *message;

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
    header.AppendUriPathOptions("a/as");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    mac_addr64_tlv.Init();
    mac_addr64_tlv.SetMacAddr(*m_mesh->GetAddress64());
    SuccessOrExit(error = message->Append(&mac_addr64_tlv, sizeof(mac_addr64_tlv)));

    if (m_previous_router_id != kMaxRouterId)
    {
        rloc_tlv.Init();
        rloc_tlv.SetRloc16(GetRloc16(m_previous_router_id));
        SuccessOrExit(error = message->Append(&rloc_tlv, sizeof(rloc_tlv)));
    }

    memset(&message_info, 0, sizeof(message_info));
    SuccessOrExit(error = GetLeaderAddress(message_info.peer_addr));
    message_info.peer_port = kCoapUdpPort;
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent address solicit to %04x\n", HostSwap16(message_info.peer_addr.addr16[7]));

exit:
    return error;
}

ThreadError MleRouter::SendAddressRelease()
{
    ThreadError error = kThreadError_None;
    Coap::Header header;
    ThreadRlocTlv rloc_tlv;
    ThreadMacAddr64Tlv mac_addr64_tlv;
    Ip6MessageInfo message_info;
    Message *message;

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
    header.AppendUriPathOptions("a/ar");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    rloc_tlv.Init();
    rloc_tlv.SetRloc16(GetRloc16(m_router_id));
    SuccessOrExit(error = message->Append(&rloc_tlv, sizeof(rloc_tlv)));

    mac_addr64_tlv.Init();
    mac_addr64_tlv.SetMacAddr(*m_mesh->GetAddress64());
    SuccessOrExit(error = message->Append(&mac_addr64_tlv, sizeof(mac_addr64_tlv)));

    memset(&message_info, 0, sizeof(message_info));
    SuccessOrExit(error = GetLeaderAddress(message_info.peer_addr));
    message_info.peer_port = kCoapUdpPort;
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent address release\n");

exit:
    return error;
}

void MleRouter::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void MleRouter::HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info)
{
    HandleAddressSolicitResponse(message);
}

void MleRouter::HandleAddressSolicitResponse(Message &message)
{
    Coap::Header header;
    ThreadStatusTlv status_tlv;
    ThreadRlocTlv rloc_tlv;
    ThreadRouterMaskTlv router_mask_tlv;
    bool old;

    SuccessOrExit(header.FromMessage(message));
    VerifyOrExit(header.GetType() == Coap::Header::kTypeAcknowledgment &&
                 header.GetCode() == Coap::Header::kCodeChanged &&
                 header.GetMessageId() == m_coap_message_id &&
                 header.GetTokenLength() == sizeof(m_coap_token) &&
                 memcmp(m_coap_token, header.GetToken(), sizeof(m_coap_token)) == 0, ;);
    message.MoveOffset(header.GetLength());

    dprintf("Received address reply\n");

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kStatus, sizeof(status_tlv), status_tlv));
    VerifyOrExit(status_tlv.IsValid() && status_tlv.GetStatus() == status_tlv.kSuccess, ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rloc_tlv), rloc_tlv));
    VerifyOrExit(rloc_tlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRouterMask, sizeof(router_mask_tlv), router_mask_tlv));
    VerifyOrExit(router_mask_tlv.IsValid(), ;);

    // assign short address
    m_router_id = GetRouterId(rloc_tlv.GetRloc16());
    m_previous_router_id = m_router_id;
    SuccessOrExit(SetStateRouter(GetRloc16(m_router_id)));
    m_routers[m_router_id].cost = 0;

    // copy router id information
    m_router_id_sequence = router_mask_tlv.GetRouterIdSequence();

    for (int i = 0; i < kMaxRouterId; i++)
    {
        old = m_routers[i].allocated;
        m_routers[i].allocated = router_mask_tlv.IsRouterIdSet(i);

        if (old && !m_routers[i].allocated)
        {
            m_address_resolver->Remove(i);
        }
    }

    // send link request
    SendLinkRequest(NULL);
    ResetAdvertiseInterval();

    // send child id responses
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (m_children[i].state == Neighbor::kStateChildIdRequest)
        {
            SendChildIdResponse(&m_children[i]);
        }
    }

exit:
    {}
}

void MleRouter::HandleAddressSolicit(void *context, Coap::Header &header, Message &message,
                                     const Ip6MessageInfo &message_info)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleAddressSolicit(header, message, message_info);
}

void MleRouter::HandleAddressSolicit(Coap::Header &header, Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    ThreadMacAddr64Tlv mac_addr64_tlv;
    ThreadRlocTlv rloc_tlv;
    int router_id;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address solicit\n");

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kMacAddr64, sizeof(mac_addr64_tlv), mac_addr64_tlv));
    VerifyOrExit(mac_addr64_tlv.IsValid(), error = kThreadError_Parse);

    router_id = -1;

    // see if allocation already exists
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].allocated &&
            memcmp(&m_routers[i].mac_addr, mac_addr64_tlv.GetMacAddr(), sizeof(m_routers[i].mac_addr)) == 0)
        {
            SendAddressSolicitResponse(header, i, message_info);
            ExitNow();
        }
    }

    if (ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rloc_tlv), rloc_tlv) == kThreadError_None)
    {
        // specific Router ID requested
        VerifyOrExit(rloc_tlv.IsValid(), error = kThreadError_Parse);
        router_id = GetRouterId(rloc_tlv.GetRloc16());

        if (router_id >= kMaxRouterId)
        {
            // requested Router ID is out of range
            router_id = -1;
        }
        else if (m_routers[router_id].allocated &&
                 memcmp(&m_routers[router_id].mac_addr, mac_addr64_tlv.GetMacAddr(),
                        sizeof(m_routers[router_id].mac_addr)))
        {
            // requested Router ID is allocated to another device
            router_id = -1;
        }
        else if (!m_routers[router_id].allocated && m_routers[router_id].reclaim_delay)
        {
            // requested Router ID is deallocated but within ID_REUSE_DELAY period
            router_id = -1;
        }
        else
        {
            router_id = AllocateRouterId(router_id);
        }
    }

    // allocate new router id
    if (router_id < 0)
    {
        router_id = AllocateRouterId();
    }
    else
    {
        dprintf("router id requested and provided!\n");
    }

    if (router_id >= 0)
    {
        memcpy(&m_routers[router_id].mac_addr, mac_addr64_tlv.GetMacAddr(), sizeof(m_routers[router_id].mac_addr));
    }
    else
    {
        dprintf("router address unavailable!\n");
    }

    SendAddressSolicitResponse(header, router_id, message_info);

exit:
    {}
}

void MleRouter::SendAddressSolicitResponse(const Coap::Header &request_header, int router_id,
                                           const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Coap::Header response_header;
    ThreadStatusTlv status_tlv;
    ThreadRouterMaskTlv router_mask_tlv;
    ThreadRlocTlv rloc_tlv;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    response_header.Init();
    response_header.SetVersion(1);
    response_header.SetType(Coap::Header::kTypeAcknowledgment);
    response_header.SetCode(Coap::Header::kCodeChanged);
    response_header.SetMessageId(request_header.GetMessageId());
    response_header.SetToken(request_header.GetToken(), request_header.GetTokenLength());
    response_header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    response_header.Finalize();
    SuccessOrExit(error = message->Append(response_header.GetBytes(), response_header.GetLength()));

    status_tlv.Init();
    status_tlv.SetStatus((router_id < 0) ? status_tlv.kNoAddressAvailable : status_tlv.kSuccess);
    SuccessOrExit(error = message->Append(&status_tlv, sizeof(status_tlv)));

    if (router_id >= 0)
    {
        rloc_tlv.Init();
        rloc_tlv.SetRloc16(GetRloc16(router_id));
        SuccessOrExit(error = message->Append(&rloc_tlv, sizeof(rloc_tlv)));

        router_mask_tlv.Init();
        router_mask_tlv.SetRouterIdSequence(m_router_id_sequence);
        router_mask_tlv.ClearRouterIdMask();

        for (int i = 0; i < kMaxRouterId; i++)
        {
            if (m_routers[i].allocated)
            {
                router_mask_tlv.SetRouterId(i);
            }
        }

        SuccessOrExit(error = message->Append(&router_mask_tlv, sizeof(router_mask_tlv)));
    }

    SuccessOrExit(error = m_coap_server->SendMessage(*message, message_info));

    dprintf("Sent address reply\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

void MleRouter::HandleAddressRelease(void *context, Coap::Header &header, Message &message,
                                     const Ip6MessageInfo &message_info)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleAddressRelease(header, message, message_info);
}

void MleRouter::HandleAddressRelease(Coap::Header &header, Message &message,
                                     const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    ThreadRlocTlv rloc_tlv;
    ThreadMacAddr64Tlv mac_addr64_tlv;
    uint8_t router_id;
    Router *router;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address release\n");

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rloc_tlv), rloc_tlv));
    VerifyOrExit(rloc_tlv.IsValid(), ;);

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kMacAddr64, sizeof(mac_addr64_tlv), mac_addr64_tlv));
    VerifyOrExit(mac_addr64_tlv.IsValid(), error = kThreadError_Parse);

    router_id = GetRouterId(rloc_tlv.GetRloc16());
    router = &m_routers[router_id];
    VerifyOrExit(memcmp(&router->mac_addr, mac_addr64_tlv.GetMacAddr(), sizeof(router->mac_addr)) == 0, ;);

    ReleaseRouterId(router_id);
    SendAddressReleaseResponse(header, message_info);

exit:
    {}
}

void MleRouter::SendAddressReleaseResponse(const Coap::Header &request_header, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Coap::Header response_header;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    response_header.Init();
    response_header.SetVersion(1);
    response_header.SetType(Coap::Header::kTypeAcknowledgment);
    response_header.SetCode(Coap::Header::kCodeChanged);
    response_header.SetMessageId(request_header.GetMessageId());
    response_header.SetToken(request_header.GetToken(), request_header.GetTokenLength());
    response_header.Finalize();
    SuccessOrExit(error = message->Append(response_header.GetBytes(), response_header.GetLength()));

    SuccessOrExit(error = m_coap_server->SendMessage(*message, message_info));

    dprintf("Sent address release response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError MleRouter::AppendConnectivity(Message &message)
{
    ThreadError error;
    ConnectivityTlv tlv;
    uint8_t cost;
    uint8_t lqi;

    tlv.Init();
    tlv.SetMaxChildCount(kMaxChildren);

    // compute number of children
    tlv.SetChildCount(0);

    for (int i = 0; i < kMaxChildren; i++)
    {
        tlv.SetChildCount(tlv.GetChildCount() + m_children[i].state == Neighbor::kStateValid);
    }

    // compute leader cost and link qualities
    tlv.SetLinkQuality1(0);
    tlv.SetLinkQuality2(0);
    tlv.SetLinkQuality3(0);

    cost = m_routers[GetLeaderId()].cost;

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        switch (m_parent.link_quality_in)
        {
        case 1:
            tlv.SetLinkQuality1(tlv.GetLinkQuality1() + 1);
            break;

        case 2:
            tlv.SetLinkQuality2(tlv.GetLinkQuality2() + 1);
            break;

        case 3:
            tlv.SetLinkQuality3(tlv.GetLinkQuality3() + 1);
            break;
        }

        cost += LQI_TO_COST(m_parent.link_quality_in);
        break;

    case kDeviceStateRouter:
        cost += GetLinkCost(m_routers[GetLeaderId()].nexthop);
        break;

    case kDeviceStateLeader:
        cost = 0;
        break;
    }

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].state != Neighbor::kStateValid || i == m_router_id)
        {
            continue;
        }

        lqi = m_routers[i].link_quality_in;

        if (lqi > m_routers[i].link_quality_out)
        {
            lqi = m_routers[i].link_quality_out;
        }

        switch (lqi)
        {
        case 1:
            tlv.SetLinkQuality1(tlv.GetLinkQuality1() + 1);
            break;

        case 2:
            tlv.SetLinkQuality2(tlv.GetLinkQuality2() + 1);
            break;

        case 3:
            tlv.SetLinkQuality3(tlv.GetLinkQuality3() + 1);
            break;
        }
    }

    tlv.SetLeaderCost((cost < kMaxRouteCost) ? cost : kMaxRouteCost);
    tlv.SetRouterIdSequence(m_router_id_sequence);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));

exit:
    return error;
}

ThreadError MleRouter::AppendChildAddresses(Message &message, Child &child)
{
    ThreadError error;
    Tlv tlv;
    AddressRegistrationEntry entry;
    Context context;
    uint8_t length = 0;

    tlv.SetType(Tlv::kAddressRegistration);

    // compute size of TLV
    for (size_t i = 0; i < sizeof(child.ip6_address) / sizeof(child.ip6_address[0]); i++)
    {
        if (m_network_data->GetContext(child.ip6_address[i], context) == kThreadError_None)
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

    for (size_t i = 0; i < sizeof(child.ip6_address) / sizeof(child.ip6_address[0]); i++)
    {
        if (m_network_data->GetContext(child.ip6_address[i], context) == kThreadError_None)
        {
            // compressed entry
            entry.SetContextId(context.context_id);
            entry.SetIid(child.ip6_address[i].addr8 + 8);
            length = 9;
        }
        else
        {
            // uncompressed entry
            entry.SetUncompressed();
            entry.SetIp6Address(child.ip6_address[i]);
            length = 17;
        }

        SuccessOrExit(error = message.Append(&entry, length));
    }

exit:
    return error;
}

ThreadError MleRouter::AppendRoute(Message &message)
{
    ThreadError error;
    RouteTlv tlv;
    int route_count = 0;
    uint8_t cost;

    tlv.Init();
    tlv.SetRouterIdSequence(m_router_id_sequence);
    tlv.ClearRouterIdMask();

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (m_routers[i].allocated == false)
        {
            continue;
        }

        tlv.SetRouterId(i);

        if (i == m_router_id)
        {
            tlv.SetLinkQualityIn(route_count, 0);
            tlv.SetLinkQualityOut(route_count, 0);
            tlv.SetRouteCost(route_count, 1);
        }
        else
        {
            if (m_routers[i].nexthop == kMaxRouterId)
            {
                cost = 0;
            }
            else
            {
                cost = m_routers[i].cost + GetLinkCost(m_routers[i].nexthop);

                if (cost >= kMaxRouteCost)
                {
                    cost = 0;
                }
            }

            tlv.SetRouteCost(route_count, cost);
            tlv.SetLinkQualityIn(route_count, m_routers[i].link_quality_in);
            tlv.SetLinkQualityOut(route_count, m_routers[i].link_quality_out);
        }

        route_count++;
    }

    tlv.SetRouteDataLength(route_count);
    SuccessOrExit(error = message.Append(&tlv, sizeof(Tlv) + tlv.GetLength()));

exit:
    return error;
}

}  // namespace Mle
}  // namespace Thread
