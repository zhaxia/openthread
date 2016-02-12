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
#include <crypto/sha256.h>

namespace Thread {

static const uint32_t K[64] = {
  0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
  0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
  0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
  0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
  0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
  0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
  0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
  0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
  0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
  0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
  0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
  0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
  0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

#define RORc(x, y)      ((static_cast<uint32_t>(x) >> static_cast<uint32_t>(y)) | \
                         (static_cast<uint32_t>(x) << static_cast<uint32_t>(32-(y))))
#define Ch(x, y, z)     ((z) ^ ((x) & ((y) ^ (z))))
#define Maj(x, y, z)    ((((x) | (y)) & (z)) | ((x) & (y)))
#define S(x, n)         RORc((x), (n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S((x), 2) ^ S((x), 13) ^ S((x), 22))
#define Sigma1(x)       (S((x), 6) ^ S((x), 11) ^ S((x), 25))
#define Gamma0(x)       (S((x), 7) ^ S((x), 18) ^ R((x), 3))
#define Gamma1(x)       (S((x), 17) ^ S((x), 19) ^ R((x), 10))

uint16_t Sha256::GetSize() const {
  return kHashSize;
}

ThreadError Sha256::Init() {
  length_lo_ = 0;
  length_hi_ = 0;
  block_index_ = 0;

  hash_[0] = 0x6A09E667UL;
  hash_[1] = 0xBB67AE85UL;
  hash_[2] = 0x3C6EF372UL;
  hash_[3] = 0xA54FF53AUL;
  hash_[4] = 0x510E527FUL;
  hash_[5] = 0x9B05688CUL;
  hash_[6] = 0x1F83D9ABUL;
  hash_[7] = 0x5BE0CD19UL;

  return kThreadError_None;
}

ThreadError Sha256::Input(const void *buf, uint16_t buf_length) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t*>(buf);

  while (buf_length--) {
    block_[block_index_] = *bytes++;
    block_index_++;
    length_lo_ += 8;
    if (length_lo_ == 0)
      length_hi_++;

    if (block_index_ == 64)
      ProcessBlock();
  }

  return kThreadError_None;
}


ThreadError Sha256::Finalize(uint8_t *hash) {
  PadMessage();

  memset(block_, 0, 64);
  length_lo_ = 0;
  length_hi_ = 0;

  for (int i = 0; i < kHashSize; i++)
    hash[i] = hash_[i >> 2] >> (8 * (3 - (i & 0x03)));

  return kThreadError_None;
}

void Sha256::PadMessage() {
  block_[block_index_++] = 0x80;

  if (block_index_ > 56) {
    while (block_index_ < 64)
      block_[block_index_++] = 0;
    ProcessBlock();
  }

  while (block_index_ < 56)
    block_[block_index_++] = 0;

  block_[56] = length_hi_ >> 24;
  block_[57] = length_hi_ >> 16;
  block_[58] = length_hi_ >> 8;
  block_[59] = length_hi_ >> 0;
  block_[60] = length_lo_ >> 24;
  block_[61] = length_lo_ >> 16;
  block_[62] = length_lo_ >> 8;
  block_[63] = length_lo_ >> 0;

  ProcessBlock();
}

void Sha256::ProcessBlock() {
  uint32_t S[8], W[64], t0, t1;
  uint32_t t;

  for (int i = 0; i < 8; i++) {
    S[i] = hash_[i];
  }

  for (int i = 0; i < 16; i++) {
    W[i] =
        (static_cast<uint32_t>(block_[i * 4 + 0]) << 24) |
        (static_cast<uint32_t>(block_[i * 4 + 1]) << 16) |
        (static_cast<uint32_t>(block_[i * 4 + 2]) << 8) |
        (static_cast<uint32_t>(block_[i * 4 + 3]) << 0);
  }

  for (int i = 16; i < 64; i++) {
    W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
  }

#define RND(a, b, c, d, e, f, g, h, i)            \
  t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i]; \
  t1 = Sigma0(a) + Maj(a, b, c);                  \
  d += t0;                                        \
  h  = t0 + t1;

  for (int i = 0; i < 64; ++i) {
    RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i);
    t = S[7]; S[7] = S[6]; S[6] = S[5]; S[5] = S[4];
    S[4] = S[3]; S[3] = S[2]; S[2] = S[1]; S[1] = S[0]; S[0] = t;
  }

  for (int i = 0; i < 8; i++) {
    hash_[i] += S[i];
  }

  block_index_ = 0;
}

}  // namespace Thread
