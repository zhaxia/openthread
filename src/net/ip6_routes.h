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

#ifndef NET_IP6_ROUTES_H_
#define NET_IP6_ROUTES_H_

#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6_address.h>

namespace Thread {

struct Ip6Route {
  Ip6Address prefix;
  uint8_t prefix_length;
  uint8_t interface_id;
  struct Ip6Route *next;
};

class Ip6Routes {
 public:
  static ThreadError Add(Ip6Route *route);
  static ThreadError Remove(Ip6Route *route);
  static const Ip6Route *Lookup(const Ip6Address *source, const Ip6Address *destination);
};

}  // namespace Thread

#endif  // NET_IP6_ROUTES_H_
