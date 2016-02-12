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

#ifndef THREAD_NETWORK_DATA_LEADER_H_
#define THREAD_NETWORK_DATA_LEADER_H_

#include <coap/coap_server.h>
#include <common/timer.h>
#include <net/ip6_address.h>
#include <thread/network_data.h>
#include <stdint.h>

namespace Thread {

class Mle;
class Netif;

class NetworkDataLeader: public NetworkData {
 public:
  NetworkDataLeader(CoapServer *coap_server, Netif *netif, Mle *mle);
  ThreadError Init();
  ThreadError Start();
  ThreadError Stop();

  uint8_t GetVersion() const;
  ThreadError SetVersion(uint8_t version);
  uint8_t GetStableVersion() const;
  ThreadError SetStableVersion(uint8_t stable_version);

  uint32_t GetContextIdReuseDelay() const;
  ThreadError SetContextIdReuseDelay(uint32_t delay);

  ThreadError GetContext(const Ip6Address &address, Context *context);
  ThreadError GetContext(uint8_t context_id, Context *context);

  bool IsOnMesh(const Ip6Address &address);
  ThreadError RouteLookup(const Ip6Address &source, const Ip6Address &destination, uint16_t *rloc);
  ThreadError SetNetworkData(uint8_t version, uint8_t stable_version, bool stable_only,
                             const uint8_t *data, uint8_t data_length);
  ThreadError RemoveBorderRouter(uint16_t rloc);

 private:
  static void HandleServerData(void *context, CoapMessage *coap, Message *message,
                               const Ip6MessageInfo *message_info);
  void HandleServerData(CoapMessage *coap, Message *message, const Ip6MessageInfo *message_info);

  static void HandleTimer(void *context);
  void HandleTimer();

  ThreadError RegisterNetworkData(uint16_t rloc, uint8_t *tlvs, uint8_t tlvs_length);

  ThreadError AddBorderRouter(PrefixTlv *prefix, BorderRouterTlv *src);
  ThreadError AddNetworkData(uint8_t *tlv, uint8_t tlv_length);
  ThreadError AddPrefix(PrefixTlv *tlv);

  int AllocateContext();
  ThreadError FreeContext(uint8_t context_id);

  ThreadError ConfigureAddresses();
  ThreadError ConfigureAddress(PrefixTlv *prefix);

  ContextTlv *FindContext(PrefixTlv *prefix);

  ThreadError RemoveContext(uint8_t context_id);
  ThreadError RemoveContext(PrefixTlv *prefix, uint8_t context_id);

  ThreadError RemoveRloc(uint16_t rloc);
  ThreadError RemoveRloc(PrefixTlv *prefix, uint16_t rloc);

  ThreadError RouteLookup(PrefixTlv *prefix, uint16_t *rloc);

  CoapServer::Resource server_data_;
  uint8_t stable_version_;
  uint8_t version_;

  NetifAddress addresses_[4];

  enum {
    kMinContextId = 1,
    kNumContextIds = 15,
    kContextIdReuseDelay = 48 * 60 * 60,
  };
  uint16_t context_used_ = 0;
  uint32_t context_last_used_[kNumContextIds];
  uint32_t context_id_reuse_delay_ = kContextIdReuseDelay;
  Timer timer_;

  CoapServer *coap_server_;
  Netif *netif_;
  Mle *mle_;
};

}  // namespace Thread

#endif  // THREAD_NETWORK_DATA_LEADER_H_
