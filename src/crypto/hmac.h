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

#ifndef CRYPTO_HMAC_H_
#define CRYPTO_HMAC_H_

#include <common/thread_error.h>
#include <crypto/hash.h>
#include <stdint.h>

namespace Thread {

class Hmac {
 public:
  explicit Hmac(Hash *hash);
  ThreadError SetKey(const void *key, uint16_t key_length);
  ThreadError Init();
  ThreadError Input(const void *buf, uint16_t buf_length);
  ThreadError Finalize(uint8_t *hash);

 private:
  enum {
    kMaxKeyLength = 64,
  };
  uint8_t key_[kMaxKeyLength];
  uint8_t key_length_ = 0;
  Hash *hash_;
};

}  // namespace Thread

#endif  // CRYPTO_HMAC_H_
