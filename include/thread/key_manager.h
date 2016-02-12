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

#ifndef THREAD_KEY_MANAGER_H_
#define THREAD_KEY_MANAGER_H_

#include <common/thread_error.h>
#include <stdint.h>

namespace Thread {

class MleRouter;

class KeyManager {
 public:
  explicit KeyManager(MleRouter *mle);
  ThreadError GetMasterKey(void *key, uint8_t *key_length);
  ThreadError SetMasterKey(const void *key, uint8_t key_length);

  uint32_t GetCurrentKeySequence() const;
  ThreadError SetCurrentKeySequence(uint32_t key_sequence);
  const uint8_t *GetCurrentMacKey() const;
  const uint8_t *GetCurrentMleKey() const;

  bool IsPreviousKeyValid() const;
  uint32_t GetPreviousKeySequence() const;
  const uint8_t *GetPreviousMacKey() const;
  const uint8_t *GetPreviousMleKey() const;

  const uint8_t *GetTemporaryMacKey(uint32_t key_sequence);
  const uint8_t *GetTemporaryMleKey(uint32_t key_sequence);

  uint32_t GetMacFrameCounter() const;
  uint32_t GetMleFrameCounter() const;

  ThreadError IncrementMacFrameCounter();
  ThreadError IncrementMleFrameCounter();

 private:
  ThreadError ComputeKey(uint32_t key_sequence, uint8_t *key);

  uint8_t master_key_[16];
  uint8_t master_key_length_;

  uint32_t previous_key_sequence_;
  uint8_t previous_key_[32];
  bool previous_key_valid_ = false;

  uint32_t current_key_sequence_;
  uint8_t current_key_[32];

  uint8_t temporary_key_[32];

  uint32_t mac_frame_counter_ = 0;
  uint32_t mle_frame_counter_ = 0;

  MleRouter *mle_;
};

}  // namespace Thread

#endif  // THREAD_KEY_MANAGER_H_
