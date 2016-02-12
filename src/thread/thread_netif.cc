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
#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6.h>
#include <net/netif.h>
#include <net/udp6.h>
#include <thread/mle.h>
#include <thread/thread_netif.h>
#include <thread/thread_tlvs.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static const uint8_t kThreadMasterKey[] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static const char name[] = "thread";

ThreadNetif::ThreadNetif():
    coap_server_(kCoapUdpPort),
    address_resolver_(&mesh_forwarder_, &coap_server_, &mle_router_, this),
    key_manager_(&mle_router_),
    mac_(&key_manager_, &mle_router_),
    mesh_forwarder_(&address_resolver_, &mac_, &mle_router_, this, &network_data_leader_),
    mle_router_(&address_resolver_, &coap_server_, &key_manager_, &mesh_forwarder_, &network_data_leader_, this),
    network_data_local_(&mle_router_),
    network_data_leader_(&coap_server_, this, &mle_router_) {
  key_manager_.SetMasterKey(kThreadMasterKey, sizeof(kThreadMasterKey));
}

const char *ThreadNetif::GetName() const {
  return name;
}

ThreadError ThreadNetif::Init() {
  mac_.Init();
  mle_router_.Init();
  return kThreadError_None;
}

ThreadError ThreadNetif::Up() {
  Netif::AddNetif();
  mesh_forwarder_.Start();
  mle_router_.Start();
  coap_server_.Start();
  is_up_ = true;
  return kThreadError_None;
}

ThreadError ThreadNetif::Down() {
  Netif::RemoveNetif();
  coap_server_.Stop();
  mle_router_.Stop();
  mesh_forwarder_.Stop();
  is_up_ = false;
  return kThreadError_None;
}

bool ThreadNetif::IsUp() const {
  return is_up_;
}

ThreadError ThreadNetif::GetLinkAddress(LinkAddress *address) const {
  address->type = LinkAddress::kEui64;
  address->length = 8;
  memcpy(&address->address64, mac_.GetAddress64(), address->length);
  return kThreadError_None;
}

AddressResolver *ThreadNetif::GetAddressResolver() {
  return &address_resolver_;
}

KeyManager *ThreadNetif::GetKeyManager() {
  return &key_manager_;
}

Mac *ThreadNetif::GetMac() {
  return &mac_;
}

MleRouter *ThreadNetif::GetMle() {
  return &mle_router_;
}

MeshForwarder *ThreadNetif::GetMeshForwarder() {
  return &mesh_forwarder_;
}

NetworkDataLocal *ThreadNetif::GetNetworkDataLocal() {
  return &network_data_local_;
}

NetworkDataLeader *ThreadNetif::GetNetworkDataLeader() {
  return &network_data_leader_;
}

ThreadError ThreadNetif::SendMessage(Message *message) {
  return mesh_forwarder_.SendMessage(message);
}

}  // namespace Thread
