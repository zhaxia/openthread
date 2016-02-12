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
#include <crypto/aes_ccm.h>

namespace Thread {

ThreadError AesCcm::Init(const AesEcb *ecb, uint32_t header_length, uint32_t plaintext_length, uint8_t tag_length,
                         const void *nonce, uint8_t nonce_length) {
  ThreadError error = kThreadError_None;
  const uint8_t *nonce_bytes = reinterpret_cast<const uint8_t*>(nonce);
  uint8_t block_length = 0;
  uint32_t len;
  uint8_t L;
  uint8_t i;

  ecb_ = ecb;

  // tag_length must be even
  tag_length &= ~1;
  if (tag_length > sizeof(block_))
    tag_length = sizeof(block_);

  VerifyOrExit(tag_length >= 4, error = kThreadError_InvalidArgs);

  L = 0;
  for (len = plaintext_length; len; len >>= 8)
    L++;
  if (L <= 1)
    L = 2;

  if (nonce_length > 13)
    nonce_length = 13;

  // increase L to match nonce len
  if (L < (15 - nonce_length))
    L = 15 - nonce_length;

  // decrease nonce_length to match L
  if (nonce_length > (15 - L))
    nonce_length = 15 - L;

  // setup initial block

  // write flags
  block_[0] = (static_cast<uint8_t>((header_length != 0) << 6) |
               static_cast<uint8_t>(((tag_length - 2) >> 1) << 3) |
               static_cast<uint8_t>(L-1));

  // write nonce
  for (i = 0; i < nonce_length; i++)
    block_[1 + i] = nonce_bytes[i];

  // write len
  len = plaintext_length;
  for (i = sizeof(block_)-1; i > nonce_length; i--) {
    block_[i] = len;
    len >>= 8;
  }

  // encrypt initial block
  SuccessOrExit(error = ecb_->Encrypt(block_, block_));

  // process header
  if (header_length > 0) {
    // process length
    if (header_length < (65536U - 256U)) {
      block_[block_length++] ^= header_length >> 8;
      block_[block_length++] ^= header_length >> 0;
    } else {
      block_[block_length++] ^= 0xff;
      block_[block_length++] ^= 0xfe;
      block_[block_length++] ^= header_length >> 24;
      block_[block_length++] ^= header_length >> 16;
      block_[block_length++] ^= header_length >> 8;
      block_[block_length++] ^= header_length >> 0;
    }
  }

  // init counter
  ctr_[0] = L-1;
  for (i = 0; i < nonce_length; i++)
    ctr_[1 + i] = nonce_bytes[i];
  for (i = i + 1; i < sizeof(ctr_); i++)
    ctr_[i] = 0;

  nonce_length_ = nonce_length;
  header_length_ = header_length;
  header_cur_ = 0;
  plaintext_length_ = plaintext_length;
  plaintext_cur_ = 0;
  block_length_ = block_length;
  ctr_length_ = sizeof(ctrpad_);
  tag_length_ = tag_length;

exit:
  return error;
}

ThreadError AesCcm::Header(const void *header, uint32_t header_length) {
  ThreadError error = kThreadError_None;
  const uint8_t *header_bytes = reinterpret_cast<const uint8_t*>(header);

  // process header
  for (int i = 0; i < header_length; i++) {
    if (block_length_ == sizeof(block_)) {
      SuccessOrExit(error = ecb_->Encrypt(block_, block_));
      block_length_ = 0;
    }
    block_[block_length_++] ^= header_bytes[i];
  }

  header_cur_ += header_length;
  if (header_cur_ >= header_length_) {
    // process remainder
    if (block_length_ != 0)
      SuccessOrExit(error = ecb_->Encrypt(block_, block_));
    block_length_ = 0;
  }

exit:
  return error;
}

ThreadError AesCcm::Payload(void *plaintext, void *ciphertext, uint32_t len, bool encrypt) {
  ThreadError error = kThreadError_None;
  uint8_t *plaintext_bytes = reinterpret_cast<uint8_t*>(plaintext);
  uint8_t *ciphertext_bytes = reinterpret_cast<uint8_t*>(ciphertext);
  uint8_t byte;

  VerifyOrExit(plaintext_cur_ + len <= plaintext_length_, error = kThreadError_InvalidArgs);
  VerifyOrExit(len != 0, error = kThreadError_InvalidArgs);

  for (int i = 0; i < len; i++) {
    if (ctr_length_ == 16) {
      for (int j = sizeof(ctr_) - 1; j > nonce_length_; j--) {
        if (++ctr_[j])
          break;
      }
      SuccessOrExit(error = ecb_->Encrypt(ctr_, ctrpad_));
      ctr_length_ = 0;
    }

    if (encrypt) {
      byte = plaintext_bytes[i];
      ciphertext_bytes[i] = byte ^ ctrpad_[ctr_length_++];
    } else {
      byte = ciphertext_bytes[i] ^ ctrpad_[ctr_length_++];
      plaintext_bytes[i] = byte;
    }

    if (block_length_ == sizeof(block_)) {
      SuccessOrExit(error = ecb_->Encrypt(block_, block_));
      block_length_ = 0;
    }

    block_[block_length_++] ^= byte;
  }

  plaintext_cur_ += len;
  if (plaintext_cur_ >= plaintext_length_) {
    if (block_length_ != 0)
      SuccessOrExit(error = ecb_->Encrypt(block_, block_));

    // reset counter
    for (uint8_t i = nonce_length_ + 1; i < sizeof(ctr_); i++)
      ctr_[i] = 0;
  }

exit:
  return error;
}

ThreadError AesCcm::Finalize(void *tag, uint8_t *tag_length) {
  ThreadError error = kThreadError_None;
  uint8_t *tag_bytes = reinterpret_cast<uint8_t*>(tag);

  VerifyOrExit(plaintext_length_ == plaintext_cur_, error = kThreadError_Error);

  if (tag_length_ > 0) {
    SuccessOrExit(error = ecb_->Encrypt(ctr_, ctrpad_));

    for (int i = 0; i < tag_length_; i++)
      tag_bytes[i] = block_[i] ^ ctrpad_[i];
  }

  if (tag_length)
    *tag_length = tag_length_;

exit:
  return error;
}

}  // namespace Thread
