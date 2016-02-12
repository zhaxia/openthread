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

#ifndef NET_IP6_ADDRESS_H_
#define NET_IP6_ADDRESS_H_

#include <common/thread_error.h>
#include <stdint.h>

namespace Thread {

class Ip6Address {
 public:
  union {
    uint8_t  u6_addr8[16];
    uint16_t u6_addr16[8];
    uint32_t u6_addr32[4];
  } in6_u;

  enum {
    kNodeLocalScope = 0,
    kInterfaceLocalScope = 1,
    kLinkLocalScope = 2,
    kRealmLocalScope = 3,
    kSiteLocalScope = 5,
    kOrgLocalScoep = 8,
    kGlobalScope = 14,
  };

  bool IsUnspecified() const;
  bool IsLoopback() const;
  bool IsInterfaceLocal() const;
  bool IsLinkLocal() const;

  bool IsMulticast() const;
  bool IsLinkLocalMulticast() const;
  bool IsLinkLocalAllNodesMulticast() const;
  bool IsLinkLocalAllRoutersMulticast() const;
  bool IsRealmLocalMulticast() const;
  bool IsRealmLocalAllNodesMulticast() const;
  bool IsRealmLocalAllRoutersMulticast() const;

  uint8_t GetScope() const;

  uint8_t PrefixMatch(const Ip6Address *other) const;

  bool operator==(const Ip6Address& other) const;
  bool operator!=(const Ip6Address& other) const;

  ThreadError FromString(const char *buf);
  const char *ToString(char *buf, uint16_t size) const;
};

struct sockaddr_in6 {
  uint16_t   sin6_port;
  Ip6Address sin6_addr;
  uint8_t    sin6_scope_id;
};

#define s6_addr    in6_u.u6_addr8
#define s6_addr16  in6_u.u6_addr16
#define s6_addr32  in6_u.u6_addr32

}  // namespace Thread

#endif  // NET_IP6_ADDRESS_H_
