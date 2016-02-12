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

#ifndef THREAD_NETWORK_DATA_LOCAL_H_
#define THREAD_NETWORK_DATA_LOCAL_H_

#include <net/udp6.h>
#include <thread/network_data.h>

namespace Thread {

class Mle;

class NetworkDataLocal: public NetworkData {
 public:
  explicit NetworkDataLocal(Mle *mle);
  ThreadError AddOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length, uint8_t flags, bool stable);
  ThreadError RemoveOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length);

  ThreadError Register(const Ip6Address *destination);

 private:
  static void RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info);
  void RecvFrom(Message *message, const Ip6MessageInfo *message_info);

  ThreadError UpdateRloc();
  ThreadError UpdateRloc(PrefixTlv *prefix);
  ThreadError UpdateRloc(BorderRouterTlv *border_router);

  Udp6Socket socket_;
  uint8_t coap_token_[2];
  uint16_t coap_message_id_;

  Mle *mle_;
};

}  // namespace Thread

#endif  // THREAD_NETWORK_DATA_LOCAL_H_
