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

#ifndef THREAD_THREAD_NETIF_H_
#define THREAD_THREAD_NETIF_H_

#include <common/thread_error.h>
#include <mac/mac.h>
#include <net/netif.h>
#include <thread/mesh_forwarder.h>
#include <thread/mle.h>
#include <thread/mle_router.h>
#include <thread/network_data_local.h>

namespace Thread {

class ThreadNetif: public Netif {
 public:
  ThreadNetif();
  ThreadError Init();
  ThreadError Up();
  ThreadError Down();
  bool IsUp() const;

  const char *GetName() const final;
  ThreadError GetLinkAddress(LinkAddress *address) const final;
  ThreadError SendMessage(Message *message) final;

  AddressResolver *GetAddressResolver();
  KeyManager *GetKeyManager();
  Mac *GetMac();
  MleRouter *GetMle();
  MeshForwarder *GetMeshForwarder();
  NetworkDataLocal *GetNetworkDataLocal();
  NetworkDataLeader *GetNetworkDataLeader();

 private:
  CoapServer coap_server_;
  AddressResolver address_resolver_;
  KeyManager key_manager_;
  Mac mac_;
  MeshForwarder mesh_forwarder_;
  MleRouter mle_router_;
  NetworkDataLocal network_data_local_;
  NetworkDataLeader network_data_leader_;
  bool is_up_;
};

struct ThreadMessageInfo {
  uint8_t link_margin;
};

}  // namespace Thread

#endif  // THREAD_THREAD_NETIF_H_
