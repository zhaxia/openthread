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

#ifndef CRYPTO_AES_ECB_H_
#define CRYPTO_AES_ECB_H_

#include <common/thread_error.h>
#include <crypto/aes.h>
#include <stdint.h>

namespace Thread {

class AesEcb {
 public:
  ThreadError SetKey(const uint8_t *key, uint16_t keylen);
  ThreadError Encrypt(const uint8_t *pt, uint8_t *ct) const;

 private:
  uint32_t eK_[44];
};

}  // namespace Thread

#endif  // CRYPTO_AES_ECB_H_
