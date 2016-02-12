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

#ifndef NET_UDP6_H_
#define NET_UDP6_H_

#include <net/ip6.h>

namespace Thread {

class Udp6Socket: private Socket {
  friend class Udp6;

 public:
  typedef void (*RecvFrom)(void *context, Message *message, const Ip6MessageInfo *message_info);

  Udp6Socket(RecvFrom callback, void *context);
  ThreadError Bind(const struct sockaddr_in6 *address);
  ThreadError Close();
  ThreadError SendTo(Message *message, const Ip6MessageInfo *message_info);

 private:
  RecvFrom callback_;
  void *context_;
  Udp6Socket *next_;
};

class Udp6 {
 public:
  static Message *NewMessage(uint16_t reserved);
  static ThreadError HandleMessage(Message *message, Ip6MessageInfo *message_info);
  static ThreadError UpdateChecksum(Message *message, uint16_t pseudoheader_checksum);
};

typedef struct udphdr {
  uint16_t source;
  uint16_t dest;
  uint16_t len;
  uint16_t check;
} UdpHeader;

}  // namespace Thread

#endif  // NET_UDP6_H_
