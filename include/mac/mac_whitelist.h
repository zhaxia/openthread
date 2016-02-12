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

#ifndef MAC_MAC_WHITELIST_H_
#define MAC_MAC_WHITELIST_H_

#include <common/thread_error.h>
#include <mac/mac_frame.h>
#include <stdint.h>

namespace Thread {

class MacWhitelist {
 public:
  MacWhitelist();

  ThreadError Enable();
  ThreadError Disable();
  bool IsEnabled() const;

  int GetMaxEntries() const;

  int Add(const MacAddr64 *address);
  ThreadError Remove(const MacAddr64 *address);
  ThreadError Clear();

  int Find(const MacAddr64 *address) const;

  const uint8_t *GetAddress(uint8_t entry) const;

  ThreadError ClearRssi(uint8_t entry);
  ThreadError GetRssi(uint8_t entry, int8_t *rssi) const;
  ThreadError SetRssi(uint8_t entry, int8_t rssi);

 private:
  enum {
    kMaxEntries = 32,
  };

  struct Entry {
    MacAddr64 addr64;
    int8_t rssi;
    bool valid : 1;
    bool rssi_valid : 1;
  };
  Entry whitelist_[kMaxEntries];

  bool enabled_ = false;
};

}  // namespace Thread

#endif  // MAC_MAC_WHITELIST_H_
