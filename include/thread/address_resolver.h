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

#ifndef THREAD_ADDRESS_RESOLVER_H_
#define THREAD_ADDRESS_RESOLVER_H_

#include <coap/coap_server.h>
#include <common/thread_error.h>
#include <common/timer.h>
#include <mac/mac.h>
#include <net/icmp6.h>
#include <net/udp6.h>
#include <thread/network_data_leader.h>

namespace Thread {

class MeshForwarder;
class MleRouter;
class ThreadTargetEidTlv;
class ThreadMeshLocalIidTlv;

class AddressResolver {
 public:
  class Cache {
   public:
    Ip6Address target;
    uint8_t iid[8];
    MacAddr16 rloc;
    uint8_t timeout;
    uint8_t failure_count : 4;
    enum State {
      kStateInvalid = 0,
      kStateDiscover = 1,
      kStateRetry = 2,
      kStateValid = 3,
    };
    State state : 2;
  };

  AddressResolver(MeshForwarder *mesh_forwarder, CoapServer *coap_server, MleRouter *mle, Netif *netif);
  ThreadError Clear();
  ThreadError Remove(uint8_t router_id);
  ThreadError Resolve(const Ip6Address *eid, MacAddr16 *rloc);

  const Cache *GetCacheEntries(uint16_t *num_entries) const;

 private:
  enum {
    kCacheEntries = 8,
    kDiscoverTimeout = 3,  // seconds
  };

  ThreadError SendAddressQuery(const Ip6Address *eid);
  ThreadError SendAddressError(const ThreadTargetEidTlv *target, const ThreadMeshLocalIidTlv *eid,
                               const Ip6Address *destination);
  ThreadError FindTlv(Message *message, uint8_t type, void *buf, uint16_t buf_length);

  static void RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info);

  static void HandleAddressError(void *context, CoapMessage *coap,
                                 Message *message, const Ip6MessageInfo *message_info);
  void HandleAddressError(CoapMessage *coap, Message *message, const Ip6MessageInfo *message_info);

  static void HandleAddressQuery(void *context, CoapMessage *coap,
                                 Message *message, const Ip6MessageInfo *message_info);
  void HandleAddressQuery(CoapMessage *coap, Message *message, const Ip6MessageInfo *message_info);

  static void HandleAddressNotification(void *context, CoapMessage *coap,
                                        Message *message, const Ip6MessageInfo *message_info);
  void HandleAddressNotification(CoapMessage *coap, Message *message, const Ip6MessageInfo *message_info);

  static void HandleDstUnreach(void *context, Message *message, const Icmp6Header *icmp6_header,
                               const Ip6MessageInfo *message_info);
  void HandleDstUnreach(Message *message, const Icmp6Header *icmp6_header, const Ip6MessageInfo *message_info);

  static void HandleTimer(void *context);
  void HandleTimer();

  CoapServer::Resource address_error_;
  CoapServer::Resource address_query_;
  CoapServer::Resource address_notification_;
  Cache cache_[kCacheEntries];
  uint16_t coap_message_id_;
  uint8_t coap_token_[2];
  Icmp6::Callbacks icmp6_callbacks_;
  Udp6Socket socket_;
  Timer timer_;

  MeshForwarder *mesh_forwarder_;
  CoapServer *coap_server_;
  MleRouter *mle_;
  Netif *netif_;
};

}  // namespace Thread

#endif  // THREAD_ADDRESS_RESOLVER_H_
