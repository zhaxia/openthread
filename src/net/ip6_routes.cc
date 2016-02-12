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

#include <net/ip6_routes.h>
#include <common/code_utils.h>
#include <common/message.h>

namespace Thread {

static Ip6Route *routes_ = NULL;

ThreadError Ip6Routes::Add(Ip6Route *route) {
  ThreadError error = kThreadError_None;

  for (Ip6Route *cur = routes_; cur; cur = cur->next)
    VerifyOrExit(cur != route, error = kThreadError_Busy);

  route->next = routes_;
  routes_ = route;

exit:
  return error;
}

ThreadError Ip6Routes::Remove(Ip6Route *route) {
  if (route == routes_) {
    routes_ = route->next;
  } else {
    for (Ip6Route *cur = routes_; cur; cur = cur->next) {
      if (cur->next == route) {
        cur->next = route->next;
        break;
      }
    }
  }

  route->next = NULL;

  return kThreadError_None;
}

const Ip6Route *Ip6Routes::Lookup(const Ip6Address *source, const Ip6Address *destination) {
  int max_prefix_match = -1;
  Ip6Route *rval = NULL;

  for (Ip6Route *cur = routes_; cur; cur = cur->next) {
    uint8_t prefix_match;

    prefix_match = cur->prefix.PrefixMatch(destination);
    if (prefix_match < cur->prefix_length)
      continue;
    if (prefix_match > cur->prefix_length)
      prefix_match = cur->prefix_length;

    if (max_prefix_match > prefix_match)
      continue;

    max_prefix_match = prefix_match;
    rval = cur;
  }

  return rval;
}

}  // namespace Thread
