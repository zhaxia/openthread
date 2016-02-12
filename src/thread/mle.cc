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

#include <thread/mle.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/random.h>
#include <crypto/aes_ccm.h>
#include <mac/mac_frame.h>
#include <net/netif.h>
#include <net/udp6.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>
#include <assert.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {

Mle::Mle(AddressResolver *address_resolver, KeyManager *key_manager, MeshForwarder *mesh_forwarder,
         MleRouter *mle_router, ThreadNetif *netif, NetworkDataLeader *network_data):
    netif_callback_(&HandleNetifCallback, this),
    parent_request_timer_(&HandleParentRequestTimer, this),
    socket_(&RecvFrom, this) {
  address_resolver_ = address_resolver;
  key_manager_ = key_manager;
  mesh_ = mesh_forwarder;
  mle_router_ = mle_router;
  netif_ = netif;
  network_data_ = network_data;

  memset(&parent_, 0, sizeof(parent_));
  memset(&child_id_request_, 0, sizeof(child_id_request_));
  memset(&link_local_64_, 0, sizeof(link_local_64_));
  memset(&link_local_16_, 0, sizeof(link_local_16_));
  memset(&mesh_local_64_, 0, sizeof(mesh_local_64_));
  memset(&mesh_local_16_, 0, sizeof(mesh_local_16_));
  memset(&link_local_all_thread_nodes_, 0, sizeof(link_local_all_thread_nodes_));
  memset(&realm_local_all_thread_nodes_, 0, sizeof(realm_local_all_thread_nodes_));

  netif_->RegisterCallback(&netif_callback_);
}

ThreadError Mle::Init() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(device_state_ == kDeviceStateDisabled, error = kThreadError_Busy);

  memset(&leader_data_, 0, sizeof(leader_data_));

  // link-local 64
  memset(&link_local_64_, 0, sizeof(link_local_64_));
  link_local_64_.address.s6_addr16[0] = HostSwap16(0xfe80);
  memcpy(link_local_64_.address.s6_addr + 8, mesh_->GetAddress64(), 8);
  link_local_64_.address.s6_addr[8] ^= 2;
  link_local_64_.prefix_length = 64;
  link_local_64_.preferred_lifetime = 0xffffffff;
  link_local_64_.valid_lifetime = 0xffffffff;
  netif_->AddAddress(&link_local_64_);

  // link-local 16
  memset(&link_local_16_, 0, sizeof(link_local_16_));
  link_local_16_.address.s6_addr16[0] = HostSwap16(0xfe80);
  link_local_16_.address.s6_addr16[5] = HostSwap16(0x00ff);
  link_local_16_.address.s6_addr16[6] = HostSwap16(0xfe00);
  link_local_16_.prefix_length = 64;
  link_local_16_.preferred_lifetime = 0xffffffff;
  link_local_16_.valid_lifetime = 0xffffffff;

  // mesh-local 64
  for (int i = 8; i < 16; i++)
    mesh_local_64_.address.s6_addr[i] = Random::Get();
  mesh_local_64_.prefix_length = 64;
  mesh_local_64_.preferred_lifetime = 0xffffffff;
  mesh_local_64_.valid_lifetime = 0xffffffff;
  netif_->AddAddress(&mesh_local_64_);

  // mesh-local 16
  mesh_local_16_.address.s6_addr16[4] = HostSwap16(0x0000);
  mesh_local_16_.address.s6_addr16[5] = HostSwap16(0x00ff);
  mesh_local_16_.address.s6_addr16[6] = HostSwap16(0xfe00);
  mesh_local_16_.prefix_length = 64;
  mesh_local_16_.preferred_lifetime = 0xffffffff;
  mesh_local_16_.valid_lifetime = 0xffffffff;

  // link-local all thread nodes
  link_local_all_thread_nodes_.address_.s6_addr[0] = 0xff;
  link_local_all_thread_nodes_.address_.s6_addr[1] = 0x32;
  link_local_all_thread_nodes_.address_.s6_addr[12] = 0x00;
  link_local_all_thread_nodes_.address_.s6_addr[13] = 0x00;
  link_local_all_thread_nodes_.address_.s6_addr[14] = 0x00;
  link_local_all_thread_nodes_.address_.s6_addr[15] = 0x01;
  netif_->SubscribeMulticast(&link_local_all_thread_nodes_);

  // realm-local all thread nodes
  realm_local_all_thread_nodes_.address_.s6_addr[0] = 0xff;
  realm_local_all_thread_nodes_.address_.s6_addr[1] = 0x33;
  realm_local_all_thread_nodes_.address_.s6_addr[12] = 0x00;
  realm_local_all_thread_nodes_.address_.s6_addr[13] = 0x00;
  realm_local_all_thread_nodes_.address_.s6_addr[14] = 0x00;
  realm_local_all_thread_nodes_.address_.s6_addr[15] = 0x01;
  netif_->SubscribeMulticast(&realm_local_all_thread_nodes_);

exit:
  return error;
}

ThreadError Mle::Start() {
  ThreadError error = kThreadError_None;

  struct sockaddr_in6 sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  // memcpy(&sockaddr.sin6_addr, &link_local_64_.address, sizeof(sockaddr.sin6_addr));
  sockaddr.sin6_port = HostSwap16(kUdpPort);
  SuccessOrExit(error = socket_.Bind(&sockaddr));

  device_state_ = kDeviceStateDetached;
  SetStateDetached();

  if (GetAddress16() == MacFrame::kShortAddrInvalid) {
    BecomeChild(kJoinAnyPartition);
  } else if (ADDR16_TO_CHILD_ID(GetAddress16()) == 0) {
    mle_router_->BecomeRouter();
  } else {
    SendChildUpdateRequest();
    parent_request_state_ = kParentSynchronize;
    parent_request_timer_.Start(1000);
  }

exit:
  return error;
}

ThreadError Mle::Stop() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(device_state_ != kDeviceStateDisabled, error = kThreadError_Busy);

  SetStateDetached();
  socket_.Close();
  device_state_ = kDeviceStateDisabled;

exit:
  return error;
}

ThreadError Mle::BecomeDetached() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(device_state_ != kDeviceStateDisabled, error = kThreadError_Busy);

  SetStateDetached();
  SetAddress16(MacFrame::kShortAddrInvalid);
  BecomeChild(kJoinAnyPartition);

exit:
  return error;
}

ThreadError Mle::BecomeChild(JoinMode mode) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(device_state_ != kDeviceStateDisabled &&
               parent_request_state_ == kParentIdle, error = kThreadError_Busy);

  parent_request_state_ = kParentRequestStart;
  parent_request_mode_ = mode;
  memset(&parent_, 0, sizeof(parent_));
  if (mode == kJoinAnyPartition)
    parent_.state = Neighbor::kStateInvalid;
  parent_request_timer_.Start(1000);

exit:
  return error;
}

Mle::DeviceState Mle::GetDeviceState() const {
  return device_state_;
}

ThreadError Mle::SetStateDetached() {
  address_resolver_->Clear();
  device_state_ = kDeviceStateDetached;
  parent_request_state_ = kParentIdle;
  parent_request_timer_.Stop();
  mesh_->SetRxOnWhenIdle(true);
  mle_router_->HandleDetachStart();
  dprintf("Mode -> Detached\n");
  return kThreadError_None;
}

ThreadError Mle::SetStateChild(uint16_t address16) {
  SetAddress16(address16);
  device_state_ = kDeviceStateChild;
  parent_request_state_ = kParentIdle;
  if ((device_mode_ & kModeRxOnWhenIdle) != 0)
    parent_request_timer_.Start((timeout_ / 2) * 1000U);
  if ((device_mode_ & kModeFFD) != 0)
    mle_router_->HandleChildStart(parent_request_mode_);
  dprintf("Mode -> Child\n");
  return kThreadError_None;
}

uint32_t Mle::GetTimeout() const {
  return timeout_;
}

ThreadError Mle::SetTimeout(uint32_t timeout) {
  if (timeout < 2)
    timeout = 2;
  timeout_ = timeout;

  if (device_state_ == kDeviceStateChild) {
    SendChildUpdateRequest();
    if ((device_mode_ & kModeRxOnWhenIdle) != 0)
      parent_request_timer_.Start((timeout_ / 2) * 1000U);
  }

  return kThreadError_None;
}

uint8_t Mle::GetDeviceMode() const {
  return device_mode_;
}

ThreadError Mle::SetDeviceMode(uint8_t device_mode) {
  uint8_t old_mode = device_mode_;

  device_mode_ = device_mode;

  if ((old_mode & kModeFFD) != 0 && (device_mode & kModeFFD) == 0) {
    // FFD -> RFD
    switch (device_state_) {
      case kDeviceStateDisabled:
      case kDeviceStateDetached:
        break;
      case kDeviceStateChild:
        SetStateChild(GetAddress16());
        SendChildUpdateRequest();
        break;
      case kDeviceStateRouter:
      case kDeviceStateLeader:
        BecomeChild(kJoinAnyPartition);
        break;
    }
  } else if ((old_mode & kModeFFD) == 0 && (device_mode & kModeFFD) != 0) {
    // RFD -> FFD
    switch (device_state_) {
      case kDeviceStateDisabled:
      case kDeviceStateDetached:
        break;
      case kDeviceStateChild:
        SetStateChild(GetAddress16());
        SendChildUpdateRequest();
        break;
      case kDeviceStateRouter:
      case kDeviceStateLeader:
        assert(false);
        break;
    }
  } else if ((old_mode & kModeRxOnWhenIdle) != 0 && (device_mode & kModeRxOnWhenIdle) == 0) {
    // rx-on-when-idle -> rx-off-when-idle
    switch (device_state_) {
      case kDeviceStateDisabled:
      case kDeviceStateDetached:
        break;
      case kDeviceStateChild:
        SetStateChild(GetAddress16());
        SendChildUpdateRequest();
        break;
      case kDeviceStateRouter:
      case kDeviceStateLeader:
        assert(false);
        break;
    }
  } else if ((old_mode & kModeRxOnWhenIdle) == 0 && (device_mode & kModeRxOnWhenIdle) != 0) {
    // rx-off-when-idle -> rx-on-when-idle
    switch (device_state_) {
      case kDeviceStateDisabled:
      case kDeviceStateDetached:
        break;
      case kDeviceStateChild:
        SetStateChild(GetAddress16());
        SendChildUpdateRequest();
        break;
      case kDeviceStateRouter:
      case kDeviceStateLeader:
        assert(false);
        break;
    }
  }

  return kThreadError_None;
}

const uint8_t *Mle::GetMeshLocalPrefix() const {
  return mesh_local_16_.address.s6_addr;
}

ThreadError Mle::SetMeshLocalPrefix(const uint8_t *xpanid) {
  mesh_local_64_.address.s6_addr[0] = 0xfd;
  memcpy(mesh_local_64_.address.s6_addr + 1, xpanid, 5);
  mesh_local_64_.address.s6_addr[6] = 0x00;
  mesh_local_64_.address.s6_addr[7] = 0x00;

  memcpy(&mesh_local_16_.address, &mesh_local_64_.address, 8);

  link_local_all_thread_nodes_.address_.s6_addr[3] = 64;
  memcpy(link_local_all_thread_nodes_.address_.s6_addr + 4, &mesh_local_64_.address, 8);

  realm_local_all_thread_nodes_.address_.s6_addr[3] = 64;
  memcpy(realm_local_all_thread_nodes_.address_.s6_addr + 4, &mesh_local_64_.address, 8);

  return kThreadError_None;
}

const Ip6Address *Mle::GetLinkLocalAllThreadNodesAddress() const {
  return &link_local_all_thread_nodes_.address_;
}

const Ip6Address *Mle::GetRealmLocalAllThreadNodesAddress() const {
  return &realm_local_all_thread_nodes_.address_;
}

uint16_t Mle::GetAddress16() const {
  return mesh_->GetAddress16();
}

ThreadError Mle::SetAddress16(MacAddr16 address16) {
  if (address16 != MacFrame::kShortAddrInvalid) {
    // link-local 16
    link_local_16_.address.s6_addr16[7] = HostSwap16(address16);
    netif_->AddAddress(&link_local_16_);

    // mesh-local 16
    mesh_local_16_.address.s6_addr16[7] = HostSwap16(address16);
    netif_->AddAddress(&mesh_local_16_);
  } else {
    netif_->RemoveAddress(&link_local_16_);
    netif_->RemoveAddress(&mesh_local_16_);
  }

  mesh_->SetAddress16(address16);

  return kThreadError_None;
}

uint8_t Mle::GetLeaderId() const {
  return leader_data_.leader_router_id;
}

const Ip6Address *Mle::GetMeshLocal16() {
  return &mesh_local_16_.address;
}

const Ip6Address *Mle::GetMeshLocal64() {
  return &mesh_local_64_.address;
}

ThreadError Mle::GetLeaderAddress(Ip6Address *address) const {
  ThreadError error = kThreadError_None;

  VerifyOrExit(mesh_->GetAddress16() != MacFrame::kShortAddrInvalid, error = kThreadError_Error);

  memcpy(address, &mesh_local_16_.address, 8);
  address->s6_addr16[4] = HostSwap16(0x0000);
  address->s6_addr16[5] = HostSwap16(0x00ff);
  address->s6_addr16[6] = HostSwap16(0xfe00);
  address->s6_addr16[7] = HostSwap16(ROUTER_ID_TO_ADDR16(leader_data_.leader_router_id));

exit:
  return error;
}

Mle::LeaderData *Mle::GetLeaderData() {
  leader_data_.version = network_data_->GetVersion();
  leader_data_.stable_version = network_data_->GetStableVersion();
  return &leader_data_;
}

void Mle::GenerateNonce(const MacAddr64 *mac_addr, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce) {
  // source address
  for (int i = 0; i < 8; i++)
    nonce[i] = mac_addr->bytes[i];
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

ThreadError Mle::AppendSecureHeader(Message *message, uint8_t command) {
  ThreadError error = kThreadError_None;
  Header header;
  uint8_t header_length;

  header.security_suite = Header::kSecurityEnabled;
  header.security_control = MacFrame::kSecEncMic32;
  if (command == Header::kCommandAdvertisement ||
      command == Header::kCommandChildIdRequest ||
      command == Header::kCommandLinkReject ||
      command == Header::kCommandParentRequest ||
      command == Header::kCommandParentResponse) {
    header.security_control |= MacFrame::kKeyIdMode5;
    header_length = offsetof(Header, key_identifier) + 5;
  } else {
    header.security_control |= MacFrame::kKeyIdMode1;
    header_length = offsetof(Header, key_identifier) + 1;
  }

  SuccessOrExit(error = message->Append(&header, header_length));
  SuccessOrExit(error = message->Append(&command, sizeof(command)));

exit:
  return error;
}

ThreadError Mle::AppendSourceAddress(Message *message) {
  SourceAddressTlv tlv;

  tlv.header.type = Tlv::kTypeSourceAddress;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.address16 = HostSwap16(mesh_->GetAddress16());

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendStatus(Message *message, uint8_t status) {
  StatusTlv tlv;

  tlv.header.type = Tlv::kTypeStatus;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.status = status;

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendMode(Message *message, uint8_t mode) {
  ModeTlv tlv;

  tlv.header.type = Tlv::kTypeMode;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.mode = mode;

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendTimeout(Message *message, uint32_t timeout) {
  TimeoutTlv tlv;

  tlv.header.type = Tlv::kTypeTimeout;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.timeout = HostSwap32(timeout);

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendChallenge(Message *message, uint8_t *challenge, uint8_t challenge_length) {
  ThreadError error;
  Tlv tlv;

  tlv.type = Tlv::kTypeChallenge;
  tlv.length = challenge_length;
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv)));
  SuccessOrExit(error = message->Append(challenge, challenge_length));
exit:
  return error;
}

ThreadError Mle::AppendResponse(Message *message, const uint8_t *response, uint8_t response_len) {
  ThreadError error;
  Tlv tlv;

  tlv.type = Tlv::kTypeResponse;
  tlv.length = response_len;
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv)));
  SuccessOrExit(error = message->Append(response, response_len));

exit:
  return error;
}

ThreadError Mle::AppendLinkFrameCounter(Message *message) {
  LinkFrameCounterTlv tlv;

  tlv.header.type = Tlv::kTypeLinkFrameCounter;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.frame_counter = HostSwap32(key_manager_->GetMacFrameCounter());

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendMleFrameCounter(Message *message) {
  MleFrameCounterTlv tlv;

  tlv.header.type = Tlv::kTypeMleFrameCounter;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.frame_counter = HostSwap32(key_manager_->GetMleFrameCounter());

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendAddress16(Message *message, uint16_t address16) {
  Address16Tlv tlv;

  tlv.header.type = Tlv::kTypeAddress16;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.address16 = HostSwap16(address16);

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendLeaderData(Message *message) {
  LeaderDataTlv tlv;

  tlv.header.type = Tlv::kTypeLeaderData;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.leader_data.partition_id = HostSwap32(leader_data_.partition_id);
  tlv.leader_data.weighting = leader_data_.weighting;
  tlv.leader_data.version = network_data_->GetVersion();
  tlv.leader_data.stable_version = network_data_->GetStableVersion();
  tlv.leader_data.leader_router_id = leader_data_.leader_router_id;

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendNetworkData(Message *message, bool stable_only) {
  ThreadError error = kThreadError_None;
  NetworkDataTlv tlv;

  tlv.header.type = Tlv::kTypeNetworkData;
  SuccessOrExit(error = network_data_->GetNetworkData(stable_only, tlv.network_data, &tlv.header.length));
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv.header) + tlv.header.length));

exit:
  return error;
}

ThreadError Mle::AppendTlvRequest(Message *message, const uint8_t *tlvs, uint8_t tlvs_length) {
  ThreadError error;
  Tlv tlv;

  tlv.type = Tlv::kTypeTlvRequest;
  tlv.length = tlvs_length;
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv)));
  SuccessOrExit(error = message->Append(tlvs, tlvs_length));

exit:
  return error;
}

ThreadError Mle::AppendScanMask(Message *message, uint8_t scan_mask) {
  ScanMaskTlv tlv;

  tlv.header.type = Tlv::kTypeScanMask;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.scan_mask = scan_mask;

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendRssi(Message *message, int8_t rssi) {
  RssiTlv tlv;

  tlv.header.type = Tlv::kTypeRssi;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.rssi = rssi;

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendVersion(Message *message) {
  VersionTlv tlv;

  tlv.header.type = Tlv::kTypeVersion;
  tlv.header.length = sizeof(tlv) - sizeof(tlv.header);
  tlv.version = HostSwap16(kVersion);

  return message->Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendIp6Address(Message *message) {
  ThreadError error;
  Ip6AddressTlv tlv;
  uint8_t *cur;

  tlv.header.type = Tlv::kTypeIp6Address;

  cur = reinterpret_cast<uint8_t*>(tlv.address);
  for (const NetifAddress *addr = netif_->GetAddresses(); addr; addr = addr->next_) {
    AddressRegistrationEntry *entry = reinterpret_cast<AddressRegistrationEntry*>(cur);
    Context context;

    if (addr->address.IsLinkLocal() ||
        addr->address == mesh_local_16_.address)
      continue;

    if (network_data_->GetContext(addr->address, &context) == kThreadError_None) {
      // compressed entry
      entry->control = entry->kCompressed | context.context_id;
      memcpy(entry->iid, addr->address.s6_addr + 8, sizeof(entry->iid));
      cur += sizeof(entry->control) + sizeof(entry->iid);
    } else {
      // uncompressed entry
      entry->control = 0;
      memcpy(&entry->ip6_address, &addr->address, sizeof(entry->ip6_address));
      cur += sizeof(entry->control) + sizeof(entry->ip6_address);
    }
  }

  tlv.header.length = cur - reinterpret_cast<uint8_t*>(&tlv.address);
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv.header) + tlv.header.length));

exit:
  return error;
}

ThreadError Mle::AppendIp6Address(Message *message, Child *child) {
  ThreadError error;
  Ip6AddressTlv tlv;
  uint8_t *cur;

  tlv.header.type = Tlv::kTypeIp6Address;

  cur = reinterpret_cast<uint8_t*>(tlv.address);
  for (int i = 0; i < sizeof(child->ip6_address)/sizeof(child->ip6_address[0]); i++) {
    AddressRegistrationEntry *entry = reinterpret_cast<AddressRegistrationEntry*>(cur);
    Ip6Address *address = &child->ip6_address[i];
    Context context;

    if (network_data_->GetContext(child->ip6_address[i], &context) == kThreadError_None) {
      // compressed entry
      entry->control = entry->kCompressed | context.context_id;
      memcpy(entry->iid, address->s6_addr + 8, sizeof(entry->iid));
      cur += sizeof(entry->control) + sizeof(entry->iid);
    } else {
      // uncompressed entry
      entry->control = 0;
      memcpy(&entry->ip6_address, &address, sizeof(entry->ip6_address));
      cur += sizeof(entry->control) + sizeof(entry->ip6_address);
    }
  }

  tlv.header.length = cur - reinterpret_cast<uint8_t*>(&tlv.address);
  SuccessOrExit(error = message->Append(&tlv, sizeof(tlv.header) + tlv.header.length));

exit:
  return error;
}

void Mle::HandleNetifCallback(void *context) {
  Mle *obj = reinterpret_cast<Mle*>(context);
  obj->HandleNetifCallback();
}

void Mle::HandleNetifCallback() {
  if (!netif_->IsAddress(&mesh_local_64_.address)) {
    // Mesh Local EID was removed, choose a new one and add it back
    for (int i = 8; i < 16; i++)
      mesh_local_64_.address.s6_addr[i] = Random::Get();
    netif_->AddAddress(&mesh_local_64_);
  }

  switch (device_state_) {
    case kDeviceStateChild:
      Ip6Address tmp;
      memset(&tmp, 0, sizeof(tmp));
      tmp.s6_addr16[0] = HostSwap16(0x2001);
      tmp.s6_addr16[15] = HostSwap16(0x0001);
      printf("%d\n", netif_->IsAddress(&tmp));
      SendChildUpdateRequest();
      break;
    default:
      break;
  }
}

void Mle::HandleParentRequestTimer(void *context) {
  Mle *obj = reinterpret_cast<Mle*>(context);
  obj->HandleParentRequestTimer();
}

void Mle::HandleParentRequestTimer() {
  switch (parent_request_state_) {
    case kParentIdle:
      if (parent_.state == Neighbor::kStateValid) {
        if (device_mode_ & kModeRxOnWhenIdle) {
          SendChildUpdateRequest();
          parent_request_timer_.Start((timeout_ / 2) * 1000U);
        }
      } else {
        BecomeDetached();
      }
      break;
    case kParentSynchronize:
      parent_request_state_ = kParentIdle;
      BecomeChild(kJoinAnyPartition);
      break;
    case kParentRequestStart:
      parent_request_state_ = kParentRequestRouter;
      parent_.state = Neighbor::kStateInvalid;
      SendParentRequest();
      parent_request_timer_.Start(kParentRequestRouterTimeout);
      break;
    case kParentRequestRouter:
      parent_request_state_ = kParentRequestChild;
      if (parent_.state == Neighbor::kStateValid) {
        SendChildIdRequest();
        parent_request_state_ = kChildIdRequest;
      } else {
        SendParentRequest();
      }
      parent_request_timer_.Start(kParentRequestChildTimeout);
      break;
    case kParentRequestChild:
      parent_request_state_ = kParentRequestChild;
      if (parent_.state == Neighbor::kStateValid) {
        SendChildIdRequest();
        parent_request_state_ = kChildIdRequest;
        parent_request_timer_.Start(kParentRequestChildTimeout);
      } else {
        switch (parent_request_mode_) {
          case kJoinAnyPartition:
            if (device_mode_ & kModeFFD) {
              mle_router_->BecomeLeader();
            } else {
              parent_request_state_ = kParentIdle;
              BecomeDetached();
            }
            break;
          case kJoinSamePartition:
            parent_request_state_ = kParentIdle;
            BecomeChild(kJoinAnyPartition);
            break;
          case kJoinBetterPartition:
            parent_request_state_ = kParentIdle;
            break;
        }
      }
      break;
    case kChildIdRequest:
      parent_request_state_ = kParentIdle;
      if (device_state_ != kDeviceStateRouter && device_state_ != kDeviceStateLeader)
        BecomeDetached();
      break;
  }
}

ThreadError Mle::SendParentRequest() {
  ThreadError error = kThreadError_None;
  Message *message;
  uint8_t scan_mask = 0;

  for (uint8_t i = 0; i < sizeof(parent_request_.challenge); i++)
    parent_request_.challenge[i] = Random::Get();

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandParentRequest));
  SuccessOrExit(error = AppendMode(message, device_mode_));
  SuccessOrExit(error = AppendChallenge(message, parent_request_.challenge, sizeof(parent_request_.challenge)));

  switch (parent_request_state_) {
    case kParentIdle:
      assert(false);
      break;
    case kParentRequestStart:
      assert(false);
      break;
    case kParentRequestRouter:
      scan_mask = ScanMaskTlv::kRouterScan;
      break;
    case kParentRequestChild:
      scan_mask = ScanMaskTlv::kRouterScan | ScanMaskTlv::kChildScan;
      break;
    case kChildIdRequest:
      assert(false);
      break;
  }

  SuccessOrExit(error = AppendScanMask(message, scan_mask));
  SuccessOrExit(error = AppendVersion(message));

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xff02);
  destination.s6_addr16[7] = HostSwap16(0x0002);
  SuccessOrExit(error = SendMessage(message, &destination));

  switch (parent_request_state_) {
    case kParentIdle:
      assert(false);
      break;
    case kParentRequestStart:
      assert(false);
      break;
    case kParentRequestRouter:
      dprintf("Sent parent request to routers\n");
      break;
    case kParentRequestChild:
      dprintf("Sent parent request to all devices\n");
      break;
    case kChildIdRequest:
      assert(false);
      break;
  }

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return kThreadError_None;
}

ThreadError Mle::SendChildIdRequest() {
  ThreadError error = kThreadError_None;
  uint8_t tlvs[] = {Tlv::kTypeAddress16, Tlv::kTypeNetworkData, Tlv::kTypeRoute};
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandChildIdRequest));
  SuccessOrExit(error = AppendResponse(message, child_id_request_.challenge, child_id_request_.challenge_length));
  SuccessOrExit(error = AppendLinkFrameCounter(message));
  SuccessOrExit(error = AppendMleFrameCounter(message));
  SuccessOrExit(error = AppendMode(message, device_mode_));
  SuccessOrExit(error = AppendTimeout(message, timeout_));
  SuccessOrExit(error = AppendVersion(message));
  if ((device_mode_ & kModeFFD) == 0)
    SuccessOrExit(error = AppendIp6Address(message));
  SuccessOrExit(error = AppendTlvRequest(message, tlvs, sizeof(tlvs)));

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xfe80);
  memcpy(destination.s6_addr + 8, &parent_.mac_addr, sizeof(parent_.mac_addr));
  destination.s6_addr[8] ^= 0x2;
  SuccessOrExit(error = SendMessage(message, &destination));
  dprintf("Sent Child ID Request\n");

  if ((device_mode_ & kModeRxOnWhenIdle) == 0) {
    mesh_->SetPollPeriod(100);
    mesh_->SetRxOnWhenIdle(false);
  }

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError Mle::SendDataRequest(const Ip6MessageInfo *message_info, uint8_t *tlvs, uint8_t tlvs_length) {
  ThreadError error = kThreadError_None;
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandDataRequest));
  SuccessOrExit(error = AppendTlvRequest(message, tlvs, tlvs_length));

  Ip6Address destination;
  memcpy(&destination, &message_info->peer_addr, sizeof(destination));
  SuccessOrExit(error = SendMessage(message, &destination));

  dprintf("Sent Data Request\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError Mle::SendDataResponse(const Ip6Address *destination, const uint8_t *tlvs, uint8_t tlvs_length) {
  ThreadError error = kThreadError_None;
  Message *message;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandDataResponse));

  Neighbor *neighbor;
  neighbor = mle_router_->GetNeighbor(destination);

  for (int i = 0; i < tlvs_length; i++) {
    switch (tlvs[i]) {
      case Tlv::kTypeLeaderData:
        SuccessOrExit(error = AppendLeaderData(message));
        break;
      case Tlv::kTypeNetworkData:
        bool stable_only;
        stable_only = neighbor != NULL ? (neighbor->mode & kModeFullNetworkData) == 0 : false;
        SuccessOrExit(error = AppendNetworkData(message, stable_only));
        break;
    }
  }

  SuccessOrExit(error = SendMessage(message, destination));

  dprintf("Sent Data Response\n");

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError Mle::SendChildUpdateRequest() {
  ThreadError error = kThreadError_None;

  Message *message;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, ;);
  SuccessOrExit(error = AppendSecureHeader(message, Header::kCommandChildUpdateRequest));
  SuccessOrExit(error = AppendMode(message, device_mode_));
  if ((device_mode_ & kModeFFD) == 0)
    SuccessOrExit(error = AppendIp6Address(message));

  switch (device_state_) {
    case kDeviceStateDetached:
      for (uint8_t i = 0; i < sizeof(parent_request_.challenge); i++)
        parent_request_.challenge[i] = Random::Get();
      SuccessOrExit(error = AppendChallenge(message, parent_request_.challenge, sizeof(parent_request_.challenge)));
      break;
    case kDeviceStateChild:
      SuccessOrExit(error = AppendSourceAddress(message));
      SuccessOrExit(error = AppendLeaderData(message));
      SuccessOrExit(error = AppendTimeout(message, timeout_));
      break;
    case kDeviceStateRouter:
    case kDeviceStateLeader:
      assert(false);
      break;
  }

  Ip6Address destination;
  memset(&destination, 0, sizeof(destination));
  destination.s6_addr16[0] = HostSwap16(0xfe80);
  memcpy(destination.s6_addr + 8, &parent_.mac_addr, sizeof(parent_.mac_addr));
  destination.s6_addr[8] ^= 0x2;
  SuccessOrExit(error = SendMessage(message, &destination));

  dprintf("Sent Child Update Request\n");

  if ((device_mode_ & kModeRxOnWhenIdle) == 0)
    mesh_->SetPollPeriod(100);

exit:
  if (error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError Mle::SendMessage(Message *message, const Ip6Address *destination) {
  ThreadError error = kThreadError_None;

  Header header;
  uint8_t header_length;
  message->Read(0, sizeof(header), &header);
  assert(header.security_suite == Header::kSecurityEnabled);
  header.frame_counter = Thread::Encoding::LittleEndian::HostSwap16(key_manager_->GetMleFrameCounter());

  uint32_t key_sequence;
  key_sequence = key_manager_->GetCurrentKeySequence();
  switch (header.security_control & MacFrame::kKeyIdModeMask) {
    case MacFrame::kKeyIdMode1:
      header.key_identifier[0] = (key_sequence & 0x7f) + 1;
      header_length = offsetof(Header, key_identifier) + 1;
      break;
    case MacFrame::kKeyIdMode5:
      header.key_identifier[4] = (key_sequence & 0x7f) + 1;
      header.key_identifier[3] = key_sequence >> 24;
      header.key_identifier[2] = key_sequence >> 16;
      header.key_identifier[1] = key_sequence >> 8;
      header.key_identifier[0] = key_sequence;
      header_length = offsetof(Header, key_identifier) + 5;
      break;
    default:
      assert(false);
  }

  message->Write(0, header_length, &header);

  uint8_t nonce[13];
  GenerateNonce(mesh_->GetAddress64(), key_manager_->GetMleFrameCounter(), MacFrame::kSecEncMic32, nonce);

  uint8_t tag[4];
  uint8_t tag_length;

  AesEcb aes_ecb;
  aes_ecb.SetKey(key_manager_->GetCurrentMleKey(), 16);

  AesCcm aes_ccm;
  aes_ccm.Init(&aes_ecb, 16 + 16 + header_length-1, message->GetLength() - header_length, sizeof(tag),
               nonce, sizeof(nonce));

  aes_ccm.Header(&link_local_64_.address, sizeof(link_local_64_.address));
  aes_ccm.Header(destination, sizeof(*destination));
  aes_ccm.Header(&header.security_control, header_length - 1);

  message->SetOffset(header_length);
  while (message->GetOffset() < message->GetLength()) {
    uint8_t buf[64];
    int length;
    length = message->Read(message->GetOffset(), sizeof(buf), buf);
    aes_ccm.Payload(buf, buf, length, true);
    message->Write(message->GetOffset(), length, buf);
    message->MoveOffset(length);
  }

  tag_length = sizeof(tag);
  aes_ccm.Finalize(tag, &tag_length);
  SuccessOrExit(message->Append(tag, tag_length));

  Ip6MessageInfo message_info;
  memset(&message_info, 0, sizeof(message_info));
  memcpy(&message_info.peer_addr, destination, sizeof(message_info.peer_addr));
  memcpy(&message_info.sock_addr, &link_local_64_.address, sizeof(message_info.sock_addr));
  message_info.peer_port = kUdpPort;
  message_info.interface_id = netif_->GetInterfaceId();
  message_info.hop_limit = 255;

  key_manager_->IncrementMleFrameCounter();

  SuccessOrExit(error = socket_.SendTo(message, &message_info));

exit:
  return error;
}

void Mle::RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info) {
  Mle *obj = reinterpret_cast<Mle*>(context);
  obj->RecvFrom(message, message_info);
}

void Mle::RecvFrom(Message *message, const Ip6MessageInfo *message_info) {
  Header header;

  message->Read(message->GetOffset(), sizeof(header), &header);
  if (header.security_suite != Header::kSecurityEnabled ||
      (header.security_control & MacFrame::kSecLevelMask) != MacFrame::kSecEncMic32)
    return;

  uint8_t header_length;
  uint32_t key_sequence;
  const uint8_t *mle_key;
  switch (header.security_control & MacFrame::kKeyIdModeMask) {
    case MacFrame::kKeyIdMode1:
      uint8_t keyid;
      keyid = header.key_identifier[0] - 1;
      if (keyid == (key_manager_->GetCurrentKeySequence() & 0x7f)) {
        key_sequence = key_manager_->GetCurrentKeySequence();
        mle_key = key_manager_->GetCurrentMleKey();
      } else if (key_manager_->IsPreviousKeyValid() &&
                 keyid == (key_manager_->GetPreviousKeySequence() & 0x7f)) {
        key_sequence = key_manager_->GetPreviousKeySequence();
        mle_key = key_manager_->GetPreviousMleKey();
      } else {
        key_sequence = (key_manager_->GetCurrentKeySequence() & ~0x7f) | keyid;
        if (key_sequence < key_manager_->GetCurrentKeySequence())
          key_sequence += 128;
        mle_key = key_manager_->GetTemporaryMleKey(key_sequence);
      }
      header_length = offsetof(Header, key_identifier) + 1;
      break;
    case MacFrame::kKeyIdMode5:
      key_sequence = (static_cast<uint32_t>(header.key_identifier[3]) << 24 |
                      static_cast<uint32_t>(header.key_identifier[2]) << 16 |
                      static_cast<uint32_t>(header.key_identifier[1]) << 8 |
                      static_cast<uint32_t>(header.key_identifier[0]) << 0);
      if (key_sequence == key_manager_->GetCurrentKeySequence())
        mle_key = key_manager_->GetCurrentMleKey();
      else if (key_manager_->IsPreviousKeyValid() &&
               key_sequence == key_manager_->GetPreviousKeySequence())
        mle_key = key_manager_->GetPreviousMleKey();
      else
        mle_key = key_manager_->GetTemporaryMleKey(key_sequence);
      header_length = offsetof(Header, key_identifier) + 5;
      break;
    default:
      return;
  }

  message->MoveOffset(header_length);

  uint32_t frame_counter;
  frame_counter = Thread::Encoding::LittleEndian::HostSwap32(header.frame_counter);

  uint8_t message_tag[4];
  uint8_t message_tag_length;
  message_tag_length = message->Read(message->GetLength() - sizeof(message_tag), sizeof(message_tag), message_tag);
  if (message_tag_length != sizeof(message_tag))
    return;
  if (message->SetLength(message->GetLength() - sizeof(message_tag)) != kThreadError_None)
    return;

  uint8_t nonce[13];

  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;
  GenerateNonce(&mac_addr, frame_counter, MacFrame::kSecEncMic32, nonce);

  AesEcb aes_ecb;
  aes_ecb.SetKey(mle_key, 16);

  AesCcm aes_ccm;
  aes_ccm.Init(&aes_ecb,
               sizeof(message_info->peer_addr) + sizeof(message_info->sock_addr) +
               header_length - offsetof(Header, security_control),
               message->GetLength() - message->GetOffset(), sizeof(message_tag),
               nonce, sizeof(nonce));
  aes_ccm.Header(&message_info->peer_addr, sizeof(message_info->peer_addr));
  aes_ccm.Header(&message_info->sock_addr, sizeof(message_info->sock_addr));
  aes_ccm.Header(&header.security_control, header_length - offsetof(Header, security_control));

  uint16_t mle_offset;
  mle_offset = message->GetOffset();
  while (message->GetOffset() < message->GetLength()) {
    uint8_t buf[64];
    int length;
    length = message->Read(message->GetOffset(), sizeof(buf), buf);
    aes_ccm.Payload(buf, buf, length, false);
    message->Write(message->GetOffset(), length, buf);
    message->MoveOffset(length);
  }

  uint8_t tag[4];
  uint8_t tag_length;
  tag_length = sizeof(tag);
  aes_ccm.Finalize(tag, &tag_length);
  VerifyOrExit(message_tag_length == tag_length && memcmp(message_tag, tag, tag_length) == 0, ;);
  if (key_sequence > key_manager_->GetCurrentKeySequence())
    key_manager_->SetCurrentKeySequence(key_sequence);

  message->SetOffset(mle_offset);

  uint8_t command;
  message->Read(message->GetOffset(), sizeof(command), &command);
  message->MoveOffset(sizeof(command));

  Neighbor *neighbor;
  switch (device_state_) {
    case kDeviceStateDetached:
    case kDeviceStateChild:
      neighbor = GetNeighbor(&mac_addr);
      break;
    case kDeviceStateRouter:
    case kDeviceStateLeader:
      if (command == Header::kCommandChildIdResponse)
        neighbor = GetNeighbor(&mac_addr);
      else
        neighbor = mle_router_->GetNeighbor(&mac_addr);
      break;
    default:
      neighbor = NULL;
      break;
  }

  if (neighbor != NULL && neighbor->state == Neighbor::kStateValid) {
    if (key_sequence == key_manager_->GetCurrentKeySequence())
      VerifyOrExit(neighbor->previous_key == true || frame_counter >= neighbor->valid.mle_frame_counter,
                   dprintf("mle frame counter reject 1\n"));
    else if (key_sequence == key_manager_->GetPreviousKeySequence())
      VerifyOrExit(neighbor->previous_key == true && frame_counter >= neighbor->valid.mle_frame_counter,
                   dprintf("mle frame counter reject 2\n"));
    else
      assert(false);
    neighbor->valid.mle_frame_counter = frame_counter + 1;
  } else {
    VerifyOrExit(command == Header::kCommandLinkRequest ||
                 command == Header::kCommandLinkAccept ||
                 command == Header::kCommandLinkAcceptAndRequest ||
                 command == Header::kCommandAdvertisement ||
                 command == Header::kCommandParentRequest ||
                 command == Header::kCommandParentResponse ||
                 command == Header::kCommandChildIdRequest ||
                 command == Header::kCommandChildUpdateRequest, dprintf("mle sequence unknown! %d\n", command));
  }

  switch (command) {
    case Header::kCommandLinkRequest:
      mle_router_->HandleLinkRequest(message, message_info);
      break;
    case Header::kCommandLinkAccept:
      mle_router_->HandleLinkAccept(message, message_info, key_sequence);
      break;
    case Header::kCommandLinkAcceptAndRequest:
      mle_router_->HandleLinkAcceptAndRequest(message, message_info, key_sequence);
      break;
    case Header::kCommandLinkReject:
      mle_router_->HandleLinkReject(message, message_info);
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
      mle_router_->HandleParentRequest(message, message_info);
      break;
    case Header::kCommandParentResponse:
      HandleParentResponse(message, message_info, key_sequence);
      break;
    case Header::kCommandChildIdRequest:
      mle_router_->HandleChildIdRequest(message, message_info, key_sequence);
      break;
    case Header::kCommandChildIdResponse:
      HandleChildIdResponse(message, message_info);
      break;
    case Header::kCommandChildUpdateRequest:
      mle_router_->HandleChildUpdateRequest(message, message_info);
      break;
    case Header::kCommandChildUpdateResponse:
      HandleChildUpdateResponse(message, message_info);
      break;
  }

exit:
  {}
}

ThreadError Mle::FindTlv(Message *message, uint8_t type, void *buf, uint16_t buf_length) {
  ThreadError error = kThreadError_Parse;
  uint16_t offset = message->GetOffset();
  uint16_t end = message->GetLength();

  while (offset < end) {
    Tlv tlv;
    message->Read(offset, sizeof(tlv), &tlv);
    if (tlv.type == type && (offset + sizeof(tlv) + tlv.length) <= end) {
      if (buf_length > sizeof(tlv) + tlv.length)
        buf_length = sizeof(tlv) + tlv.length;
      message->Read(offset, buf_length, buf);

      ExitNow(error = kThreadError_None);
    }
    offset += sizeof(tlv) + tlv.length;
  }

exit:
  return error;
}

ThreadError Mle::HandleAdvertisement(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  if (device_state_ != kDeviceStateDetached)
    SuccessOrExit(error = mle_router_->HandleAdvertisement(message, message_info));

  MacAddr64 mac_addr;
  memcpy(&mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  bool is_neighbor;
  is_neighbor = false;

  switch (device_state_) {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
      break;
    case kDeviceStateChild:
      if (memcmp(&parent_.mac_addr, &mac_addr, sizeof(parent_.mac_addr)))
        break;
      is_neighbor = true;
      parent_.last_heard = parent_request_timer_.GetNow();
      break;
    case kDeviceStateRouter:
    case kDeviceStateLeader:
      Neighbor *neighbor;
      if ((neighbor = mle_router_->GetNeighbor(&mac_addr)) != NULL &&
          neighbor->state == Neighbor::kStateValid)
        is_neighbor = true;
      break;
  }

  if (is_neighbor) {
    LeaderDataTlv leader_data;
    SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
    VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
                 error = kThreadError_Parse);

    if (static_cast<int8_t>(leader_data.leader_data.version - network_data_->GetVersion()) > 0) {
      uint8_t tlvs[] = {Tlv::kTypeLeaderData, Tlv::kTypeNetworkData};
      SendDataRequest(message_info, tlvs, sizeof(tlvs));
    }
  }

exit:
  return error;
}

ThreadError Mle::HandleDataRequest(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  TlvRequestTlv tlv_request;

  dprintf("Received Data Request\n");

  // TLV Request
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeTlvRequest, &tlv_request, sizeof(tlv_request)));
  VerifyOrExit(tlv_request.header.length <= sizeof(tlv_request) - sizeof(tlv_request.header),
               error = kThreadError_Parse);

  SendDataResponse(&message_info->peer_addr, tlv_request.tlvs, tlv_request.header.length);

exit:
  return error;
}

ThreadError Mle::HandleDataResponse(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  LeaderDataTlv leader_data;
  NetworkDataTlv network_data;

  dprintf("Received Data Response\n");

  SuccessOrExit(error = FindTlv(message, Tlv::kTypeNetworkData, &network_data, sizeof(network_data)));
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
  VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
               error = kThreadError_Parse);

  int8_t diff;
  diff = leader_data.leader_data.version - network_data_->GetVersion();
  VerifyOrExit(diff > 0, ;);

  SuccessOrExit(error = network_data_->SetNetworkData(leader_data.leader_data.version,
                                                      leader_data.leader_data.stable_version,
                                                      (device_mode_ & kModeFullNetworkData) == 0,
                                                      network_data.network_data, network_data.header.length));

exit:
  return error;
}

uint8_t Mle::LinkMarginToQuality(uint8_t link_margin) {
  if (link_margin > 20)
    return 3;
  else if (link_margin > 10)
    return 2;
  else if (link_margin > 2)
    return 1;
  else
    return 0;
}

ThreadError Mle::HandleParentResponse(Message *message, const Ip6MessageInfo *message_info,
                                          uint32_t key_sequence) {
  ThreadError error = kThreadError_None;

  dprintf("Received Parent Response\n");

  // Response
  ResponseTlv response;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeResponse, &response, sizeof(response)));
  VerifyOrExit(response.header.length == sizeof(response.response) &&
               memcmp(response.response, parent_request_.challenge, sizeof(response.response)) == 0,
               error = kThreadError_Parse);

  // Source Address
  SourceAddressTlv source_address;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeSourceAddress, &source_address, sizeof(source_address)));
  VerifyOrExit(source_address.header.length == sizeof(source_address.address16),
               error = kThreadError_Parse);

  // Leader Data
  LeaderDataTlv leader_data;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
  VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
               error = kThreadError_Parse);

  // Weight
  VerifyOrExit(leader_data.leader_data.weighting >= mle_router_->GetLeaderWeight(), ;);

  // Partition ID
  uint32_t peer_partition_id;
  peer_partition_id = HostSwap32(leader_data.leader_data.partition_id);
  if (device_state_ != kDeviceStateDetached) {
    switch (parent_request_mode_) {
      case kJoinAnyPartition:
        break;

      case kJoinSamePartition:
        if (peer_partition_id != leader_data_.partition_id)
          ExitNow();
        break;

      case kJoinBetterPartition:
        dprintf("partition info  %d %d %d %d\n",
                leader_data.leader_data.weighting, peer_partition_id,
                leader_data_.weighting, leader_data_.partition_id);
        if ((leader_data.leader_data.weighting < leader_data_.weighting) ||
            (leader_data.leader_data.weighting == leader_data_.weighting &&
             peer_partition_id <= leader_data_.partition_id))
          ExitNow(dprintf("ignore parent response\n"));
        break;
    }
  }

  // Link Quality
  RssiTlv rssi;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeRssi, &rssi, sizeof(rssi)));
  VerifyOrExit(rssi.header.length == sizeof(rssi) - sizeof(rssi.header), error = kThreadError_Parse);

  uint8_t link_margin;
  link_margin = reinterpret_cast<ThreadMessageInfo*>(message_info->link_info)->link_margin;
  if (link_margin > rssi.rssi)
    link_margin = rssi.rssi;

  uint8_t link_quality;
  link_quality = LinkMarginToQuality(link_margin);

  VerifyOrExit(parent_request_state_ != kParentRequestRouter || link_quality == 3, ;);

  // Connectivity
  ConnectivityTlv connectivity;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeConnectivity, &connectivity, sizeof(connectivity)));
  VerifyOrExit(connectivity.header.length == sizeof(connectivity) - sizeof(connectivity.header),
               error = kThreadError_Parse);
  if (peer_partition_id == leader_data_.partition_id) {
    int8_t diff = connectivity.id_sequence - mle_router_->GetRouterIdSequence();
    VerifyOrExit(diff > 0 || (diff == 0 && mle_router_->GetLeaderAge() < mle_router_->GetNetworkIdTimeout()), ;);
  }

  // XXX: how does Leader Cost factor into this?
  uint32_t connectivity_metric;
  connectivity_metric =
      (static_cast<uint32_t>(link_quality) << 24) |
      (static_cast<uint32_t>(connectivity.link_quality_3) << 16) |
      (static_cast<uint32_t>(connectivity.link_quality_2) << 8) |
      (static_cast<uint32_t>(connectivity.link_quality_1));
  printf("connectivity = %08x\n", connectivity_metric);

  if (parent_.state == Neighbor::kStateValid) {
    VerifyOrExit(connectivity_metric > parent_connectivity_, ;);
  }

  // Link Frame Counter
  LinkFrameCounterTlv link_frame_counter;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeLinkFrameCounter, &link_frame_counter,
                                      sizeof(link_frame_counter)));
  VerifyOrExit(link_frame_counter.header.length == sizeof(link_frame_counter.frame_counter),
               error = kThreadError_Parse);

  // Mle Frame Counter
  MleFrameCounterTlv mle_frame_counter;
  if (FindTlv(message, Tlv::kTypeMleFrameCounter, &mle_frame_counter, sizeof(mle_frame_counter)) ==
      kThreadError_None) {
    VerifyOrExit(mle_frame_counter.header.length == sizeof(mle_frame_counter.frame_counter), ;);
  } else {
    mle_frame_counter.frame_counter = link_frame_counter.frame_counter;
  }

  // Challenge
  ChallengeTlv challenge;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeChallenge, &challenge, sizeof(challenge)));
  VerifyOrExit(challenge.header.length <= sizeof(challenge.challenge), error = kThreadError_Parse);
  memcpy(child_id_request_.challenge, challenge.challenge, challenge.header.length);
  child_id_request_.challenge_length = challenge.header.length;

  memcpy(&parent_.mac_addr, message_info->peer_addr.s6_addr + 8, sizeof(parent_.mac_addr));
  parent_.mac_addr.bytes[0] ^= 0x2;
  parent_.valid.address16 = HostSwap16(source_address.address16);
  parent_.valid.link_frame_counter = HostSwap32(link_frame_counter.frame_counter);
  parent_.valid.mle_frame_counter = HostSwap32(mle_frame_counter.frame_counter);
  parent_.mode = kModeFFD | kModeRxOnWhenIdle | kModeFullNetworkData;
  parent_.state = Neighbor::kStateValid;
  assert(key_sequence == key_manager_->GetCurrentKeySequence() ||
         key_sequence == key_manager_->GetPreviousKeySequence());
  parent_.previous_key = key_sequence == key_manager_->GetPreviousKeySequence();
  parent_connectivity_ = connectivity_metric;

exit:
  return error;
}

ThreadError Mle::HandleChildIdResponse(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  dprintf("Received Child ID Response\n");
  VerifyOrExit(parent_request_state_ == kChildIdRequest, ;);

  // Leader Data
  LeaderDataTlv leader_data;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
  VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
               error = kThreadError_Parse);

  // Source Address
  SourceAddressTlv source_address;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeSourceAddress, &source_address, sizeof(source_address)));
  VerifyOrExit(source_address.header.length == sizeof(source_address.address16),
               error = kThreadError_Parse);

  // Address16
  Address16Tlv address16;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeAddress16, &address16, sizeof(address16)));
  VerifyOrExit(address16.header.length == sizeof(address16) - sizeof(address16.header),
               error = kThreadError_Parse);

  // Network Data
  NetworkDataTlv network_data;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeNetworkData, &network_data, sizeof(network_data)));
  SuccessOrExit(error = network_data_->SetNetworkData(leader_data.leader_data.version,
                                                      leader_data.leader_data.stable_version,
                                                      (device_mode_ & kModeFullNetworkData) == 0,
                                                      network_data.network_data, network_data.header.length));

  // Parent Attach Success
  parent_request_timer_.Stop();

  leader_data_.partition_id = HostSwap32(leader_data.leader_data.partition_id);
  leader_data_.weighting = leader_data.leader_data.weighting;
  leader_data_.leader_router_id = leader_data.leader_data.leader_router_id;

  if ((device_mode_ & kModeRxOnWhenIdle) == 0) {
    mesh_->SetPollPeriod((timeout_ / 2) * 1000U);
    mesh_->SetRxOnWhenIdle(false);
  } else {
    mesh_->SetRxOnWhenIdle(true);
  }

  parent_.valid.address16 = HostSwap16(source_address.address16);
  SuccessOrExit(error = SetStateChild(HostSwap16(address16.address16)));

  // Route
  RouteTlv route;
  if (FindTlv(message, Tlv::kTypeRoute, &route, sizeof(route)) == kThreadError_None) {
    uint8_t num_routers = 0;
    for (int i = 0; i < kMaxRouterId; i++)
      num_routers += (route.router_mask[i / 8] & (0x80 >> (i % 8))) != 0;
    if (num_routers < mle_router_->GetRouterUpgradeThreshold())
      mle_router_->BecomeRouter();
  }

exit:
  return error;
}

ThreadError Mle::HandleChildUpdateResponse(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  dprintf("Received Child Update Response\n");

  // Status
  StatusTlv status;
  if (FindTlv(message, Tlv::kTypeStatus, &status, sizeof(status)) == kThreadError_None) {
    BecomeDetached();
    ExitNow();
  }

  // Mode
  ModeTlv mode;
  SuccessOrExit(error = FindTlv(message, Tlv::kTypeMode, &mode, sizeof(mode)));
  VerifyOrExit(mode.header.length == sizeof(mode) - sizeof(mode.header), error = kThreadError_Parse);
  VerifyOrExit(mode.mode == device_mode_, error = kThreadError_Drop);

  switch (device_state_) {
    case kDeviceStateDetached: {
      // Response
      ResponseTlv response;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeResponse, &response, sizeof(response)));
      VerifyOrExit(response.header.length == sizeof(response.response), error = kThreadError_Parse);
      VerifyOrExit(memcmp(response.response, parent_request_.challenge, sizeof(response.response)) == 0,
                   error = kThreadError_Drop);

      SetStateChild(GetAddress16());
      break;
    }

    case kDeviceStateChild: {
      // Leader Data
      LeaderDataTlv leader_data;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeLeaderData, &leader_data, sizeof(leader_data)));
      VerifyOrExit(leader_data.header.length == sizeof(leader_data) - sizeof(leader_data.header),
                   error = kThreadError_Parse);
      if (static_cast<int8_t>(leader_data.leader_data.version - network_data_->GetVersion()) > 0) {
        uint8_t tlvs[] = {Tlv::kTypeLeaderData, Tlv::kTypeNetworkData};
        SendDataRequest(message_info, tlvs, sizeof(tlvs));
      }

      // Source Address
      SourceAddressTlv source_address;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeSourceAddress, &source_address, sizeof(source_address)));
      VerifyOrExit(source_address.header.length == sizeof(source_address) - sizeof(source_address.header),
                   error = kThreadError_Parse);
      if (ADDR16_TO_ROUTER_ID(HostSwap16(source_address.address16)) != ADDR16_TO_ROUTER_ID(GetAddress16())) {
        BecomeDetached();
        ExitNow();
      }

      // Timeout
      TimeoutTlv timeout;
      SuccessOrExit(error = FindTlv(message, Tlv::kTypeTimeout, &timeout, sizeof(timeout)));
      VerifyOrExit(timeout.header.length == sizeof(timeout) - sizeof(timeout.header), error = kThreadError_Parse);

      timeout_ = HostSwap32(timeout.timeout);
      if ((mode.mode & kModeRxOnWhenIdle) == 0) {
        mesh_->SetPollPeriod((timeout_ / 2) * 1000U);
        mesh_->SetRxOnWhenIdle(false);
      } else {
        mesh_->SetRxOnWhenIdle(true);
      }

      break;
    }

    default: {
      assert(false);
      break;
    }
  }

exit:
  return error;
}

Child *Mle::GetChild(MacAddr16 address) {
  return NULL;
}

Child *Mle::GetChild(const MacAddr64 *address) {
  return NULL;
}

Child *Mle::GetChild(const MacAddress *address) {
  return NULL;
}

int Mle::GetChildIndex(const Child *child) {
  return -1;
}

Child *Mle::GetChildren(uint8_t *num_children) {
  if (num_children != NULL)
    *num_children = 0;
  return NULL;
}

Neighbor *Mle::GetNeighbor(MacAddr16 address) {
  return (parent_.state == Neighbor::kStateValid && parent_.valid.address16 == address) ? &parent_ : NULL;
}

Neighbor *Mle::GetNeighbor(const MacAddr64 *address) {
  return (parent_.state == Neighbor::kStateValid &&
          memcmp(&parent_.mac_addr, address, sizeof(parent_.mac_addr)) == 0) ? &parent_ : NULL;
}

Neighbor *Mle::GetNeighbor(const MacAddress *address) {
  switch (address->length) {
    case 2: return GetNeighbor(address->address16);
    case 8: return GetNeighbor(&address->address64);
  }
  return NULL;
}

Neighbor *Mle::GetNeighbor(const Ip6Address *address) {
  return NULL;
}

MacAddr16 Mle::GetNextHop(MacAddr16 destination) {
  return (parent_.state == Neighbor::kStateValid) ? parent_.valid.address16 : MacFrame::kShortAddrInvalid;
}

bool Mle::IsRoutingLocator(const Ip6Address *address) {
  return memcmp(&mesh_local_16_, address, 14) == 0;
}

Router *Mle::GetParent() {
  return &parent_;
}

ThreadError Mle::CheckReachability(MacAddr16 meshsrc, MacAddr16 meshdst, const Ip6Header *ip6_header) {
  if (meshdst != GetAddress16())
    return kThreadError_None;

  if (netif_->IsAddress(&ip6_header->ip6_dst))
    return kThreadError_None;

  Ip6Address dst;
  memcpy(&dst, GetMeshLocal16(), 14);
  dst.s6_addr16[7] = HostSwap16(meshsrc);
  Icmp6::SendError(&dst, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOROUTE, ip6_header);

  return kThreadError_Drop;
}

ThreadError Mle::HandleNetworkDataUpdate() {
  if (device_mode_ & kModeFFD)
    mle_router_->HandleNetworkDataUpdateRouter();
  return kThreadError_None;
}

}  // namespace Thread
