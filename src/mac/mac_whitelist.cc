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
#include <mac/mac_whitelist.h>

namespace Thread {

MacWhitelist::MacWhitelist() {
  for (int i = 0; i < kMaxEntries; i++)
    whitelist_[i].valid = false;
}

ThreadError MacWhitelist::Enable() {
  enabled_ = true;
  return kThreadError_None;
}

ThreadError MacWhitelist::Disable() {
  enabled_ = false;
  return kThreadError_None;
}

bool MacWhitelist::IsEnabled() const {
  return enabled_;
}

int MacWhitelist::GetMaxEntries() const {
  return kMaxEntries;
}

int MacWhitelist::Add(const MacAddr64 *address) {
  int rval = -1;

  VerifyOrExit((rval = Find(address)) < 0, ;);

  for (int i = 0; i < kMaxEntries; i++) {
    if (whitelist_[i].valid)
      continue;
    memcpy(&whitelist_[i], address, sizeof(whitelist_[i]));
    whitelist_[i].valid = true;
    whitelist_[i].rssi_valid = false;
    ExitNow(rval = i);
  }

exit:
  return rval;
}

ThreadError MacWhitelist::Clear() {
  for (int i = 0; i < kMaxEntries; i++)
    whitelist_[i].valid = false;
  return kThreadError_None;
}

ThreadError MacWhitelist::Remove(const MacAddr64 *address) {
  ThreadError error = kThreadError_None;
  int i;

  VerifyOrExit((i = Find(address)) >= 0, ;);
  memset(&whitelist_[i], 0, sizeof(whitelist_[i]));

exit:
  return error;
}

int MacWhitelist::Find(const MacAddr64 *address) const {
  int rval = -1;

  for (int i = 0; i < kMaxEntries; i++) {
    if (!whitelist_[i].valid)
      continue;
    if (memcmp(&whitelist_[i].addr64, address, sizeof(whitelist_[i].addr64)) == 0)
      ExitNow(rval = i);
  }

exit:
  return rval;
}

const uint8_t *MacWhitelist::GetAddress(uint8_t entry) const {
  const uint8_t *rval;

  VerifyOrExit(entry < kMaxEntries, rval = NULL);
  rval = whitelist_[entry].addr64.bytes;

exit:
  return rval;
}

ThreadError MacWhitelist::ClearRssi(uint8_t entry) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(entry < kMaxEntries, error = kThreadError_Error);
  whitelist_[entry].rssi_valid = false;

exit:
  return error;
}

ThreadError MacWhitelist::GetRssi(uint8_t entry, int8_t *rssi) const {
  ThreadError error = kThreadError_None;

  VerifyOrExit(entry < kMaxEntries && whitelist_[entry].valid && whitelist_[entry].rssi_valid,
               error = kThreadError_Error);

  if (rssi != NULL)
    *rssi = whitelist_[entry].rssi;

exit:
  return error;
}

ThreadError MacWhitelist::SetRssi(uint8_t entry, int8_t rssi) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(entry < kMaxEntries, error = kThreadError_Error);
  whitelist_[entry].rssi_valid = true;
  whitelist_[entry].rssi = rssi;

exit:
  return error;
}

}  // namespace Thread
