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

#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/random.h>
#include <mac/mac_frame.h>
#include <net/icmp6.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>
#include <thread/thread_tlvs.h>
#include <assert.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {

#define ROUTE_DATA_OUT_LQI(x) (((x) >> 6) & 0x3)
#define ROUTE_DATA_IN_LQI(x) (((x) >> 4) & 0x3)
#define ROUTE_DATA_COST(x) ((((x) & 0xf) != 0) ? (x) & 0xf : kMaxRouteCost)
#define LQI_TO_COST(x) (kLqiToCost[(x)])

static const uint8_t kLqiToCost[] = {
  16, 6, 2, 1,
};

MleRouter::MleRouter(AddressResolver *address_resolver, CoapServer *coap_server, KeyManager *key_manager,
                     MeshForwarder *mesh, NetworkDataLeader *network_data, ThreadNetif *netif):
    Mle(address_resolver, key_manager, mesh, this, netif, network_data),
    advertise_timer_(&HandleAdvertiseTimer, this),
    state_update_timer_(&HandleStateUpdateTimer, this),
    socket_(&RecvFrom, this),
    address_solicit_("a/as", &HandleAddressSolicit, this),
    address_release_("a/ar", &HandleAddressRelease, this) {
  next_child_id_ = 1;
  router_id_sequence_ = 0;
  memset(children_, 0, sizeof(children_));
  memset(routers_, 0, sizeof(routers_));
  coap_server_ = coap_server;
  coap_message_id_ = Random::Get();
}

int MleRouter::AllocateRouterId() {
  int rval = -1;

  // count available router ids
  uint8_t num_available = 0;
  uint8_t num_allocated = 0;
  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].allocated)
      num_allocated++;
    else if (routers_[i].reclaim_delay == false)
      num_available++;
  }

  VerifyOrExit(num_allocated < kMaxRouters && num_available > 0, rval = -1);

  // choose available router id at random
  uint8_t free_bit;
  // free_bit = Random::Get() % num_available;
  free_bit = 0;

  // allocate router id
  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].allocated || routers_[i].reclaim_delay)
      continue;
    if (free_bit == 0) {
      rval = AllocateRouterId(i);
      ExitNow();
    }
    free_bit--;
  }

exit:
  return rval;
}

int MleRouter::AllocateRouterId(uint8_t router_id) {
  int rval = -1;

  VerifyOrExit(!routers_[router_id].allocated, rval = -1);

  // init router state
  routers_[router_id].allocated = true;
  routers_[router_id].last_heard = Timer::GetNow();
  memset(&routers_[router_id].mac_addr, 0, sizeof(routers_[router_id].mac_addr));

  // bump sequence number
  router_id_sequence_++;
  routers_[router_id_].last_heard = Timer::GetNow();
  rval = router_id;

  dprintf("add router id %d\n", router_id);

exit:
  return rval;
}

ThreadError MleRouter::ReleaseRouterId(uint8_t router_id) {
  dprintf("delete router id %d\n", router_id);
  routers_[router_id].allocated = false;
  routers_[router_id].reclaim_delay = true;
  routers_[router_id].state = Neighbor::kStateInvalid;
  routers_[router_id].nexthop = kMaxRouterId;
  router_id_sequence_++;
  address_resolver_->Remove(router_id);
  network_data_->RemoveBorderRouter(ROUTER_ID_TO_ADDR16(router_id));
  ResetAdvertiseInterval();
  return kThreadError_None;
}

uint32_t MleRouter::GetLeaderAge() {
  return (Timer::GetNow() - routers_[GetLeaderId()].last_heard) / 1000;
}

ThreadError MleRouter::BecomeRouter() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(device_state_ == kDeviceStateDetached || device_state_ == kDeviceStateChild, error = kThreadError_Busy);
  VerifyOrExit(device_mode_ & kModeFFD, ;);

  for (int i = 0; i < kMaxRouterId; i++) {
    routers_[i].allocated = false;
    routers_[i].reclaim_delay = false;
    routers_[i].state = Neighbor::kStateInvalid;
    routers_[i].nexthop = kMaxRouterId;
  }

  advertise_timer_.Stop();
  address_resolver_->Clear();

  switch (device_state_) {
    case kDeviceStateDetached:
      SuccessOrExit(error = SendLinkRequest(NULL));
      state_update_timer_.Start(1000);
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

ThreadError MleRouter::BecomeLeader() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(device_state_ != kDeviceStateDisabled && device_state_ != kDeviceStateLeader,
               error = kThreadError_Busy);

  for (int i = 0; i < kMaxRouterId; i++) {
    routers_[i].allocated = false;
    routers_[i].reclaim_delay = false;
    routers_[i].state = Neighbor::kStateInvalid;
    routers_[i].nexthop = kMaxRouterId;
  }

  advertise_timer_.Stop();
  ResetAdvertiseInterval();
  state_update_timer_.Start(1000);
  address_resolver_->Clear();

  router_id_ = (previous_router_id_ != kMaxRouterId) ? AllocateRouterId(previous_router_id_) : AllocateRouterId();
  VerifyOrExit(router_id_ >= 0, error = kThreadError_NoBufs);

  memcpy(&routers_[router_id_].mac_addr, mesh_->GetAddress64(), sizeof(routers_[router_id_].mac_addr));

  leader_data_.partition_id = Random::Get();
  leader_data_.weighting = leader_weight_;
  leader_data_.leader_router_id = router_id_;

  network_data_->Init();

  SuccessOrExit(error = SetStateLeader(router_id_ << 10));

exit:
  return error;
}

ThreadError MleRouter::HandleDetachStart() {
  ThreadError error = kThreadError_None;

  for (int i = 0; i < kMaxRouterId; i++)
    routers_[i].state = Neighbor::kStateInvalid;
  for (int i = 0; i < kMaxChildren; i++)
    children_[i].state = Neighbor::kStateInvalid;

  advertise_timer_.Stop();
  state_update_timer_.Stop();
  network_data_->Stop();

  return error;
}

ThreadError MleRouter::HandleChildStart(JoinMode mode) {
#if 0
  for (int i = 0; i < kMaxRouterId; i++)
    routers_[i].state = Neighbor::kStateInvalid;
  for (int i = 0; i < kMaxChildren; i++)
    children_[i].state = Neighbor::kStateInvalid;
#endif

  routers_[GetLeaderId()].last_heard = Timer::GetNow();

  advertise_timer_.Stop();
  state_update_timer_.Start(1000);
  network_data_->Stop();

  switch (mode) {
    case kJoinAnyPartition:
      break;
    case kJoinSamePartition:
      SendAddressRelease();
      break;
    case kJoinBetterPartition:
      // BecomeRouter();
      break;
  }

  if (device_mode_ & kModeFFD) {
    uint32_t advertise_delay;
    advertise_delay = (kReedAdvertiseInterval + (Random::Get() % kReedAdvertiseJitter)) * 1000U;
    advertise_timer_.Start(advertise_delay);
    netif_->SubscribeAllRoutersMulticast();
  }

  return kThreadError_None;
}

ThreadError MleRouter::HandleRouterStart() {
  netif_->SubscribeAllRoutersMulticast();
  routers_[router_id_].nexthop = router_id_;
  routers_[GetLeaderId()].last_heard = Timer::GetNow();
  network_data_->Stop();
  state_update_timer_.Start(1000);

  return kThreadError_None;
}

ThreadError MleRouter::HandleLeaderStart() {
  netif_->SubscribeAllRoutersMulticast();
  routers_[router_id_].nexthop = router_id_;
  routers_[router_id_].last_heard = Timer::GetNow();

  network_data_->Start();
  coap_server_->AddResource(&address_solicit_);
  coap_server_->AddResource(&address_release_);

  return kThreadError_None;
}

ThreadError MleRouter::SetStateRouter(uint16_t address16) {
  SetAddress16(address16);
  device_state_ = kDeviceStateRouter;
  parent_request_state_ = kParentIdle;
  parent_request_timer_.Stop();
  mle_router_->HandleRouterStart();
  dprintf("Mode -> Router\n");
  return kThreadError_None;
}

ThreadError MleRouter::SetStateLeader(uint16_t address16) {
  SetAddress16(address16);
  device_state_ = kDeviceStateLeader;
  parent_request_state_ = kParentIdle;
  parent_request_timer_.Stop();
  mle_router_->HandleLeaderStart();
  SendLinkRequest(NULL);
  dprintf("Mode -> Leader %d\n", leader_data_.partition_id);
  return kThreadError_None;
}

uint8_t MleRouter::GetNetworkIdTimeout() {
  return network_id_timeout_;
}

ThreadError MleRouter::SetNetworkIdTimeout(uint8_t timeout) {
  network_id_timeout_ = timeout;
  return kThreadError_None;
}

uint8_t MleRouter::GetRouterUpgradeThreshold() {
  return router_upgrade_threshold_;
}

ThreadError MleRouter::SetRouterUpgradeThreshold(uint8_t threshold) {
  router_upgrade_threshold_ = threshold;
  return kThreadError_None;
}

void MleRouter::HandleAdvertiseTimer(void *context) {
  MleRouter *obj = reinterpret_cast<MleRouter*>(context);
  obj->HandleAdvertiseTimer();
}

void MleRouter::HandleAdvertiseTimer() {
  if ((device_mode_ & kModeFFD) == 0)
    return;

  SendAdvertisement();

  uint32_t advertise_delay;
  switch (GetDeviceState()) {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
      assert(false);
      break;
    case kDeviceStateChild:
      advertise_delay = (kReedAdvertiseInterval + (Random::Get() % kReedAdvertiseJitter)) * 1000U;
      advertise_timer_.Start(advertise_delay);
      break;
    case kDeviceStateRouter:
    case kDeviceStateLeader:
      advertise_interval_ *= 2;
      if (advertise_interval_ > kAdvertiseIntervalMax)
        advertise_interval_ = kAdvertiseIntervalMax;

      advertise_delay = (advertise_interval_ * 1000U) / 2;
      advertise_delay += Random::Get() % (advertise_delay);
      advertise_timer_.Start(advertise_delay);
      break;
  }
}

ThreadError MleRouter::ResetAdvertiseInterval() {
  VerifyOrExit(advertise_interval_ != kAdvertiseIntervalMin || !advertise_timer_.IsRunning(), ;);

  advertise_interval_ = kAdvertiseIntervalMin;

  uint32_t advertise_delay;
  advertise_delay = (advertise_interval_ * 1000U) / 2;
  advertise_delay += Random::Get() % advertise_delay;
  advertise_timer_.Start(advertise_delay);

  dprintf("reset advertise interval\n");

exit:
  return kThreadError_None;
}

ThreadError MleRouter::SendAdvertisement() {
  ThreadError error = kThreadError_None;
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandAdvertisement));
  SuccessOrExit(error = AppendSourceAddress(message));
  SuccessOrExit(error = AppendLeaderData(message));
  switch (GetDeviceState()) {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
      assert(false);
      break;
    case kDeviceStateChild:
      break;
    case kDeviceStateRouter:
    case kDeviceStateLeader:
      SuccessOrExit(error = AppendRoute(message));
      break;
  }

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xff02);
  destination.s6_addr16[7] = HostSwap16(0x0002);
  SuccessOrExit(error = SendMessage(message, &destination));

  dprintf("Sent advertisement\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError MleRouter::SendLinkRequest(Neighbor *neighbor) {
  ThreadError error = kThreadError_None;
  Message *message;
  Ip6Address destination;

  memset(&destination, 0, sizeof(destination));

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandLinkRequest));
  SuccessOrExit(error = AppendVersion(message));

  switch (device_state_) {
    case kDeviceStateDetached: {
      static const uint8_t tlvs[] = {Tlv::kTypeNetworkData, Tlv::kTypeAddress16, Tlv::kTypeRoute};
      SuccessOrExit(error = AppendTlvRequest(message, tlvs, sizeof(tlvs)));
      break;
    }
    case kDeviceStateChild: {
      SuccessOrExit(error = AppendSourceAddress(message));
      break;
    }
    case kDeviceStateRouter:
    case kDeviceStateLeader: {
      static const uint8_t tlvs[] = {Tlv::kTypeRssi};
      SuccessOrExit(error = AppendTlvRequest(message, tlvs, sizeof(tlvs)));
      SuccessOrExit(error = AppendSourceAddress(message));
      SuccessOrExit(error = AppendLeaderData(message));
      break;
    }
  }

  if (neighbor == NULL) {
    for (uint8_t i = 0; i < sizeof(challenge_); i++)
      challenge_[i] = Random::Get();
    SuccessOrExit(error = AppendChallenge(message, challenge_, sizeof(challenge_)));
    destination.s6_addr[0] = 0xff;
    destination.s6_addr[1] = 0x02;
    destination.s6_addr[15] = 1;
  } else {
    for (uint8_t i = 0; i < sizeof(neighbor->pending.challenge); i++)
      neighbor->pending.challenge[i] = Random::Get();
    SuccessOrExit(error = AppendChallenge(message, challenge_, sizeof(challenge_)));
    destination.s6_addr16[0] = HostSwap16(0xfe80);
    memcpy(destination.s6_addr + 8, &neighbor->mac_addr, sizeof(neighbor->mac_addr));
    destination.s6_addr[8] ^= 0x2;
  }

  SuccessOrExit(error = SendMessage(message, &destination));

  dprintf("Sent link request\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError MleRouter::HandleLinkRequest(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Neighbor *neighbor = NULL;

  dprintf("Received link request\n");

  VerifyOrExit(GetDeviceState() == kDeviceStateRouter ||
               GetDeviceState() == kDeviceStateLeader, ;);

  VerifyOrExit(parent_request_state_ == kParentIdle, ;);

  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  // Challenge
  ChallengeTlv challenge;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeChallenge, &challenge, sizeof(challenge)));
  VerifyOrExit(challenge.header.length <= sizeof(challenge.challenge), error = kThreadError_Parse);

  // Version
  VersionTlv version;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeVersion, &version, sizeof(version)));
  VerifyOrExit(HostSwap16(version.version) == kVersion, error = kThreadError_Parse);

  // Leader Data
  LeaderDataTlv leader_data;
  if (FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)) == kThreadError_None) {
    VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
                 error = kThreadError_Parse);
    VerifyOrExit(HostSwap32(leader_data.leader_data.partition_id) == GetLeaderData()->partition_id, ;);
  }

  // Source Address
  SourceAddressTlv source_address;
  if (FindTlv(message, Tlv::kTypeSourceAddress, &source_address, sizeof(source_address)) == kThreadError_None) {
    VerifyOrExit(source_address.header.length == sizeof(source_address) - sizeof(source_address.header),
                 error = kThreadError_Parse);

    uint16_t rloc16 = HostSwap16(source_address.address16);

    if ((neighbor = GetNeighbor(&mac_addr)) != NULL && neighbor->valid.address16 != rloc16) {
      // remove stale neighbors
      neighbor->state = Neighbor::kStateInvalid;
      neighbor = NULL;
    }

    if (ADDR16_TO_CHILD_ID(rloc16) == 0) {
      // source is a router
      neighbor = &routers_[ADDR16_TO_ROUTER_ID(rloc16)];
      if (neighbor->state != Neighbor::kStateValid) {
        memcpy(&neighbor->mac_addr, &mac_addr, sizeof(neighbor->mac_addr));
        neighbor->state = Neighbor::kStateLinkRequest;
      } else {
        VerifyOrExit(memcmp(&neighbor->mac_addr, &mac_addr, sizeof(neighbor->mac_addr)) == 0, ;);
      }
    }
  } else {
    // lack of source address indicates router coming out of reset
    VerifyOrExit((neighbor = GetNeighbor(&mac_addr)) != NULL, error = kThreadError_Drop);
  }

  // TLV Request
  TlvRequestTlv tlv_request;
  if (FindTlv(message, Tlv::kTypeTlvRequest, &tlv_request, sizeof(tlv_request)) == kThreadError_None) {
    VerifyOrExit(tlv_request.header.length <= sizeof(tlv_request.tlvs), error = kThreadError_Parse);
  } else {
    tlv_request.header.length = 0;
  }

  SuccessOrExit(error = SendLinkAccept(message_info, neighbor, &tlv_request, &challenge));

exit:
  return error;
}

ThreadError MleRouter::SendLinkAccept(const Ip6MessageInfo *message_info, Neighbor *neighbor,
                                      TlvRequestTlv *tlv_request, ChallengeTlv *challenge) {
  ThreadError error = kThreadError_None;
  Message *message;

  uint8_t command = (neighbor == NULL || neighbor->state == Neighbor::kStateValid) ?
      Header::kCommandLinkAccept : Header::kCommandLinkAcceptAndRequest;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, command));
  SuccessOrExit(error = AppendVersion(message));
  SuccessOrExit(error = AppendSourceAddress(message));
  SuccessOrExit(error = AppendResponse(message, challenge->challenge, challenge->header.length));
  SuccessOrExit(error = AppendLinkFrameCounter(message));
  SuccessOrExit(error = AppendMleFrameCounter(message));

  if (neighbor != NULL && ADDR16_TO_CHILD_ID(neighbor->valid.address16) == 0)
    SuccessOrExit(error = AppendLeaderData(message));

  for (uint8_t i = 0; i < tlv_request->header.length; i++) {
    switch (tlv_request->tlvs[i]) {
      case Tlv::kTypeRoute:
        SuccessOrExit(error = AppendRoute(message));
        break;
      case Tlv::kTypeAddress16:
        VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
        SuccessOrExit(error = AppendAddress16(message, neighbor->valid.address16));
        break;
      case Tlv::kTypeNetworkData:
        VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
        SuccessOrExit(error = AppendNetworkData(message, (neighbor->mode & kModeFullNetworkData) == 0));
        break;
      case Tlv::kTypeRssi:
        VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
        SuccessOrExit(error = AppendRssi(message, neighbor->rssi));
        break;
      default:
        ExitNow(error = kThreadError_Drop);
        break;
    }
  }

  if (neighbor != NULL && neighbor->state != Neighbor::kStateValid) {
    for (uint8_t i = 0; i < sizeof(neighbor->pending.challenge); i++)
      neighbor->pending.challenge[i] = Random::Get();
    SuccessOrExit(error = AppendChallenge(message, neighbor->pending.challenge, sizeof(neighbor->pending.challenge)));
    neighbor->state = Neighbor::kStateLinkRequest;
  }

  SuccessOrExit(error = SendMessage(message, &message_info->peer_addr));

  dprintf("Sent link accept\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError MleRouter::HandleLinkAccept(Message *message, const Ip6MessageInfo *message_info, uint32_t key_sequence) {
  dprintf("Received link accept\n");
  return HandleLinkAccept(message, message_info, key_sequence, false);
}

ThreadError MleRouter::HandleLinkAcceptAndRequest(Message *message, const Ip6MessageInfo *message_info,
                                                  uint32_t key_sequence) {
  dprintf("Received link accept and request\n");
  return HandleLinkAccept(message, message_info, key_sequence, true);
}

ThreadError MleRouter::HandleLinkAccept(Message *message, const Ip6MessageInfo *message_info, uint32_t key_sequence,
                                        bool request) {
  ThreadError error = kThreadError_None;
  Neighbor *neighbor = NULL;

  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  // Version
  VersionTlv version;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeVersion, &version, sizeof(version)));
  VerifyOrExit(version.header.length == sizeof(version) - sizeof(version.header), error = kThreadError_Parse);

  // Response
  ResponseTlv response;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeResponse, &response, sizeof(response)));
  VerifyOrExit(response.header.length == sizeof(response.response), error = kThreadError_Parse);

  // Source Address
  SourceAddressTlv source_address;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeSourceAddress, &source_address, sizeof(source_address)));
  VerifyOrExit(source_address.header.length == sizeof(source_address) - sizeof(source_address.header),
               error = kThreadError_Parse);

  // Remove stale neighbors
  if ((neighbor = GetNeighbor(&mac_addr)) != NULL &&
      neighbor->valid.address16 != HostSwap16(source_address.address16)) {
    neighbor->state = Neighbor::kStateInvalid;
    neighbor = NULL;
  }

  // Link-Layer Frame Counter
  LinkFrameCounterTlv link_frame_counter;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeLinkFrameCounter, &link_frame_counter, sizeof(link_frame_counter)));
  VerifyOrExit(link_frame_counter.header.length == sizeof(link_frame_counter) - sizeof(link_frame_counter.header),
               error = kThreadError_Parse);

  // MLE Frame Counter
  MleFrameCounterTlv mle_frame_counter;
  if (FindTlv(message, Tlv::kTypeMleFrameCounter, &mle_frame_counter, sizeof(mle_frame_counter)) ==
      kThreadError_None) {
    VerifyOrExit(mle_frame_counter.header.length == sizeof(mle_frame_counter) - sizeof(mle_frame_counter.header),
                 error = kThreadError_Parse);
  } else {
    mle_frame_counter.frame_counter = link_frame_counter.frame_counter;
  }

  uint8_t router_id;
  router_id = ADDR16_TO_ROUTER_ID(HostSwap16(source_address.address16));

  if (router_id != router_id_)
    neighbor = &routers_[router_id];
  else
    VerifyOrExit((neighbor = FindChild(&mac_addr)) != NULL, error = kThreadError_Error);

  // verify response
  VerifyOrExit(memcmp(challenge_, response.response, sizeof(challenge_)) == 0 ||
               memcmp(neighbor->pending.challenge, response.response, sizeof(neighbor->pending.challenge)) == 0,
               error = kThreadError_Error);

  switch (device_state_) {
    case kDeviceStateDetached: {
      // Address16
      Address16Tlv address16;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeAddress16, &address16, sizeof(address16)));
      VerifyOrExit(address16.header.length == sizeof(address16) - sizeof(address16.header),
                   error = kThreadError_Parse);
      VerifyOrExit(GetAddress16() == HostSwap16(address16.address16), error = kThreadError_Drop);

      // Route
      RouteTlv route;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeRoute, &route, sizeof(route)));
      VerifyOrExit(route.header.length <= sizeof(route) - sizeof(route.header), error = kThreadError_Parse);
      SuccessOrExit(error = ProcessRouteTlv(&route));

      // Leader Data
      LeaderDataTlv leader_data;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
      VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
                   error = kThreadError_Parse);
      leader_data_.partition_id = HostSwap32(leader_data.leader_data.partition_id);
      leader_data_.weighting = leader_data.leader_data.weighting;
      leader_data_.leader_router_id = leader_data.leader_data.leader_router_id;

      // Network Data
      NetworkDataTlv network_data;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeNetworkData, &network_data, sizeof(network_data)));
      SuccessOrExit(error = network_data_->SetNetworkData(leader_data.leader_data.version,
                                                          leader_data.leader_data.stable_version,
                                                          (device_mode_ & kModeFullNetworkData) == 0,
                                                          network_data.network_data, network_data.header.length));

      if (leader_data_.leader_router_id == ADDR16_TO_ROUTER_ID(GetAddress16()))
        SetStateLeader(GetAddress16());
      else
        SetStateRouter(GetAddress16());
      break;
    }

    case kDeviceStateChild: {
      routers_[router_id].link_quality_out = 3;
      routers_[router_id].link_quality_in = 3;
      break;
    }

    case kDeviceStateRouter:
    case kDeviceStateLeader: {
      // Leader Data
      LeaderDataTlv leader_data;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
      VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
                   error = kThreadError_Parse);

      LeaderData *local_leader_data;
      local_leader_data = GetLeaderData();
      VerifyOrExit(HostSwap32(leader_data.leader_data.partition_id) == local_leader_data->partition_id, ;);

      // RSSI
      RssiTlv rssi;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeRssi, &rssi, sizeof(rssi)));
      VerifyOrExit(rssi.header.length == sizeof(rssi) - sizeof(rssi.header), error = kThreadError_Parse);
      routers_[router_id].link_quality_out = 3;
      routers_[router_id].link_quality_in = 3;

      // update routing table
      if (router_id != router_id_ && routers_[router_id].nexthop == kMaxRouterId) {
        routers_[router_id].nexthop = router_id;
        ResetAdvertiseInterval();
      }

      break;
    }
  }

  // finish link synchronization
  memcpy(&neighbor->mac_addr, &mac_addr, sizeof(neighbor->mac_addr));
  neighbor->valid.address16 = HostSwap16(source_address.address16);
  neighbor->valid.link_frame_counter = HostSwap32(link_frame_counter.frame_counter);
  neighbor->valid.mle_frame_counter = HostSwap32(mle_frame_counter.frame_counter);
  neighbor->last_heard = Timer::GetNow();
  neighbor->mode = kModeFFD | kModeRxOnWhenIdle | kModeFullNetworkData;
  neighbor->state = Neighbor::kStateValid;
  assert(key_sequence == key_manager_->GetCurrentKeySequence() ||
         key_sequence == key_manager_->GetPreviousKeySequence());
  neighbor->previous_key = key_sequence == key_manager_->GetPreviousKeySequence();

  if (request) {
    // Challenge
    ChallengeTlv challenge;
    SuccessOrExit(error = FindTlv(message, Tlv::kTypeChallenge, &challenge, sizeof(challenge)));
    VerifyOrExit(challenge.header.length == sizeof(challenge.challenge), error = kThreadError_Parse);

    // TLV Request
    TlvRequestTlv tlv_request;
    if (FindTlv(message, Tlv::kTypeTlvRequest, &tlv_request, sizeof(tlv_request)) == kThreadError_None)
      VerifyOrExit(tlv_request.header.length <= sizeof(tlv_request.tlvs), error = kThreadError_Parse);
    else
      tlv_request.header.length = 0;

    SuccessOrExit(error = SendLinkAccept(message_info, neighbor, &tlv_request, &challenge));
  }

exit:
  return error;
}

ThreadError MleRouter::SendLinkReject(Ip6Address *destination) {
  ThreadError error = kThreadError_None;
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandLinkReject));
  SuccessOrExit(error = AppendStatus(message, StatusTlv::kError));

  SuccessOrExit(error = SendMessage(message, destination));

  dprintf("Sent link reject\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError MleRouter::HandleLinkReject(Message *message, const Ip6MessageInfo *message_info) {
  dprintf("Received link reject\n");

  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  return kThreadError_None;
}

Child *MleRouter::NewChild() {
  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state == Neighbor::kStateInvalid)
      return &children_[i];
  }
  return NULL;
}

Child *MleRouter::FindChild(uint16_t rloc16) {
  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state != Neighbor::kStateInvalid &&
        children_[i].valid.address16 == rloc16)
      return &children_[i];
  }
  return NULL;
}

Child *MleRouter::FindChild(const MacAddr64 *address) {
  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state != Neighbor::kStateInvalid &&
        memcmp(&children_[i].mac_addr, address, sizeof(children_[i].mac_addr)) == 0)
      return &children_[i];
  }
  return NULL;
}

uint8_t MleRouter::GetLinkCost(uint8_t router_id) {
  uint8_t rval;

  assert(router_id <= kMaxRouterId);

  VerifyOrExit(router_id != router_id_ &&
               router_id != kMaxRouterId &&
               routers_[router_id].state == Neighbor::kStateValid,
               rval = kMaxRouteCost);

  rval = routers_[router_id].link_quality_in;
  if (rval > routers_[router_id].link_quality_out)
    rval = routers_[router_id].link_quality_out;
  rval = LQI_TO_COST(rval);

exit:
  return rval;
}

ThreadError MleRouter::ProcessRouteTlv(RouteTlv *route) {
  ThreadError error = kThreadError_None;
  int8_t diff = route->sequence - router_id_sequence_;

  // check for newer route data
  if (diff > 0 || device_state_ == kDeviceStateDetached) {
    router_id_sequence_ = route->sequence;
    for (int i = 0; i < kMaxRouterId; i++) {
      bool old = routers_[i].allocated;
      routers_[i].allocated = (route->router_mask[i / 8] & (0x80 >> (i % 8))) != 0;
      if (old && !routers_[i].allocated) {
        routers_[i].nexthop = kMaxRouterId;
        address_resolver_->Remove(i);
      }
    }
    if (GetDeviceState() == kDeviceStateRouter && !routers_[router_id_].allocated) {
      BecomeDetached();
      ExitNow(error = kThreadError_NoRoute);
    }
    routers_[GetLeaderId()].last_heard = Timer::GetNow();
  }

exit:
  return error;
}

ThreadError MleRouter::HandleAdvertisement(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  // Source Address
  SourceAddressTlv source_address;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeSourceAddress, &source_address, sizeof(source_address)));
  VerifyOrExit(source_address.header.length == sizeof(source_address) - sizeof(source_address.header),
               error = kThreadError_Parse);

  // Remove stale neighbors
  Neighbor *neighbor;
  if ((neighbor = GetNeighbor(&mac_addr)) != NULL &&
      neighbor->valid.address16 != HostSwap16(source_address.address16)) {
    neighbor->state = Neighbor::kStateInvalid;
  }

  // Leader Data
  LeaderDataTlv leader_data;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
  VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
               error = kThreadError_Parse);

  dprintf("Received advertisement from %04x\n", HostSwap16(source_address.address16));

  LeaderData *local_leader_data;
  local_leader_data = GetLeaderData();

  uint32_t peer_partition_id;
  peer_partition_id = HostSwap32(leader_data.leader_data.partition_id);
  if (peer_partition_id != local_leader_data->partition_id) {
    dprintf("different partition! %d %d %d %d\n",
            leader_data.leader_data.weighting, peer_partition_id,
            local_leader_data->weighting, local_leader_data->partition_id);
    if ((leader_data.leader_data.weighting > local_leader_data->weighting) ||
        (leader_data.leader_data.weighting == local_leader_data->weighting &&
         peer_partition_id > local_leader_data->partition_id)) {
      dprintf("trying to migrate\n");
      BecomeChild(kJoinBetterPartition);
    }
    ExitNow(error = kThreadError_Drop);
  } else if (leader_data.leader_data.leader_router_id != GetLeaderId()) {
    BecomeDetached();
    ExitNow(error = kThreadError_Drop);
  }

  // XXX: what to do if partition id is the same for two different partitions?

  VerifyOrExit(ADDR16_TO_CHILD_ID(HostSwap16(source_address.address16)) == 0, ;);

  // Route Data
  RouteTlv route;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeRoute, &route, sizeof(route)));
  VerifyOrExit(route.header.length <= sizeof(route) - sizeof(route.header), error = kThreadError_Parse);

  if ((GetDeviceState() == kDeviceStateChild && memcmp(&parent_.mac_addr, &mac_addr, sizeof(parent_.mac_addr)) == 0) ||
      GetDeviceState() == kDeviceStateRouter || GetDeviceState() == kDeviceStateLeader) {
    SuccessOrExit(error = ProcessRouteTlv(&route));
  }

  uint8_t router_id;
  router_id = ADDR16_TO_ROUTER_ID(HostSwap16(source_address.address16));

  Router *router;
  router = NULL;
  switch (GetDeviceState()) {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
      ExitNow();
      break;

    case kDeviceStateChild:
      uint8_t router_count;
      router_count = 0;
      for (int i = 0; i < kMaxRouterId; i++)
        router_count += routers_[i].allocated;

      if (router_count < router_upgrade_threshold_) {
        BecomeRouter();
        ExitNow();
      }

      router = &parent_;
      if (memcmp(&router->mac_addr, &mac_addr, sizeof(router->mac_addr)) == 0) {
        if (router->valid.address16 != HostSwap16(source_address.address16)) {
          SetStateDetached();
          ExitNow(error = kThreadError_NoRoute);
        }
      } else {
        router = &routers_[router_id];
        if (router->state != Neighbor::kStateValid) {
          memcpy(&router->mac_addr, &mac_addr, sizeof(router->mac_addr));
          router->state = Neighbor::kStateLinkRequest;
          router->previous_key = false;
          SendLinkRequest(router);
          ExitNow(error = kThreadError_NoRoute);
        }
      }

      router->last_heard = Timer::GetNow();
      router->link_quality_in =
          LinkMarginToQuality(reinterpret_cast<ThreadMessageInfo*>(message_info->link_info)->link_margin);

      ExitNow();
      break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
      router = &routers_[router_id];

      // router is not in list, reject
      if (!router->allocated)
        ExitNow(error = kThreadError_NoRoute);

      // Send link request if no link to router
      if (router->state != Neighbor::kStateValid) {
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
          LinkMarginToQuality(reinterpret_cast<ThreadMessageInfo*>(message_info->link_info)->link_margin);
      break;
  }

  // update routes
  bool update;
  do {
    update = false;

    for (int i = 0, route_count = 0; i < kMaxRouterId; i++) {
      if ((route.router_mask[i / 8] & (0x80 >> (i % 8))) == 0)
        continue;

      if (routers_[i].allocated == false) {
        route_count++;
        continue;
      }

      if (i == router_id_) {
        uint8_t lqi = ROUTE_DATA_IN_LQI(route.route_data[route_count]);
        if (routers_[router_id].link_quality_out != lqi) {
          routers_[router_id].link_quality_out = lqi;
          update = true;
        }
      } else {
        uint8_t old_next_hop = routers_[i].nexthop;

        uint8_t cost = (i == router_id) ? 0 : ROUTE_DATA_COST(route.route_data[route_count]);
        if (i != router_id && cost == 0 && routers_[i].nexthop == router_id) {
          // route nexthop is neighbor, but neighbor no longer has route
          ResetAdvertiseInterval();
          routers_[i].nexthop = kMaxRouterId;
          routers_[i].cost = 0;
          routers_[i].last_heard = Timer::GetNow();
        } else if (routers_[i].nexthop == kMaxRouterId || routers_[i].nexthop == router_id) {
          // route has no nexthop or nexthop is neighbor
          uint8_t new_cost = cost + GetLinkCost(router_id);
          if (i == router_id) {
            if (routers_[i].nexthop == kMaxRouterId)
              ResetAdvertiseInterval();
            routers_[i].nexthop = router_id;
            routers_[i].cost = 0;
            // routers_[i].last_heard = Timer::GetNow();
          } else if (new_cost <= kMaxRouteCost) {
            if (routers_[i].nexthop == kMaxRouterId)
              ResetAdvertiseInterval();
            routers_[i].nexthop = router_id;
            routers_[i].cost = cost;
            // routers_[i].last_heard = Timer::GetNow();
          } else if (routers_[i].nexthop != kMaxRouterId) {
            ResetAdvertiseInterval();
            routers_[i].nexthop = kMaxRouterId;
            routers_[i].cost = 0;
            routers_[i].last_heard = Timer::GetNow();
          }
        } else {
          uint8_t cur_cost = routers_[i].cost + GetLinkCost(routers_[i].nexthop);
          uint8_t new_cost = cost + GetLinkCost(router_id);
          if (new_cost < cur_cost) {
            routers_[i].nexthop = router_id;
            routers_[i].cost = cost;
            // routers_[i].last_heard = Timer::GetNow();
          } else if (new_cost == cur_cost && i == router_id) {
            routers_[i].nexthop = router_id;
            routers_[i].cost = cost;
            // routers_[i].last_heard = Timer::GetNow();
          }
        }

        update |= routers_[i].nexthop != old_next_hop;
      }

      route_count++;
    }
  } while (update);

#if 1
  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].allocated == false || routers_[i].nexthop == kMaxRouterId)
      continue;
    dprintf("%x: %x %d %d %d %d\n", ROUTER_ID_TO_ADDR16(i), ROUTER_ID_TO_ADDR16(routers_[i].nexthop),
            routers_[i].cost, GetLinkCost(i), routers_[i].link_quality_in, routers_[i].link_quality_out);
  }
#endif

exit:
  return error;
}

ThreadError MleRouter::HandleParentRequest(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Child *child;

  dprintf("Received parent request\n");

  // A Router MUST NOT send an MLE Parent Response if:

  // 1. It has no available Child capacity (if Max Child Count minus
  // Child Count would be equal to zero)
  // ==> verified below when allocating a child entry

  // 2. It is disconnected from its Partition (that is, it has not
  // received an updated ID sequence number within LEADER_TIMEOUT
  // seconds
  VerifyOrExit((Timer::GetNow() - routers_[GetLeaderId()].last_heard) < (network_id_timeout_ * 1000U),
               error = kThreadError_Drop);

  // 3. Its current routing path cost to the Leader is infinite.
  VerifyOrExit(routers_[GetLeaderId()].nexthop != kMaxRouterId, error = kThreadError_Drop);

  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  // Version
  VersionTlv version;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeVersion, &version, sizeof(version)));
  VerifyOrExit(version.header.length == sizeof(version) - sizeof(version.header) &&
               version.version == HostSwap16(1), error = kThreadError_Parse);

  // Scan Mask
  ScanMaskTlv scan_mask;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeScanMask, &scan_mask, sizeof(scan_mask)));
  VerifyOrExit(scan_mask.header.length == sizeof(scan_mask) - sizeof(scan_mask.header),
               error = kThreadError_Parse);

  switch (GetDeviceState()) {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
      ExitNow();
      break;
    case kDeviceStateChild:
      VerifyOrExit((scan_mask.scan_mask & ScanMaskTlv::kChildScan) != 0, ;);
      break;
    case kDeviceStateRouter:
    case kDeviceStateLeader:
      VerifyOrExit((scan_mask.scan_mask & ScanMaskTlv::kRouterScan) != 0, ;);
      break;
  }

  VerifyOrExit((child = FindChild(&mac_addr)) != NULL || (child = NewChild()) != NULL, ;);
  memset(child, 0, sizeof(*child));

  // Challenge
  ChallengeTlv challenge;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeChallenge, &challenge, sizeof(challenge)));
  VerifyOrExit(challenge.header.length <= sizeof(challenge.challenge), error = kThreadError_Parse);

  // MAC Address
  memcpy(&child->mac_addr, &mac_addr, sizeof(child->mac_addr));

  child->state = Neighbor::kStateParentRequest;
  child->frame_pending = false;
  child->data_request = false;
  child->previous_key = false;
  child->rssi = reinterpret_cast<ThreadMessageInfo*>(message_info->link_info)->link_margin;
  child->timeout = 2 * kParentRequestChildTimeout * 1000U;
  SuccessOrExit(error = SendParentResponse(child, &challenge));

exit:
  return error;
}

void MleRouter::HandleStateUpdateTimer(void *context) {
  MleRouter *obj = reinterpret_cast<MleRouter*>(context);
  obj->HandleStateUpdateTimer();
}

void MleRouter::HandleStateUpdateTimer() {
  uint8_t leader_id = GetLeaderId();
  switch (GetDeviceState()) {
    case kDeviceStateDisabled:
      assert(false);
      break;
    case kDeviceStateDetached:
      SetStateDetached();
      BecomeChild(kJoinAnyPartition);
      ExitNow();
      break;
    case kDeviceStateChild:
    case kDeviceStateRouter:
      // verify path to leader
      dprintf("network id timeout = %d\n", (Timer::GetNow() - routers_[leader_id].last_heard) / 1000);
      if ((Timer::GetNow() - routers_[leader_id].last_heard) >= (network_id_timeout_ * 1000U))
        BecomeChild(kJoinSamePartition);
      break;
    case kDeviceStateLeader:
      // update router id sequence
      if ((Timer::GetNow() - routers_[leader_id].last_heard) >= (kRouterIdSequencePeriod * 1000U)) {
        router_id_sequence_++;
        routers_[leader_id].last_heard = Timer::GetNow();
      }
      break;
  }

  // update children state
  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state == Neighbor::kStateInvalid)
      continue;
    if ((Timer::GetNow() - children_[i].last_heard) >= children_[i].timeout * 1000U)
      children_[i].state = Neighbor::kStateInvalid;
  }

  // update router state
  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].state != Neighbor::kStateInvalid) {
      if ((Timer::GetNow() - routers_[i].last_heard) >= kMaxNeighborAge * 1000U) {
        routers_[i].state = Neighbor::kStateInvalid;
        routers_[i].nexthop = kMaxRouterId;
        routers_[i].link_quality_in = 0;
        routers_[i].link_quality_out = 0;
      }
    }

    if (GetDeviceState() == kDeviceStateLeader) {
      if (routers_[i].allocated) {
        if (routers_[i].nexthop == kMaxRouterId &&
            (Timer::GetNow() - routers_[i].last_heard) >= kMaxLeaderToRouterTimeout * 1000U)
          ReleaseRouterId(i);
      } else if (routers_[i].reclaim_delay) {
        if ((Timer::GetNow() - routers_[i].last_heard) >= kRouterIdReuseDelay * 1000U)
          routers_[i].reclaim_delay = false;
      }
    }
  }

  state_update_timer_.Start(1000);

exit:
  {}
}

ThreadError MleRouter::SendParentResponse(Child *child, ChallengeTlv *challenge) {
  ThreadError error = kThreadError_None;
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandParentResponse));
  SuccessOrExit(error = AppendSourceAddress(message));
  SuccessOrExit(error = AppendLeaderData(message));
  SuccessOrExit(error = AppendLinkFrameCounter(message));
  SuccessOrExit(error = AppendMleFrameCounter(message));
  SuccessOrExit(error = AppendResponse(message, challenge->challenge, challenge->header.length));
  for (uint8_t i = 0; i < sizeof(child->pending.challenge); i++)
    child->pending.challenge[i] = Random::Get();
  SuccessOrExit(error = AppendChallenge(message, child->pending.challenge, sizeof(child->pending.challenge)));
  SuccessOrExit(error = AppendRssi(message, child->rssi));
  SuccessOrExit(error = AppendConnectivity(message));
  SuccessOrExit(error = AppendVersion(message));

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xfe80);
  memcpy(destination.s6_addr + 8, &child->mac_addr, sizeof(child->mac_addr));
  destination.s6_addr[8] ^= 0x2;
  SuccessOrExit(error = SendMessage(message, &destination));

  dprintf("Sent Parent Response\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return kThreadError_None;
}

ThreadError MleRouter::UpdateChildAddresses(Ip6AddressTlv *tlv, Child *child) {
  uint8_t *cur = reinterpret_cast<uint8_t*>(tlv->address);
  uint8_t *end = cur + tlv->header.length;
  uint8_t count = 0;

  memset(&child->ip6_address, 0, sizeof(child->ip6_address));

  while (cur < end) {
    AddressRegistrationEntry *entry = reinterpret_cast<AddressRegistrationEntry*>(cur);
    if (entry->control & entry->kCompressed) {
      Context context;
      // xxx check if context id exists
      network_data_->GetContext(entry->control & entry->kContextIdMask, &context);
      memcpy(&child->ip6_address[count], context.prefix, (context.prefix_length+7)/8);
      memcpy(child->ip6_address[count].s6_addr + 8, entry->iid, sizeof(entry->iid));
      cur += sizeof(entry->control) + sizeof(entry->iid);
    } else {
      memcpy(&child->ip6_address[count], &entry->ip6_address, sizeof(child->ip6_address[count]));
      cur += sizeof(entry->control) + sizeof(entry->ip6_address);
    }
    count++;
  }

  return kThreadError_None;
}

ThreadError MleRouter::HandleChildIdRequest(Message *message, const Ip6MessageInfo *message_info,
                                            uint32_t key_sequence) {
  ThreadError error = kThreadError_None;

  dprintf("Received Child ID Request\n");

  // Find Child
  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  Child *child;
  VerifyOrExit((child = FindChild(&mac_addr)) != NULL, ;);

  // Response
  ResponseTlv response;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeResponse, &response, sizeof(response)));
  VerifyOrExit(response.header.length == sizeof(response.response) &&
               memcmp(response.response, child->pending.challenge, sizeof(response.response)) == 0, ;);

  // Link-Layer Frame Counter
  LinkFrameCounterTlv link_frame_counter;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeLinkFrameCounter, &link_frame_counter, sizeof(link_frame_counter)));
  VerifyOrExit(link_frame_counter.header.length == sizeof(link_frame_counter) - sizeof(link_frame_counter.header),
               error = kThreadError_Parse);

  // MLE Frame Counter
  MleFrameCounterTlv mle_frame_counter;
  if (FindTlv(message, Tlv::kTypeMleFrameCounter, &mle_frame_counter, sizeof(mle_frame_counter)) ==
      kThreadError_None) {
    VerifyOrExit(mle_frame_counter.header.length == sizeof(mle_frame_counter) - sizeof(mle_frame_counter.header),
                 error = kThreadError_Parse);
  } else {
    mle_frame_counter.frame_counter = link_frame_counter.frame_counter;
  }

  // Mode
  ModeTlv mode;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeMode, &mode, sizeof(mode)));
  VerifyOrExit(mode.header.length == sizeof(mode) - sizeof(mode.header), error = kThreadError_Parse);

  // Timeout
  TimeoutTlv timeout;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeTimeout, &timeout, sizeof(timeout)));
  VerifyOrExit(timeout.header.length == sizeof(timeout) - sizeof(timeout.header),
               error = kThreadError_Parse);

  // Ip6 Address
  Ip6AddressTlv address;
  address.header.length = 0;
  if ((mode.mode & kModeFFD) == 0) {
    SuccessOrExit(error = FindTlv(message, Tlv::kTypeIp6Address, &address, sizeof(address)));
    VerifyOrExit(address.header.length <= sizeof(address) - sizeof(address.header), error = kThreadError_Parse);
  }

  // TLV Request
  TlvRequestTlv tlv_request;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeTlvRequest, &tlv_request, sizeof(tlv_request)));
  VerifyOrExit(tlv_request.header.length <= sizeof(tlv_request.tlvs) &&
               tlv_request.header.length <= sizeof(child->request_tlvs), error = kThreadError_Parse);

  // Remove from router table
  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].state != Neighbor::kStateInvalid &&
        memcmp(&routers_[i].mac_addr, &mac_addr, sizeof(routers_[i].mac_addr)) == 0) {
      routers_[i].state = Neighbor::kStateInvalid;
      break;
    }
  }

  child->state = Neighbor::kStateChildIdRequest;
  child->last_heard = Timer::GetNow();
  child->valid.link_frame_counter = HostSwap32(link_frame_counter.frame_counter);
  child->valid.mle_frame_counter = HostSwap32(mle_frame_counter.frame_counter);
  child->mode = mode.mode;
  child->timeout = HostSwap32(timeout.timeout);
  if (mode.mode & kModeFullNetworkData)
    child->network_data_version = GetLeaderData()->version;
  else
    child->network_data_version = GetLeaderData()->stable_version;

  UpdateChildAddresses(&address, child);

  assert(key_sequence == key_manager_->GetCurrentKeySequence() ||
         key_sequence == key_manager_->GetPreviousKeySequence());
  child->previous_key = key_sequence == key_manager_->GetPreviousKeySequence();
  for (uint8_t i = 0; i < tlv_request.header.length; i++)
    child->request_tlvs[i] = tlv_request.tlvs[i];
  for (uint8_t i = tlv_request.header.length; i < sizeof(child->request_tlvs); i++)
    child->request_tlvs[i] = Tlv::kTypeInvalid;

  switch (GetDeviceState()) {
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

ThreadError MleRouter::HandleChildUpdateRequest(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  uint8_t tlvs[7];
  uint8_t tlvs_length = 0;

  dprintf("Received Child Update Request\n");

  // Find Child
  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  Child *child;
  child = FindChild(&mac_addr);
  if (child == NULL) {
    tlvs[tlvs_length++] = Tlv::kTypeStatus;
    SendChildUpdateResponse(NULL, message_info, tlvs, tlvs_length, NULL);
    ExitNow();
  }

  tlvs[tlvs_length++] = Tlv::kTypeSourceAddress;
  tlvs[tlvs_length++] = Tlv::kTypeLeaderData;

  // Mode
  ModeTlv mode;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeMode, &mode, sizeof(mode)));
  VerifyOrExit(mode.header.length == sizeof(mode) - sizeof(mode.header), error = kThreadError_Parse);
  child->mode = mode.mode;
  tlvs[tlvs_length++] = Tlv::kTypeMode;

  // Challenge
  ChallengeTlv challenge;
  if (FindTlv(message, Tlv::kTypeChallenge, &challenge, sizeof(challenge)) == kThreadError_None) {
    VerifyOrExit(challenge.header.length <= sizeof(challenge.challenge), error = kThreadError_Parse);
    tlvs[tlvs_length++] = Tlv::kTypeResponse;
  }

  // Ip6 Address TLV
  Ip6AddressTlv address;
  if (FindTlv(message, Tlv::kTypeIp6Address, &address, sizeof(address)) == kThreadError_None) {
    VerifyOrExit(address.header.length <= sizeof(address) - sizeof(address.header), error = kThreadError_Parse);
    UpdateChildAddresses(&address, child);
    tlvs[tlvs_length++] = Tlv::kTypeIp6Address;
  }

  // Leader Data
  LeaderDataTlv leader_data;
  if (FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)) == kThreadError_None) {
    VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
                 error = kThreadError_Parse);
    if (child->mode & kModeFullNetworkData)
      child->network_data_version = leader_data.leader_data.version;
    else
      child->network_data_version = leader_data.leader_data.stable_version;
  }

  // Timeout
  TimeoutTlv timeout;
  if (FindTlv(message, Tlv::kTypeTimeout, &timeout, sizeof(timeout)) == kThreadError_None) {
    VerifyOrExit(timeout.header.length == sizeof(timeout) - sizeof(timeout.header),
                 error = kThreadError_Parse);
    child->timeout = HostSwap32(timeout.timeout);
    tlvs[tlvs_length++] = Tlv::kTypeTimeout;
  }

  child->last_heard = Timer::GetNow();

  SendChildUpdateResponse(child, message_info, tlvs, tlvs_length, &challenge);

exit:
  return error;
}

ThreadError MleRouter::HandleNetworkDataUpdateRouter() {
  VerifyOrExit(device_state_ == kDeviceStateRouter || device_state_ == kDeviceStateLeader, ;);

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xff02);
  destination.s6_addr16[7] = HostSwap16(0x0001);

  static const uint8_t tlvs[] = {Mle::Tlv::kTypeLeaderData, Mle::Tlv::kTypeNetworkData};
  SendDataResponse(&destination, tlvs, sizeof(tlvs));

exit:
  return kThreadError_None;
}

ThreadError MleRouter::SendChildIdResponse(Child *child) {
  ThreadError error = kThreadError_None;
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandChildIdResponse));
  SuccessOrExit(error = AppendSourceAddress(message));
  SuccessOrExit(error = AppendLeaderData(message));

  child->valid.address16 = mesh_->GetAddress16() | next_child_id_;

  next_child_id_++;
  if (next_child_id_ >= 512)
    next_child_id_ = 1;

  SuccessOrExit(error = AppendAddress16(message, child->valid.address16));

  for (uint8_t i = 0; i < sizeof(child->request_tlvs); i++) {
    switch (child->request_tlvs[i]) {
      case Tlv::kTypeNetworkData:
        SuccessOrExit(error = AppendNetworkData(message, (child->mode & kModeFullNetworkData) == 0));
        break;
      case Tlv::kTypeRoute:
        SuccessOrExit(error = AppendRoute(message));
        break;
    }
  }

  if ((child->mode & kModeFFD) == 0)
    SuccessOrExit(error = AppendChildAddresses(message, child));

  child->state = Neighbor::kStateValid;

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xfe80);
  memcpy(destination.s6_addr + 8, &child->mac_addr, sizeof(child->mac_addr));
  destination.s6_addr[8] ^= 0x2;
  SuccessOrExit(error = SendMessage(message, &destination));

  dprintf("Sent Child ID Response\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return kThreadError_None;
}

ThreadError MleRouter::SendChildUpdateResponse(Child *child, const Ip6MessageInfo *message_info,
                                               const uint8_t *tlvs, uint8_t tlvs_length,
                                               const ChallengeTlv *challenge) {
  ThreadError error = kThreadError_None;
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandChildUpdateResponse));

  for (int i = 0; i < tlvs_length; i++) {
    switch (tlvs[i]) {
      case Tlv::kTypeStatus:
        SuccessOrExit(error = AppendStatus(message, StatusTlv::kError));
        break;
      case Tlv::kTypeIp6Address:
        SuccessOrExit(error = AppendIp6Address(message, child));
        break;
      case Tlv::kTypeLeaderData:
        SuccessOrExit(error = AppendLeaderData(message));
        break;
      case Tlv::kTypeMode:
        SuccessOrExit(error = AppendMode(message, child->mode));
        break;
      case Tlv::kTypeResponse:
        SuccessOrExit(error = AppendResponse(message, challenge->challenge, challenge->header.length));
        break;
      case Tlv::kTypeSourceAddress:
        SuccessOrExit(error = AppendSourceAddress(message));
        break;
      case Tlv::kTypeTimeout:
        SuccessOrExit(error = AppendTimeout(message, child->timeout));
        break;
    }
  }

  SuccessOrExit(error = SendMessage(message, &message_info->peer_addr));

  dprintf("Sent Child Update Response\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return kThreadError_None;
}

Child *MleRouter::GetChild(MacAddr16 address) {
  if (device_state_ == kDeviceStateChild)
    return Mle::GetChild(address);

  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state == Neighbor::kStateValid && children_[i].valid.address16 == address)
      return &children_[i];
  }
  return NULL;
}

Child *MleRouter::GetChild(const MacAddr64 *address) {
  if (device_state_ == kDeviceStateChild)
    return Mle::GetChild(address);

  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state == Neighbor::kStateValid &&
        memcmp(&children_[i].mac_addr, address, sizeof(children_[i].mac_addr)) == 0)
      return &children_[i];
  }
  return NULL;
}

Child *MleRouter::GetChild(const MacAddress *address) {
  if (device_state_ == kDeviceStateChild)
    return Mle::GetChild(address);

  switch (address->length) {
    case 2: return GetChild(address->address16);
    case 8: return GetChild(&address->address64);
  }
  return NULL;
}

int MleRouter::GetChildIndex(const Child *child) {
  if (device_state_ == kDeviceStateChild)
    return Mle::GetChildIndex(child);

  return child - children_;
}

Child *MleRouter::GetChildren(uint8_t *num_children) {
  if (device_state_ == kDeviceStateChild)
    return Mle::GetChildren(num_children);

  if (num_children != NULL)
    *num_children = kMaxChildren;
  return children_;
}

Neighbor *MleRouter::GetNeighbor(uint16_t address) {
  Neighbor *rval = NULL;

  if (address == MacFrame::kShortAddrBroadcast || address == MacFrame::kShortAddrInvalid)
    ExitNow();

  if (device_state_ == kDeviceStateChild && (rval = Mle::GetNeighbor(address)) != NULL)
    ExitNow();

  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state == Neighbor::kStateValid && children_[i].valid.address16 == address)
      ExitNow(rval = &children_[i]);
  }
  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].state == Neighbor::kStateValid && routers_[i].valid.address16 == address)
      ExitNow(rval = &routers_[i]);
  }

exit:
  return rval;
}

Neighbor *MleRouter::GetNeighbor(const MacAddr64 *address) {
  Neighbor *rval = NULL;

  if (device_state_ == kDeviceStateChild && (rval = Mle::GetNeighbor(address)) != NULL)
    ExitNow();

  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state == Neighbor::kStateValid &&
        memcmp(&children_[i].mac_addr, address, sizeof(children_[i].mac_addr)) == 0)
      ExitNow(rval = &children_[i]);
  }

  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].state == Neighbor::kStateValid &&
        memcmp(&routers_[i].mac_addr, address, sizeof(routers_[i].mac_addr)) == 0)
      ExitNow(rval = &routers_[i]);
  }

exit:
  return rval;
}

Neighbor *MleRouter::GetNeighbor(const MacAddress *address) {
  Neighbor *rval = NULL;

  switch (address->length) {
    case 2: rval = GetNeighbor(address->address16); break;
    case 8: rval = GetNeighbor(&address->address64); break;
    default: break;
  }

  return rval;
}

Neighbor *MleRouter::GetNeighbor(const Ip6Address *address) {
  Neighbor *rval = NULL;

  if (address->IsLinkLocal()) {
    MacAddress macaddr;
    if (address->s6_addr16[4] == HostSwap16(0x0000) &&
        address->s6_addr16[5] == HostSwap16(0x00ff) &&
        address->s6_addr16[6] == HostSwap16(0xfe00)) {
      macaddr.length = 2;
      macaddr.address16 = HostSwap16(address->s6_addr16[7]);
    } else {
      macaddr.length = 8;
      memcpy(macaddr.address64.bytes, address->s6_addr + 8, sizeof(macaddr.address64));
      macaddr.address64.bytes[0] ^= 0x02;
    }
    ExitNow(rval = GetNeighbor(&macaddr));
  }

  Context context;
  if (network_data_->GetContext(*address, &context) != kThreadError_None)
    context.context_id = 0xff;

  for (int i = 0; i < kMaxChildren; i++) {
    Child *child = &children_[i];

    if (child->state != Neighbor::kStateValid)
      continue;

    if (context.context_id == 0 &&
        address->s6_addr16[4] == HostSwap16(0x0000) &&
        address->s6_addr16[5] == HostSwap16(0x00ff) &&
        address->s6_addr16[6] == HostSwap16(0xfe00) &&
        address->s6_addr16[7] == HostSwap16(child->valid.address16))
      ExitNow(rval = child);

    for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++) {
      if (memcmp(&child->ip6_address[j], address->s6_addr, sizeof(child->ip6_address[j])) == 0)
        ExitNow(rval = child);
    }
  }

  VerifyOrExit(context.context_id == 0, rval = NULL);

  for (int i = 0; i < kMaxRouterId; i++) {
    Router *router = &routers_[i];

    if (router->state != Neighbor::kStateValid)
      continue;

    if (address->s6_addr16[4] == HostSwap16(0x0000) &&
        address->s6_addr16[5] == HostSwap16(0x00ff) &&
        address->s6_addr16[6] == HostSwap16(0xfe00) &&
        address->s6_addr16[7] == HostSwap16(router->valid.address16)) {
      ExitNow(rval = router);
    }
  }

exit:
  return rval;
}

uint16_t MleRouter::GetNextHop(uint16_t destination) {
  if (device_state_ == kDeviceStateChild)
    return Mle::GetNextHop(destination);

  uint8_t nexthop = routers_[ADDR16_TO_ROUTER_ID(destination)].nexthop;

  if (nexthop == kMaxRouterId || routers_[nexthop].state == Neighbor::kStateInvalid)
    return MacFrame::kShortAddrInvalid;

  return ROUTER_ID_TO_ADDR16(nexthop);
}

uint8_t MleRouter::GetRouteCost(uint16_t rloc) {
  uint8_t router_id = ADDR16_TO_ROUTER_ID(rloc);
  uint8_t rval;

  VerifyOrExit(router_id < kMaxRouterId && routers_[router_id].nexthop != kMaxRouterId, rval = kMaxRouteCost);

  rval = routers_[router_id].cost;

exit:
  return rval;
}

uint8_t MleRouter::GetDeviceMode() const {
  return device_mode_;
}

uint8_t MleRouter::GetRouterIdSequence() const {
  return router_id_sequence_;
}

uint8_t MleRouter::GetLeaderWeight() const {
  return leader_weight_;
}

ThreadError MleRouter::SetLeaderWeight(uint8_t weight) {
  leader_weight_ = weight;
  return kThreadError_None;
}

ThreadError MleRouter::HandleMacDataRequest(const Child *child) {
  uint8_t tlvs[] = {Tlv::kTypeLeaderData, Tlv::kTypeNetworkData};

  VerifyOrExit(child->state == Neighbor::kStateValid && (child->mode & kModeRxOnWhenIdle) == 0, ;);

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xfe80);
  memcpy(destination.s6_addr + 8, &child->mac_addr, sizeof(child->mac_addr));
  destination.s6_addr[8] ^= 0x2;

  if (child->mode & kModeFullNetworkData) {
    if (child->network_data_version != network_data_->GetVersion())
      SendDataResponse(&destination, tlvs, sizeof(tlvs));
  } else {
    if (child->network_data_version != network_data_->GetStableVersion())
      SendDataResponse(&destination, tlvs, sizeof(tlvs));
  }

exit:
  return kThreadError_None;
}

Router *MleRouter::GetRouters(uint8_t *num_routers) {
  if (num_routers != NULL)
    *num_routers = kMaxRouterId;
  return routers_;
}

ThreadError MleRouter::CheckReachability(MacAddr16 meshsrc, MacAddr16 meshdst, const Ip6Header *ip6_header) {
  if (device_state_ == kDeviceStateChild)
    return Mle::CheckReachability(meshsrc, meshdst, ip6_header);

  if (meshdst == mesh_->GetAddress16()) {
    // mesh destination is this device
    if (netif_->IsAddress(&ip6_header->ip6_dst)) {
      // IPv6 destination is this device
      return kThreadError_None;
    } else if (GetNeighbor(&ip6_header->ip6_dst) != NULL) {
      // IPv6 destination is an RFD child
      return kThreadError_None;
    }
  } else if (ADDR16_TO_ROUTER_ID(meshdst) == router_id_) {
    // mesh destination is a child of this device
    if (GetChild(meshdst))
      return kThreadError_None;
  } else if (GetNextHop(meshdst) != MacFrame::kShortAddrInvalid) {
    // forwarding to another router and route is known
    return kThreadError_None;
  }

  Ip6Address dst;
  memcpy(&dst, GetMeshLocal16(), 14);
  dst.s6_addr16[7] = HostSwap16(meshsrc);
  Icmp6::SendError(&dst, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOROUTE, ip6_header);

  return kThreadError_Drop;
}

ThreadError MleRouter::SendAddressSolicit() {
  ThreadError error = kThreadError_None;

  socket_.Bind(NULL);

  for (int i = 0; i < sizeof(coap_token_); i++)
    coap_token_[i] = Random::Get();

  CoapMessage coap;
  coap.SetVersion(1);
  coap.SetType(CoapMessage::kTypeConfirmable);
  coap.SetCode(CoapMessage::kCodePost);
  coap.SetMessageId(++coap_message_id_);
  coap.SetToken(coap_token_, sizeof(coap_token_));
  coap.AppendUriPathOptions("a/as");
  coap.Finalize();

  Message *message;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = message->Append(coap.GetHeaderBytes(), coap.GetHeaderLength()));

  ThreadMacAddr64Tlv mac_addr64_tlv;
  mac_addr64_tlv.header.type = ThreadTlv::kTypeMacAddr64;
  mac_addr64_tlv.header.length = sizeof(mac_addr64_tlv) - sizeof(mac_addr64_tlv.header);
  memcpy(&mac_addr64_tlv.address, mesh_->GetAddress64(), sizeof(mac_addr64_tlv.address));
  SuccessOrExit(error = message->Append(&mac_addr64_tlv, sizeof(mac_addr64_tlv)));

  if (previous_router_id_ != kMaxRouterId) {
    ThreadRlocTlv rloc_tlv;
    rloc_tlv.header.type = ThreadTlv::kTypeRloc;
    rloc_tlv.header.length = sizeof(rloc_tlv) - sizeof(rloc_tlv.header);
    rloc_tlv.address = HostSwap16(ROUTER_ID_TO_ADDR16(previous_router_id_));
    SuccessOrExit(error = message->Append(&rloc_tlv, sizeof(rloc_tlv)));
  }

  Ip6MessageInfo message_info;
  memset(&message_info, 0, sizeof(message_info));
  SuccessOrExit(error = GetLeaderAddress(&message_info.peer_addr));
  message_info.peer_port = kCoapUdpPort;
  SuccessOrExit(error = socket_.SendTo(message, &message_info));

  dprintf("Sent address solicit to %04x\n", HostSwap16(message_info.peer_addr.s6_addr16[7]));

exit:
  return error;
}

ThreadError MleRouter::SendAddressRelease() {
  ThreadError error = kThreadError_None;

  socket_.Bind(NULL);

  for (int i = 0; i < sizeof(coap_token_); i++)
    coap_token_[i] = Random::Get();

  CoapMessage coap;
  coap.SetVersion(1);
  coap.SetType(CoapMessage::kTypeConfirmable);
  coap.SetCode(CoapMessage::kCodePost);
  coap.SetMessageId(++coap_message_id_);
  coap.SetToken(coap_token_, sizeof(coap_token_));
  coap.AppendUriPathOptions("a/ar");
  coap.Finalize();

  Message *message;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = message->Append(coap.GetHeaderBytes(), coap.GetHeaderLength()));

  ThreadRlocTlv rloc_tlv;
  rloc_tlv.header.type = ThreadTlv::kTypeRloc;
  rloc_tlv.header.length = sizeof(rloc_tlv) - sizeof(rloc_tlv.header);
  rloc_tlv.address = HostSwap16(ROUTER_ID_TO_ADDR16(router_id_));
  SuccessOrExit(error = message->Append(&rloc_tlv, sizeof(rloc_tlv)));

  ThreadMacAddr64Tlv mac_addr64_tlv;
  mac_addr64_tlv.header.type = ThreadTlv::kTypeMacAddr64;
  mac_addr64_tlv.header.length = sizeof(mac_addr64_tlv) - sizeof(mac_addr64_tlv.header);
  memcpy(&mac_addr64_tlv.address, mesh_->GetAddress64(), sizeof(mac_addr64_tlv.address));
  SuccessOrExit(error = message->Append(&mac_addr64_tlv, sizeof(mac_addr64_tlv)));

  ThreadMeshLocalIidTlv mesh_local_iid_tlv;
  mesh_local_iid_tlv.header.type = ThreadTlv::kTypeMeshLocalIid;
  mesh_local_iid_tlv.header.length = sizeof(mesh_local_iid_tlv) - sizeof(mesh_local_iid_tlv.header);
  // memcpy(mesh_local_iid_tlv.iid, context->netif_address.address.s6_addr + 8, sizeof(mesh_local_iid_tlv.iid));
  SuccessOrExit(error = message->Append(&mesh_local_iid_tlv, sizeof(mesh_local_iid_tlv)));

  Ip6MessageInfo message_info;
  memset(&message_info, 0, sizeof(message_info));
  SuccessOrExit(error = GetLeaderAddress(&message_info.peer_addr));
  message_info.peer_port = kCoapUdpPort;
  SuccessOrExit(error = socket_.SendTo(message, &message_info));

  dprintf("Sent address release\n");

exit:
  return error;
}

void MleRouter::RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info) {
  MleRouter *obj = reinterpret_cast<MleRouter*>(context);
  obj->RecvFrom(message, message_info);
}

void MleRouter::RecvFrom(Message *message, const Ip6MessageInfo *message_info) {
  CoapMessage coap;

  SuccessOrExit(coap.FromMessage(message));
  VerifyOrExit(coap.GetType() == CoapMessage::kTypeAcknowledgment &&
               coap.GetCode() == CoapMessage::kCodeChanged &&
               coap.GetMessageId() == coap_message_id_ &&
               coap.GetTokenLength() == sizeof(coap_token_) &&
               memcmp(coap_token_, coap.GetToken(NULL), sizeof(coap_token_)) == 0, ;);
  message->MoveOffset(coap.GetHeaderLength());

  dprintf("Received address reply\n");

  ThreadStatusTlv status_tlv;
  SuccessOrExit(FindTlv(message, ThreadTlv::kTypeStatus, &status_tlv, sizeof(status_tlv)));
  VerifyOrExit(status_tlv.header.length == sizeof(status_tlv) - sizeof(status_tlv.header) &&
               status_tlv.status == status_tlv.kSuccess, ;);

  ThreadRlocTlv rloc_tlv;
  SuccessOrExit(FindTlv(message, ThreadTlv::kTypeRloc, &rloc_tlv, sizeof(rloc_tlv)));
  VerifyOrExit(rloc_tlv.header.length == sizeof(rloc_tlv) - sizeof(rloc_tlv.header), ;);

  ThreadRouterMaskTlv router_mask_tlv;
  SuccessOrExit(FindTlv(message, ThreadTlv::kTypeRouterMask, &router_mask_tlv, sizeof(router_mask_tlv)));
  VerifyOrExit(router_mask_tlv.header.length == sizeof(router_mask_tlv) - sizeof(router_mask_tlv.header), ;);

  // assign short address
  router_id_ = ADDR16_TO_ROUTER_ID(HostSwap16(rloc_tlv.address));
  previous_router_id_ = router_id_;
  SuccessOrExit(SetStateRouter(ROUTER_ID_TO_ADDR16(router_id_)));
  routers_[router_id_].cost = 0;

  // copy router id information
  router_id_sequence_ = router_mask_tlv.router_id_sequence;
  for (int i = 0; i < kMaxRouterId; i++) {
    bool old = routers_[i].allocated;
    routers_[i].allocated = (router_mask_tlv.router_id_mask[i / 8] & (0x80 >> (i % 8))) != 0;
    if (old && !routers_[i].allocated)
      address_resolver_->Remove(i);
  }

  // send link request
  SendLinkRequest(NULL);
  ResetAdvertiseInterval();

  // send child id responses
  for (int i = 0; i < kMaxChildren; i++) {
    if (children_[i].state == Neighbor::kStateChildIdRequest)
      SendChildIdResponse(&children_[i]);
  }

exit:
  {}
}

void MleRouter::HandleAddressSolicit(void *context, CoapMessage *coap_message, Message *message,
                                     const Ip6MessageInfo *message_info) {
  MleRouter *obj = reinterpret_cast<MleRouter*>(context);
  obj->HandleAddressSolicit(coap_message, message, message_info);
}

void MleRouter::HandleAddressSolicit(CoapMessage *coap_message, Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Message *reply = NULL;

  VerifyOrExit(coap_message->GetType() == CoapMessage::kTypeConfirmable &&
               coap_message->GetCode() == CoapMessage::kCodePost, ;);

  dprintf("Received address solicit\n");

  ThreadMacAddr64Tlv mac_addr64_tlv;
  SuccessOrExit(error = FindTlv(message, ThreadTlv::kTypeMacAddr64, &mac_addr64_tlv, sizeof(mac_addr64_tlv)));
  VerifyOrExit(mac_addr64_tlv.header.length == sizeof(mac_addr64_tlv) - sizeof(mac_addr64_tlv.header),
               error = kThreadError_Parse);

  int router_id;
  router_id = -1;

  // see if allocation already exists
  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].allocated &&
        memcmp(&routers_[i].mac_addr, &mac_addr64_tlv.address, sizeof(routers_[i].mac_addr)) == 0) {
      router_id = i;
      goto found;
    }
  }

  ThreadRlocTlv rloc_tlv;
  if (FindTlv(message, ThreadTlv::kTypeRloc, &rloc_tlv, sizeof(rloc_tlv)) == kThreadError_None) {
    // specific Router ID requested
    VerifyOrExit(rloc_tlv.header.length == sizeof(rloc_tlv) - sizeof(rloc_tlv.header), error = kThreadError_Parse);
    router_id = ADDR16_TO_ROUTER_ID(HostSwap16(rloc_tlv.address));
    if (router_id >= kMaxRouterId) {
      // requested Router ID is out of range
      router_id = -1;
    } else if (routers_[router_id].allocated &&
               memcmp(&routers_[router_id].mac_addr, &mac_addr64_tlv.address, sizeof(routers_[router_id].mac_addr))) {
      // requested Router ID is allocated to another device
      router_id = -1;
    } else {
      router_id = AllocateRouterId(router_id);
    }
  }

  // allocate new router id
  if (router_id < 0)
    router_id = AllocateRouterId();
  else
    dprintf("router id requested and provided!\n");

  if (router_id >= 0)
    memcpy(&routers_[router_id].mac_addr, &mac_addr64_tlv.address, sizeof(routers_[router_id].mac_addr));
  else
    dprintf("router address unavailable!\n");

found:
  uint16_t message_id;
  message_id = coap_message->GetMessageId();

  uint8_t token_length;
  token_length = coap_message->GetTokenLength();

  uint8_t token[CoapMessage::kMaxTokenLength];
  memcpy(token, coap_message->GetToken(NULL), coap_message->GetTokenLength());

  coap_message->Init();
  coap_message->SetVersion(1);
  coap_message->SetType(CoapMessage::kTypeAcknowledgment);
  coap_message->SetCode(CoapMessage::kCodeChanged);
  coap_message->SetMessageId(message_id);
  coap_message->SetToken(token, token_length);
  coap_message->Finalize();

  VerifyOrExit((reply = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = reply->Append(coap_message->GetHeaderBytes(), coap_message->GetHeaderLength()));

  ThreadStatusTlv status_tlv;
  status_tlv.header.type = ThreadTlv::kTypeStatus;
  status_tlv.header.length = sizeof(status_tlv) - sizeof(status_tlv.header);
  status_tlv.status = (router_id < 0) ? status_tlv.kNoAddressAvailable : status_tlv.kSuccess;
  SuccessOrExit(error = reply->Append(&status_tlv, sizeof(status_tlv)));

  if (router_id >= 0) {
    rloc_tlv.header.type = ThreadTlv::kTypeRloc;
    rloc_tlv.header.length = sizeof(rloc_tlv) - sizeof(rloc_tlv.header);
    rloc_tlv.address = HostSwap16(ROUTER_ID_TO_ADDR16(router_id));
    SuccessOrExit(error = reply->Append(&rloc_tlv, sizeof(rloc_tlv)));

    ThreadRouterMaskTlv router_mask_tlv;
    router_mask_tlv.header.type = ThreadTlv::kTypeRouterMask;
    router_mask_tlv.header.length = sizeof(router_mask_tlv) - sizeof(router_mask_tlv.header);
    router_mask_tlv.router_id_sequence = router_id_sequence_;
    memset(router_mask_tlv.router_id_mask, 0, sizeof(router_mask_tlv.router_id_mask));
    for (int i = 0; i < kMaxRouterId; i++) {
      if (routers_[i].allocated)
        router_mask_tlv.router_id_mask[i / 8] |= 0x80 >> (i % 8);
    }
    SuccessOrExit(error = reply->Append(&router_mask_tlv, sizeof(router_mask_tlv)));
  }

  SuccessOrExit(error = coap_server_->SendMessage(reply, message_info));

  dprintf("Sent address reply\n");

exit:
  if (error != kThreadError_None && reply != NULL)
    Message::Free(reply);
}

void MleRouter::HandleAddressRelease(void *context, CoapMessage *coap_message, Message *message,
                                     const Ip6MessageInfo *message_info) {
  MleRouter *obj = reinterpret_cast<MleRouter*>(context);
  obj->HandleAddressRelease(coap_message, message, message_info);
}

void MleRouter::HandleAddressRelease(CoapMessage *coap_message, Message *message,
                                     const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Message *reply = NULL;

  VerifyOrExit(coap_message->GetType() == CoapMessage::kTypeConfirmable &&
               coap_message->GetCode() == CoapMessage::kCodePost, ;);

  dprintf("Received address release\n");

  ThreadRlocTlv rloc_tlv;
  SuccessOrExit(FindTlv(message, ThreadTlv::kTypeRloc, &rloc_tlv, sizeof(rloc_tlv)));
  VerifyOrExit(rloc_tlv.header.length == sizeof(rloc_tlv) - sizeof(rloc_tlv.header), ;);

  ThreadMacAddr64Tlv mac_addr64_tlv;
  SuccessOrExit(error = FindTlv(message, ThreadTlv::kTypeMacAddr64, &mac_addr64_tlv, sizeof(mac_addr64_tlv)));
  VerifyOrExit(mac_addr64_tlv.header.length == sizeof(mac_addr64_tlv) - sizeof(mac_addr64_tlv.header),
               error = kThreadError_Parse);

  uint8_t router_id;
  router_id = ADDR16_TO_ROUTER_ID(HostSwap16(rloc_tlv.address));

  Router *router;
  router = &routers_[router_id];

  VerifyOrExit(memcmp(&router->mac_addr, &mac_addr64_tlv.address, sizeof(router->mac_addr)) == 0, ;);

  ReleaseRouterId(router_id);

  uint16_t message_id;
  message_id = coap_message->GetMessageId();

  uint8_t token_length;
  token_length = coap_message->GetTokenLength();

  uint8_t token[CoapMessage::kMaxTokenLength];
  memcpy(token, coap_message->GetToken(NULL), coap_message->GetTokenLength());

  coap_message->Init();
  coap_message->SetVersion(1);
  coap_message->SetType(CoapMessage::kTypeAcknowledgment);
  coap_message->SetCode(CoapMessage::kCodeChanged);
  coap_message->SetMessageId(message_id);
  coap_message->SetToken(token, token_length);
  coap_message->Finalize();

  VerifyOrExit((reply = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = reply->Append(coap_message->GetHeaderBytes(), coap_message->GetHeaderLength()));

  SuccessOrExit(error = coap_server_->SendMessage(reply, message_info));

  dprintf("Sent address release response\n");

exit:
  if (error != kThreadError_None && reply != NULL)
    Message::Free(reply);
}

ThreadError MleRouter::AppendConnectivity(Message *message) {
  ThreadError error;
  ConnectivityTlv tlv;

  // XXX: what if device is end-device?  it doesn't have bi-directional LQ to compute cost to leader
  tlv.header.type = Tlv::kTypeConnectivity;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);

  tlv.max_child_count = kMaxChildren;

  // compute number of children
  tlv.child_count = 0;
  for (int i = 0; i < kMaxChildren; i++)
    tlv.child_count += children_[i].state == Neighbor::kStateValid;

  // compute leader cost and link qualities
  tlv.link_quality_1 = 0;
  tlv.link_quality_2 = 0;
  tlv.link_quality_3 = 0;

  uint8_t leader_id = GetLeaderId();
  uint8_t cost = routers_[leader_id].cost;
  switch (GetDeviceState()) {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
      assert(false);
      break;

    case kDeviceStateChild:
      uint8_t parent_link_cost;
      parent_link_cost = parent_.link_quality_in;
      switch (parent_link_cost) {
        case 1: tlv.link_quality_1++; break;
        case 2: tlv.link_quality_2++; break;
        case 3: tlv.link_quality_3++; break;
      }
      cost += LQI_TO_COST(parent_link_cost);
      break;

    case kDeviceStateRouter:
      cost += GetLinkCost(routers_[leader_id].nexthop);
      break;

    case kDeviceStateLeader:
      cost = 0;
      break;
  }

  for (int i = 0; i < kMaxRouterId; i++) {
    uint8_t lqi;

    if (routers_[i].state != Neighbor::kStateValid || i == router_id_)
      continue;

    lqi = routers_[i].link_quality_in;
    if (lqi > routers_[i].link_quality_out)
      lqi = routers_[i].link_quality_out;

    switch (lqi) {
      case 1: tlv.link_quality_1++; break;
      case 2: tlv.link_quality_2++; break;
      case 3: tlv.link_quality_3++; break;
    }
  }

  tlv.leader_cost = (cost < kMaxRouteCost) ? cost : kMaxRouteCost;
  tlv.id_sequence = router_id_sequence_;

  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv.header) + tlv.header.length));

exit:
  return error;
}

ThreadError MleRouter::AppendChildAddresses(Message *message, Child *child) {
  ThreadError error;
  Ip6AddressTlv tlv;
  uint8_t *cur;

  tlv.header.type = Tlv::kTypeIp6Address;

  cur = reinterpret_cast<uint8_t*>(tlv.address);
  for (int i = 0; i < Child::kMaxIp6AddressPerChild; i++) {
    AddressRegistrationEntry *entry = reinterpret_cast<AddressRegistrationEntry*>(cur);
    Ip6Address *address = &child->ip6_address[i];
    Context context;

    if (address->IsUnspecified())
      continue;

    if (network_data_->GetContext(*address, &context) == kThreadError_None) {
      // compressed entry
      entry->control = entry->kCompressed | context.context_id;
      memcpy(entry->iid, address->s6_addr + 8, sizeof(entry->iid));
      cur += sizeof(entry->control) + sizeof(entry->iid);
    } else {
      // uncompressed entry
      entry->control = 0;
      memcpy(&entry->ip6_address, address, sizeof(entry->ip6_address));
      cur += sizeof(entry->control) + sizeof(entry->ip6_address);
    }
  }

  tlv.header.length = cur - reinterpret_cast<uint8_t*>(&tlv.address);
  VerifyOrExit(tlv.header.length > 0, error = kThreadError_Drop);
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv.header) + tlv.header.length));

exit:
  return error;
}

ThreadError MleRouter::AppendRoute(Message *message) {
  ThreadError error;
  RouteTlv tlv;
  int route_count = 0;

  tlv.header.type = Tlv::kTypeRoute;
  tlv.sequence = router_id_sequence_;
  memset(tlv.router_mask, 0, sizeof(tlv.router_mask));

  for (int i = 0; i < kMaxRouterId; i++) {
    if (routers_[i].allocated == false)
      continue;

    tlv.router_mask[i/8] |= 0x80 >> (i%8);

    if (i == router_id_) {
      tlv.route_data[route_count] = 0x01;
    } else {
      uint8_t cost;
      if (routers_[i].nexthop == kMaxRouterId) {
        cost = 0;
      } else {
        cost = routers_[i].cost + GetLinkCost(routers_[i].nexthop);
        if (cost >= kMaxRouteCost)
          cost = 0;
      }

      tlv.route_data[route_count] = cost;
      tlv.route_data[route_count] |= routers_[i].link_quality_in << 4;
      tlv.route_data[route_count] |= routers_[i].link_quality_out << 6;
    }

    route_count++;
  }

  tlv.header.length = sizeof(tlv.sequence) + sizeof(tlv.router_mask) + route_count;
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv.header) + tlv.header.length));

exit:
  return error;
}

}  // namespace Thread
