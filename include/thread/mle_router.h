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

#ifndef THREAD_MLE_ROUTER_H_
#define THREAD_MLE_ROUTER_H_

#include <coap/coap_message.h>
#include <coap/coap_server.h>
#include <common/timer.h>
#include <mac/mac_frame.h>
#include <net/icmp6.h>
#include <net/udp6.h>
#include <thread/mesh_forwarder.h>
#include <thread/mle.h>
#include <thread/network_data_leader.h>
#include <thread/topology.h>

namespace Thread {

class MleRouter: public Mle {
 public:
  MleRouter(AddressResolver *address_resolver, CoapServer *coap_server, KeyManager *key_manager,
            MeshForwarder *mesh, NetworkDataLeader *network_data, ThreadNetif *netif);

  uint8_t GetRouterIdSequence() const;
  uint8_t GetLeaderWeight() const;
  ThreadError SetLeaderWeight(uint8_t weight);

  Child *GetChild(MacAddr16 address);
  Child *GetChild(const MacAddr64 *address);
  Child *GetChild(const MacAddress *address);
  int GetChildIndex(const Child *child);
  Child *GetChildren(uint8_t *num_children);
  Neighbor *GetNeighbor(uint16_t address);
  Neighbor *GetNeighbor(const MacAddr64 *address);
  Neighbor *GetNeighbor(const MacAddress *address);
  Neighbor *GetNeighbor(const Ip6Address *address);
  Router *GetRouters(uint8_t *num_routers);

  uint16_t GetNextHop(uint16_t destination);
  uint8_t GetRouteCost(uint16_t rloc);

  uint8_t GetDeviceMode() const;
  ThreadError HandleMacDataRequest(const Child *child);
  ThreadError CheckReachability(MacAddr16 meshsrc, MacAddr16 meshdst, const Ip6Header *ip6_header);

  ThreadError BecomeRouter();
  ThreadError BecomeLeader();
  ThreadError SetStateRouter(uint16_t rloc);
  ThreadError SetStateLeader(uint16_t rloc);

  uint8_t GetNetworkIdTimeout();
  ThreadError SetNetworkIdTimeout(uint8_t timeout);
  uint8_t GetRouterUpgradeThreshold();
  ThreadError SetRouterUpgradeThreshold(uint8_t threshold);

  ThreadError ReleaseRouterId(uint8_t router_id);
  uint32_t GetLeaderAge();

  ThreadError HandleDetachStart();
  ThreadError HandleChildStart(JoinMode mode);
  ThreadError HandleRouterStart();
  ThreadError HandleLeaderStart();
  ThreadError HandleLinkRequest(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleLinkAccept(Message *message, const Ip6MessageInfo *message_info, uint32_t key_sequence);
  ThreadError HandleLinkAcceptAndRequest(Message *message, const Ip6MessageInfo *message_info,
                                         uint32_t key_sequence);
  ThreadError HandleLinkReject(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleAdvertisement(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleParentRequest(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleChildIdRequest(Message *message, const Ip6MessageInfo *message_info, uint32_t key_sequence);
  ThreadError HandleChildUpdateRequest(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleNetworkDataUpdateRouter();

  ThreadError SendLinkReject(Ip6Address *destination);

 private:
  ThreadError SendAdvertisement();
  ThreadError SendLinkRequest(Neighbor *neighbor);
  ThreadError SendLinkAccept(const Ip6MessageInfo *message_info, Neighbor *neighbor, TlvRequestTlv *tlv_request,
                             ChallengeTlv *challenge);
  ThreadError SendParentResponse(Child *child, ChallengeTlv *challenge);
  ThreadError SendChildIdResponse(Child *child);
  ThreadError SendChildUpdateResponse(Child *child, const Ip6MessageInfo *message_info,
                                      const uint8_t *tlvs, uint8_t tlvs_length,  const ChallengeTlv *challenge);
  ThreadError HandleLinkAccept(Message *message, const Ip6MessageInfo *message_info, uint32_t key_sequence,
                               bool request);

  ThreadError SendAddressSolicit();
  ThreadError SendAddressRelease();

  uint8_t GetLinkCost(uint8_t router_id);

  Child *NewChild();
  Child *FindChild(uint16_t rloc16);
  Child *FindChild(const MacAddr64 *mac_addr);

  int AllocateRouterId();
  int AllocateRouterId(uint8_t router_id);
  bool InRouterIdMask(uint8_t router_id);

  ThreadError ResetAdvertiseInterval();

  static void HandleAdvertiseTimer(void *context);
  void HandleAdvertiseTimer();

  static void HandleStateUpdateTimer(void *context);
  void HandleStateUpdateTimer();

  static void RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info);
  void RecvFrom(Message *message, const Ip6MessageInfo *message_info);

  static void HandleAddressSolicit(void *context, CoapMessage *coap, Message *message,
                                   const Ip6MessageInfo *message_info);
  void HandleAddressSolicit(CoapMessage *coap, Message *message, const Ip6MessageInfo *message_info);

  static void HandleAddressRelease(void *context, CoapMessage *coap, Message *message,
                                   const Ip6MessageInfo *message_info);
  void HandleAddressRelease(CoapMessage *coap, Message *message, const Ip6MessageInfo *message_info);

  ThreadError AppendConnectivity(Message *message);
  ThreadError AppendChildAddresses(Message *message, Child *child);
  ThreadError AppendRoute(Message *message);
  ThreadError ProcessRouteTlv(RouteTlv *route);
  ThreadError UpdateChildAddresses(Ip6AddressTlv *tlv, Child *child);

  Timer advertise_timer_;
  Timer state_update_timer_;

  Udp6Socket socket_;
  CoapServer::Resource address_solicit_;
  CoapServer::Resource address_release_;
  uint8_t router_id_sequence_;
  Router routers_[kMaxRouterId];
  Child children_[kMaxChildren];
  uint8_t challenge_[8];
  uint16_t next_child_id_;
  uint8_t network_id_timeout_ = kNetworkIdTimeout;
  uint8_t router_upgrade_threshold_ = kRouterUpgradeThreadhold;
  uint8_t leader_weight_ = 0;

  int8_t router_id_ = kMaxRouterId;
  int8_t previous_router_id_ = kMaxRouterId;
  uint32_t advertise_interval_ = kAdvertiseIntervalMin;

  CoapServer *coap_server_ = NULL;
  uint8_t coap_token_[2];
  uint16_t coap_message_id_;
};

}  // namespace Thread

#endif  // THREAD_MLE_ROUTER_H_
