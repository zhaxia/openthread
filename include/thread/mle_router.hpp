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

/**
 * @file
 *   This file includes definitions for MLE functionality required by the Thread Router and Leader roles.
 */

#ifndef MLE_ROUTER_HPP_
#define MLE_ROUTER_HPP_

#include <coap/coap_header.hpp>
#include <coap/coap_server.hpp>
#include <common/timer.hpp>
#include <mac/mac_frame.hpp>
#include <net/icmp6.hpp>
#include <net/udp6.hpp>
#include <thread/mle.hpp>
#include <thread/mle_tlvs.hpp>
#include <thread/topology.hpp>

namespace Thread {
namespace Mle {

class MeshForwarder;
class NetworkDataLeader;

/**
 * @addtogroup core-mle-router
 *
 * @brief
 *   This module includes definitions for MLE functionality required by the Thread Router and Leader roles.
 *
 * @{
 */

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

    ThreadError ReleaseRouterId(uint8_t routerId);

    Child *GetChild(uint16_t address);
    Child *GetChild(const Mac::Address64 &address);
    Child *GetChild(const Mac::Address &address);
    int GetChildIndex(const Child &child);
    Child *GetChildren(uint8_t *numChildren);
    Neighbor *GetNeighbor(uint16_t address);
    Neighbor *GetNeighbor(const Mac::Address64 &address);
    Neighbor *GetNeighbor(const Mac::Address &address);
    Neighbor *GetNeighbor(const Ip6::Address &address);
    Router *GetRouters(uint8_t *numRouters);

    ThreadError HandleMacDataRequest(const Child &child);
    ThreadError CheckReachability(Mac::Address16 meshsrc, Mac::Address16 meshdst, Ip6::Header &ip6Header);

    ThreadError SendLinkReject(const Ip6::Address &destination);

private:
    ThreadError AppendConnectivity(Message &message);
    ThreadError AppendChildAddresses(Message &message, Child &child);
    ThreadError AppendRoute(Message &message);
    uint8_t GetLinkCost(uint8_t routerId);
    ThreadError HandleDetachStart();
    ThreadError HandleChildStart(otMleAttachFilter filter);
    ThreadError HandleLinkRequest(const Message &message, const Ip6::MessageInfo &messageInfo);
    ThreadError HandleLinkAccept(const Message &message, const Ip6::MessageInfo &messageInfo, uint32_t keySequence);
    ThreadError HandleLinkAccept(const Message &message, const Ip6::MessageInfo &messageInfo, uint32_t keySequence,
                                 bool request);
    ThreadError HandleLinkAcceptAndRequest(const Message &message, const Ip6::MessageInfo &messageInfo,
                                           uint32_t keySequence);
    ThreadError HandleLinkReject(const Message &message, const Ip6::MessageInfo &messageInfo);
    ThreadError HandleAdvertisement(const Message &message, const Ip6::MessageInfo &messageInfo);
    ThreadError HandleParentRequest(const Message &message, const Ip6::MessageInfo &messageInfo);
    ThreadError HandleChildIdRequest(const Message &message, const Ip6::MessageInfo &messageInfo,
                                     uint32_t keySequence);
    ThreadError HandleChildUpdateRequest(const Message &message, const Ip6::MessageInfo &messageInfo);
    ThreadError HandleNetworkDataUpdateRouter();

    ThreadError ProcessRouteTlv(const RouteTlv &route);
    ThreadError ResetAdvertiseInterval();
    ThreadError SendAddressSolicit();
    ThreadError SendAddressRelease();
    void SendAddressSolicitResponse(const Coap::Header &request, int routerId, const Ip6::MessageInfo &messageInfo);
    void SendAddressReleaseResponse(const Coap::Header &requestHeader, const Ip6::MessageInfo &messageInfo);
    ThreadError SendAdvertisement();
    ThreadError SendLinkRequest(Neighbor *neighbor);
    ThreadError SendLinkAccept(const Ip6::MessageInfo &messageInfo, Neighbor *neighbor,
                               const TlvRequestTlv &tlv_request, const ChallengeTlv &challenge);
    ThreadError SendParentResponse(Child *child, const ChallengeTlv &challenge);
    ThreadError SendChildIdResponse(Child *child);
    ThreadError SendChildUpdateResponse(Child *child, const Ip6::MessageInfo &messageInfo,
                                        const uint8_t *tlvs, uint8_t tlvsLength,  const ChallengeTlv *challenge);
    ThreadError SetStateRouter(uint16_t rloc);
    ThreadError SetStateLeader(uint16_t rloc);
    ThreadError UpdateChildAddresses(const AddressRegistrationTlv &tlv, Child &child);
    void UpdateRoutes(const RouteTlv &tlv, uint8_t routerId);

    static void HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo);
    void HandleUdpReceive(Message &message, const Ip6::MessageInfo &messageInfo);
    void HandleAddressSolicitResponse(Message &message);
    static void HandleAddressRelease(void *context, Coap::Header &header, Message &message,
                                     const Ip6::MessageInfo &messageInfo);
    void HandleAddressRelease(Coap::Header &header, Message &message, const Ip6::MessageInfo &messageInfo);
    static void HandleAddressSolicit(void *context, Coap::Header &header, Message &message,
                                     const Ip6::MessageInfo &messageInfo);
    void HandleAddressSolicit(Coap::Header &header, Message &message, const Ip6::MessageInfo &messageInfo);

    Child *NewChild();
    Child *FindChild(const Mac::Address64 &macAddr);

    int AllocateRouterId();
    int AllocateRouterId(uint8_t routerId);
    bool InRouterIdMask(uint8_t routerId);

    static void HandleAdvertiseTimer(void *context);
    void HandleAdvertiseTimer();
    static void HandleStateUpdateTimer(void *context);
    void HandleStateUpdateTimer();

    Timer mAdvertiseTimer;
    Timer mStateUpdateTimer;

    Ip6::UdpSocket mSocket;
    Coap::Resource mAddressSolicit;
    Coap::Resource mAddressRelease;

    uint8_t mRouterIdSequence;
    uint32_t mRouterIdSequenceLastUpdated;
    Router mRouters[kMaxRouterId];
    Child mChildren[kMaxChildren];

    uint8_t mChallenge[8];
    uint16_t mNextChildId;
    uint8_t mNetworkIdTimeout = kNetworkIdTimeout;
    uint8_t mRouterUpgradeThreshold = kRouterUpgradeThreadhold;
    uint8_t mLeaderWeight = 0;

    int8_t mRouterId = kMaxRouterId;
    int8_t mPreviousRouterId = kMaxRouterId;
    uint32_t mAdvertiseInterval = kAdvertiseIntervalMin;

    Coap::Server *mCoapServer = NULL;
    uint8_t mCoapToken[2];
    uint16_t mCoapMessageId;
};

}  // namespace Mle

/**
 * @}
 */

}  // namespace Thread

#endif  // MLE_ROUTER_HPP_
