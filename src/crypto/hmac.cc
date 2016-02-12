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
#include <common/thread_error.h>
#include <crypto/hmac.h>

namespace Thread {

Hmac::Hmac(Hash *hash) {
  hash_ = hash;
}

ThreadError Hmac::SetKey(const void *key, uint16_t key_length) {
  if (key_length > kMaxKeyLength) {
    hash_->Init();
    hash_->Input(key, key_length);
    hash_->Finalize(key_);
    key_length_ = hash_->GetSize();
  } else {
    memcpy(key_, key, key_length);
    key_length_ = key_length;
  }

  return kThreadError_None;
}

ThreadError Hmac::Init() {
  uint8_t pad[kMaxKeyLength];
  int i;

  for (i = 0; i < key_length_; i++)
    pad[i] = key_[i] ^ 0x36;
  for (; i < 64; i++)
    pad[i] = 0x36;

  // start inner hash
  hash_->Init();
  hash_->Input(pad, sizeof(pad));

  return kThreadError_None;
}

ThreadError Hmac::Input(const void *buf, uint16_t buf_length) {
  return hash_->Input(buf, buf_length);
}

ThreadError Hmac::Finalize(uint8_t *hash) {
  uint8_t pad[kMaxKeyLength];
  int i;

  // finish inner hash
  hash_->Finalize(hash);

  // perform outer hash
  for (i = 0; i < key_length_; i++)
    pad[i] = key_[i] ^ 0x5c;
  for (; i < 64; i++)
    pad[i] = 0x5c;

  hash_->Init();
  hash_->Input(pad, kMaxKeyLength);
  hash_->Input(hash, hash_->GetSize());
  hash_->Finalize(hash);

  return kThreadError_None;
}

}  // namespace Thread
