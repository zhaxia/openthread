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

#ifndef CRYPTO_AES_CCM_H_
#define CRYPTO_AES_CCM_H_

#include <common/thread_error.h>
#include <crypto/aes_ecb.h>
#include <stdint.h>

namespace Thread {

class AesCcm {
 public:
  ThreadError Init(const AesEcb *ecb, uint32_t header_length, uint32_t plaintext_length, uint8_t tag_length,
                   const void *nonce, uint8_t nonce_length);
  ThreadError Header(const void *header, uint32_t header_length);
  ThreadError Payload(void *plaintext, void *ciphertext, uint32_t length, bool encrypt);
  ThreadError Finalize(void *tag, uint8_t *tag_length);

 private:
  const AesEcb *ecb_;
  uint8_t block_[16];
  uint8_t ctr_[16];
  uint8_t ctrpad_[16];
  uint8_t nonce_length_;
  uint32_t header_length_;
  uint32_t header_cur_;
  uint32_t plaintext_length_;
  uint32_t plaintext_cur_;
  uint16_t block_length_;
  uint16_t ctr_length_;
  uint8_t tag_length_;
};

}  // namespace Thread

#endif  // CRYPTO_AES_CCM_H_
