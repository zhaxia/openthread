/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 * @file
 *   This file implements SHA-256.
 */

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>
#include <crypto/sha256.hpp>

namespace Thread {
namespace Crypto {

static const uint32_t K[64] =
{
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

uint16_t Sha256::GetSize() const
{
    return kHashSize;
}

ThreadError Sha256::Init()
{
    mLengthLo = 0;
    mLengthHi = 0;
    mBlockIndex = 0;

    mHash[0] = 0x6A09E667UL;
    mHash[1] = 0xBB67AE85UL;
    mHash[2] = 0x3C6EF372UL;
    mHash[3] = 0xA54FF53AUL;
    mHash[4] = 0x510E527FUL;
    mHash[5] = 0x9B05688CUL;
    mHash[6] = 0x1F83D9ABUL;
    mHash[7] = 0x5BE0CD19UL;

    return kThreadError_None;
}

ThreadError Sha256::Input(const void *buf, uint16_t bufLength)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(buf);

    while (bufLength--)
    {
        mBlock[mBlockIndex] = *bytes++;
        mBlockIndex++;
        mLengthLo += 8;

        if (mLengthLo == 0)
        {
            mLengthHi++;
        }

        if (mBlockIndex == 64)
        {
            ProcessBlock();
        }
    }

    return kThreadError_None;
}


ThreadError Sha256::Finalize(uint8_t *hash)
{
    PadMessage();

    memset(mBlock, 0, 64);
    mLengthLo = 0;
    mLengthHi = 0;

    for (int i = 0; i < kHashSize; i++)
    {
        hash[i] = mHash[i >> 2] >> (8 * (3 - (i & 0x03)));
    }

    return kThreadError_None;
}

void Sha256::PadMessage()
{
    mBlock[mBlockIndex++] = 0x80;

    if (mBlockIndex > 56)
    {
        while (mBlockIndex < 64)
        {
            mBlock[mBlockIndex++] = 0;
        }

        ProcessBlock();
    }

    while (mBlockIndex < 56)
    {
        mBlock[mBlockIndex++] = 0;
    }

    mBlock[56] = mLengthHi >> 24;
    mBlock[57] = mLengthHi >> 16;
    mBlock[58] = mLengthHi >> 8;
    mBlock[59] = mLengthHi >> 0;
    mBlock[60] = mLengthLo >> 24;
    mBlock[61] = mLengthLo >> 16;
    mBlock[62] = mLengthLo >> 8;
    mBlock[63] = mLengthLo >> 0;

    ProcessBlock();
}

void Sha256::ProcessBlock()
{
    uint32_t S[8], W[64], t0, t1;
    uint32_t t;

    for (int i = 0; i < 8; i++)
    {
        S[i] = mHash[i];
    }

    for (int i = 0; i < 16; i++)
    {
        W[i] =
            (static_cast<uint32_t>(mBlock[i * 4 + 0]) << 24) |
            (static_cast<uint32_t>(mBlock[i * 4 + 1]) << 16) |
            (static_cast<uint32_t>(mBlock[i * 4 + 2]) << 8) |
            (static_cast<uint32_t>(mBlock[i * 4 + 3]) << 0);
    }

    for (int i = 16; i < 64; i++)
    {
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
    }

#define RND(a, b, c, d, e, f, g, h, i)            \
  t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i]; \
  t1 = Sigma0(a) + Maj(a, b, c);                  \
  d += t0;                                        \
  h  = t0 + t1;

    for (int i = 0; i < 64; ++i)
    {
        RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i);
        t = S[7];
        S[7] = S[6];
        S[6] = S[5];
        S[5] = S[4];
        S[4] = S[3];
        S[3] = S[2];
        S[2] = S[1];
        S[1] = S[0];
        S[0] = t;
    }

    for (int i = 0; i < 8; i++)
    {
        mHash[i] += S[i];
    }

    mBlockIndex = 0;
}

}  // namespace Crypto
}  // namespace Thread
