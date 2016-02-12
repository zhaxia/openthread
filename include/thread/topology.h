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

#ifndef THREAD_TOPOLOGY_H_
#define THREAD_TOPOLOGY_H_

#include <mac/mac_frame.h>
#include <net/ip6.h>

namespace Thread {

enum {
  kChildIdMask = 0x1ff,
  kRouterIdOffset = 10,
};

class Neighbor {
 public:
  MacAddr64 mac_addr;
  uint32_t last_heard;
  union {
    struct {
      uint32_t link_frame_counter;
      uint32_t mle_frame_counter;
      uint16_t address16;
    } valid;
    struct {
      uint8_t challenge[8];
      uint8_t challenge_length;
    } pending;
  };

  enum State {
    kStateInvalid = 0,
    kStateParentRequest = 1,
    kStateChildIdRequest = 2,
    kStateLinkRequest = 3,
    kStateValid = 4,
  };
  State state : 3;
  uint8_t mode : 4;
  bool previous_key : 1;
  bool frame_pending : 1;
  bool data_request : 1;
  bool allocated : 1;
  bool reclaim_delay : 1;
  int8_t rssi;
};

class Child : public Neighbor {
 public:
  enum {
    kMaxIp6AddressPerChild = 4,
  };
  Ip6Address ip6_address[kMaxIp6AddressPerChild];
  uint32_t timeout;
  uint16_t fragment_offset;
  uint8_t request_tlvs[4];
  uint8_t network_data_version;
};

class Router : public Neighbor {
 public:
  uint8_t nexthop;
  uint8_t link_quality_out : 2;
  uint8_t link_quality_in : 2;
  uint8_t cost : 4;
};

}  // namespace Thread

#endif  // THREAD_TOPOLOGY_H_
