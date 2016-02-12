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

#ifndef NET_SOCKET_H_
#define NET_SOCKET_H_

#include <net/ip6_address.h>

namespace Thread {

typedef struct {
  Ip6Address sock_addr;
  Ip6Address peer_addr;
  uint16_t peer_port;
  uint16_t sock_port;
  uint8_t interface_id;
  uint8_t hop_limit;
  void *link_info;
} Ip6MessageInfo;

class Socket {
 protected:
  struct sockaddr_in6 sockname_;
  struct sockaddr_in6 peername_;
};

}  // namespace Thread

#endif  // NET_SOCKET_H_
