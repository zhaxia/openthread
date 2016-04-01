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

#ifndef MLE_ROUTER_H_
#define MLE_ROUTER_H_

#include <coap/coap_header.h>
#include <coap/coap_server.h>
#include <common/timer.h>
#include <mac/mac_frame.h>
#include <net/icmp6.h>
#include <net/udp6.h>
#include <thread/mle.h>
#include <thread/mle_tlvs.h>
#include <thread/topology.h>

namespace Thread {
namespace Mle {

class MeshForwarder;
class NetworkDataLeader;

class MleRouter: public Mle
{
    friend class Mle;

public:
    explicit MleRouter(ThreadNetif &netif);

    ThreadError BecomeRouter();
    ThreadError BecomeLeader();

    uint32_t GetLeaderAge() const;

    uint8_t GetLeaderWeight() const;
    ThreadError SetLeaderWeight(uint8_t weight);

    uint16_t GetNextHop(uint16_t destination) const;

    uint8_t GetNetworkIdTimeout() const;
    ThreadError SetNetworkIdTimeout(uint8_t timeout);

    uint8_t GetRouteCost(uint16_t rloc) const;

    uint8_t GetRouterIdSequence() const;

    uint8_t GetRouterUpgradeThreshold() const;
    ThreadError SetRouterUpgradeThreshold(uint8_t threshold);

    ThreadError ReleaseRouterId(uint8_t router_id);

    Child *GetChild(uint16_t address);
    Child *GetChild(const Mac::Address64 &address);
    Child *GetChild(const Mac::Address &address);
    int GetChildIndex(const Child &child);
    Child *GetChildren(uint8_t *num_children);
    Neighbor *GetNeighbor(uint16_t address);
    Neighbor *GetNeighbor(const Mac::Address64 &address);
    Neighbor *GetNeighbor(const Mac::Address &address);
    Neighbor *GetNeighbor(const Ip6Address &address);
    Router *GetRouters(uint8_t *num_routers);

    ThreadError HandleMacDataRequest(const Child &child);
    ThreadError CheckReachability(Mac::Address16 meshsrc, Mac::Address16 meshdst, Ip6Header &ip6_header);

    ThreadError SendLinkReject(const Ip6Address &destination);

private:
    ThreadError AppendConnectivity(Message &message);
    ThreadError AppendChildAddresses(Message &message, Child &child);
    ThreadError AppendRoute(Message &message);
    uint8_t GetLinkCost(uint8_t router_id);
    ThreadError HandleDetachStart();
    ThreadError HandleChildStart(JoinMode mode);
    ThreadError HandleLinkRequest(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleLinkAccept(const Message &message, const Ip6MessageInfo &message_info, uint32_t key_sequence);
    ThreadError HandleLinkAccept(const Message &message, const Ip6MessageInfo &message_info, uint32_t key_sequence,
                                 bool request);
    ThreadError HandleLinkAcceptAndRequest(const Message &message, const Ip6MessageInfo &message_info,
                                           uint32_t key_sequence);
    ThreadError HandleLinkReject(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleAdvertisement(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleParentRequest(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleChildIdRequest(const Message &message, const Ip6MessageInfo &message_info,
                                     uint32_t key_sequence);
    ThreadError HandleChildUpdateRequest(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleNetworkDataUpdateRouter();

    ThreadError ProcessRouteTlv(const RouteTlv &route);
    ThreadError ResetAdvertiseInterval();
    ThreadError SendAddressSolicit();
    ThreadError SendAddressRelease();
    void SendAddressSolicitResponse(const Coap::Header &request, int router_id, const Ip6MessageInfo &message_info);
    void SendAddressReleaseResponse(const Coap::Header &request_header, const Ip6MessageInfo &message_info);
    ThreadError SendAdvertisement();
    ThreadError SendLinkRequest(Neighbor *neighbor);
    ThreadError SendLinkAccept(const Ip6MessageInfo &message_info, Neighbor *neighbor,
                               const TlvRequestTlv &tlv_request, const ChallengeTlv &challenge);
    ThreadError SendParentResponse(Child *child, const ChallengeTlv &challenge);
    ThreadError SendChildIdResponse(Child *child);
    ThreadError SendChildUpdateResponse(Child *child, const Ip6MessageInfo &message_info,
                                        const uint8_t *tlvs, uint8_t tlvs_length,  const ChallengeTlv *challenge);
    ThreadError SetStateRouter(uint16_t rloc);
    ThreadError SetStateLeader(uint16_t rloc);
    ThreadError UpdateChildAddresses(const AddressRegistrationTlv &tlv, Child &child);
    void UpdateRoutes(const RouteTlv &tlv, uint8_t router_id);

    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);
    void HandleAddressSolicitResponse(Message &message);
    static void HandleAddressRelease(void *context, Coap::Header &header, Message &message,
                                     const Ip6MessageInfo &message_info);
    void HandleAddressRelease(Coap::Header &header, Message &message, const Ip6MessageInfo &message_info);
    static void HandleAddressSolicit(void *context, Coap::Header &header, Message &message,
                                     const Ip6MessageInfo &message_info);
    void HandleAddressSolicit(Coap::Header &header, Message &message, const Ip6MessageInfo &message_info);

    Child *NewChild();
    Child *FindChild(const Mac::Address64 &mac_addr);

    int AllocateRouterId();
    int AllocateRouterId(uint8_t router_id);
    bool InRouterIdMask(uint8_t router_id);

    static void HandleAdvertiseTimer(void *context);
    void HandleAdvertiseTimer();
    static void HandleStateUpdateTimer(void *context);
    void HandleStateUpdateTimer();

    Timer m_advertise_timer;
    Timer m_state_update_timer;

    Udp6Socket m_socket;
    Coap::Resource m_address_solicit;
    Coap::Resource m_address_release;

    uint8_t m_router_id_sequence;
    Router m_routers[kMaxRouterId];
    Child m_children[kMaxChildren];

    uint8_t m_challenge[8];
    uint16_t m_next_child_id;
    uint8_t m_network_id_timeout = kNetworkIdTimeout;
    uint8_t m_router_upgrade_threshold = kRouterUpgradeThreadhold;
    uint8_t m_leader_weight = 0;

    int8_t m_router_id = kMaxRouterId;
    int8_t m_previous_router_id = kMaxRouterId;
    uint32_t m_advertise_interval = kAdvertiseIntervalMin;

    Coap::Server *m_coap_server = NULL;
    uint8_t m_coap_token[2];
    uint16_t m_coap_message_id;
};

}  // namespace Mle
}  // namespace Thread

#endif  // MLE_ROUTER_H_
