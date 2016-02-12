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

#include <common/code_utils.h>
#include <common/message.h>
#include <net/ip6_mpl.h>

namespace Thread {

Ip6Mpl::Ip6Mpl():
    timer_(&HandleTimer, this) {
  memset(entries_, 0, sizeof(entries_));
}

ThreadError Ip6Mpl::ProcessOption(const Message *message) {
  ThreadError error = kThreadError_None;
  Ip6OptMpl option;
  MplEntry *entry = NULL;
  VerifyOrExit(message->Read(message->GetOffset(), sizeof(option), &option) == sizeof(option) &&
               option.ip6o_len == sizeof(Ip6OptMpl) - offsetof(Ip6OptMpl, mpl_control),
               error = kThreadError_Drop);

  for (int i = 0; i < kNumEntries; i++) {
    if (entries_[i].lifetime == 0) {
      entry = &entries_[i];
    } else if (entries_[i].seed == option.mpl_seed) {
      entry = &entries_[i];

      int8_t diff;
      diff = option.mpl_sequence - entry->sequence;
      if (diff <= 0)
        error = kThreadError_Drop;

      break;
    }
  }

  VerifyOrExit(entry != NULL, error = kThreadError_Drop);

  entry->seed = option.mpl_seed;
  entry->sequence = option.mpl_sequence;
  entry->lifetime = kLifetime;
  timer_.Start(1000);

exit:
  return error;
}

void Ip6Mpl::HandleTimer(void *context) {
  Ip6Mpl *obj = reinterpret_cast<Ip6Mpl*>(context);
  obj->HandleTimer();
}

void Ip6Mpl::HandleTimer() {
  bool start_timer = false;

  for (int i = 0; i < kNumEntries; i++) {
    if (entries_[i].lifetime > 0) {
      entries_[i].lifetime--;
      start_timer = true;
    }
  }

  if (start_timer)
    timer_.Start(1000);
}

}  // namespace Thread
