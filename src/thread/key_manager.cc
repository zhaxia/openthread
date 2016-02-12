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

#include <thread/key_manager.h>
#include <common/code_utils.h>
#include <common/thread_error.h>
#include <crypto/hmac.h>
#include <crypto/sha256.h>
#include <thread/mle_router.h>

namespace Thread {

static const uint8_t kThreadString[] = {
  'T', 'h', 'r', 'e', 'a', 'd',
};

KeyManager::KeyManager(MleRouter *mle) {
  mle_ = mle;
}

ThreadError KeyManager::GetMasterKey(void *key, uint8_t *key_length) {
  if (key)
    memcpy(key, master_key_, master_key_length_);
  if (key_length)
    *key_length = master_key_length_;
  return kThreadError_None;
}

ThreadError KeyManager::SetMasterKey(const void *key, uint8_t key_length) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(key_length <= sizeof(master_key_), error = kThreadError_InvalidArgs);
  memcpy(master_key_, key, key_length);
  master_key_length_ = key_length;
  current_key_sequence_ = 0;
  ComputeKey(current_key_sequence_, current_key_);

exit:
  return error;
}

ThreadError KeyManager::ComputeKey(uint32_t key_sequence, uint8_t *key) {
  Sha256 sha256;
  Hmac hmac(&sha256);

  hmac.SetKey(master_key_, master_key_length_);
  hmac.Init();

  uint8_t key_sequence_bytes[4];
  key_sequence_bytes[0] = key_sequence >> 24;
  key_sequence_bytes[1] = key_sequence >> 16;
  key_sequence_bytes[2] = key_sequence >> 8;
  key_sequence_bytes[3] = key_sequence >> 0;
  hmac.Input(key_sequence_bytes, sizeof(key_sequence_bytes));
  hmac.Input(kThreadString, sizeof(kThreadString));

  hmac.Finalize(key);

  return kThreadError_None;
}

uint32_t KeyManager::GetCurrentKeySequence() const {
  return current_key_sequence_;
}

ThreadError KeyManager::SetCurrentKeySequence(uint32_t key_sequence) {
  ThreadError error = kThreadError_None;

  previous_key_valid_ = true;
  previous_key_sequence_ = current_key_sequence_;
  memcpy(previous_key_, current_key_, sizeof(previous_key_));

  current_key_sequence_ = key_sequence;
  ComputeKey(current_key_sequence_, current_key_);

  mac_frame_counter_ = 0;
  mle_frame_counter_ = 0;

  uint8_t num_neighbors;

  Router *routers;
  routers = mle_->GetParent();
  routers->previous_key = true;

  routers = mle_->GetRouters(&num_neighbors);
  for (int i = 0; i < num_neighbors; i++)
    routers[i].previous_key = true;

  Child *children;
  children = mle_->GetChildren(&num_neighbors);
  for (int i = 0; i < num_neighbors; i++)
    children[i].previous_key = true;

  return error;
}

const uint8_t *KeyManager::GetCurrentMacKey() const {
  return current_key_ + 16;
}

const uint8_t *KeyManager::GetCurrentMleKey() const {
  return current_key_;
}

bool KeyManager::IsPreviousKeyValid() const {
  return previous_key_valid_;
}

uint32_t KeyManager::GetPreviousKeySequence() const {
  return previous_key_sequence_;
}

const uint8_t *KeyManager::GetPreviousMacKey() const {
  return previous_key_ + 16;
}

const uint8_t *KeyManager::GetPreviousMleKey() const {
  return previous_key_;
}

const uint8_t *KeyManager::GetTemporaryMacKey(uint32_t key_sequence) {
  ComputeKey(key_sequence, temporary_key_);
  return temporary_key_ + 16;
}

const uint8_t *KeyManager::GetTemporaryMleKey(uint32_t key_sequence) {
  ComputeKey(key_sequence, temporary_key_);
  return temporary_key_;
}

uint32_t KeyManager::GetMacFrameCounter() const {
  return mac_frame_counter_;
}

uint32_t KeyManager::GetMleFrameCounter() const {
  return mle_frame_counter_;
}

ThreadError KeyManager::IncrementMacFrameCounter() {
  mac_frame_counter_++;
  return kThreadError_None;
}

ThreadError KeyManager::IncrementMleFrameCounter() {
  mle_frame_counter_++;
  return kThreadError_None;
}

}  // namespace Thread
