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

#ifndef NET_IP6_MPL_H_
#define NET_IP6_MPL_H_

#include <common/message.h>
#include <common/thread_error.h>
#include <common/timer.h>
#include <net/ip6_address.h>

namespace Thread {

#define IP6OPT_MPL          0x6D    /* 01 1 01101 */

typedef struct ip6_opt_mpl {
  uint8_t ip6o_type;
  uint8_t ip6o_len;
  uint8_t mpl_control;
  uint8_t mpl_sequence;
  uint16_t mpl_seed;
} Ip6OptMpl;

#define IP6_MPL_SEED_0      (0 << 6)
#define IP6_MPL_SEED_2      (1 << 6)
#define IP6_MPL_SEED_8      (2 << 6)
#define IP6_MPL_SEED_16     (3 << 6)
#define IP6_MPL_MAX         (1 << 5)
#define IP6_MPL_VERSION     (1 << 4)

class Ip6Mpl {
 public:
  Ip6Mpl();
  ThreadError ProcessOption(const Message *message);

 private:
  enum {
    kNumEntries = 32,
    kLifetime = 5,  // seconds
  };

  static void HandleTimer(void *context);
  void HandleTimer();

  Timer timer_;

  struct MplEntry {
    uint16_t seed;
    uint8_t sequence;
    uint8_t lifetime;
  };
  MplEntry entries_[kNumEntries];
};

}  // namespace Thread

#endif  // NET_IP6_MPL_H_
