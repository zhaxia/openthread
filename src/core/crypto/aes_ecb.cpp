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
 *   This file implements AES-ECB.
 */

#include <common/code_utils.hpp>
#include <common/debug.hpp>
#include <common/thread_error.hpp>
#include <crypto/aes_ecb.hpp>

namespace Thread {
namespace Crypto {

static uint32_t SetupMix(uint32_t aTemp)
{
    return
        (Te4_3[byte(aTemp, 2)]) ^
        (Te4_2[byte(aTemp, 1)]) ^
        (Te4_1[byte(aTemp, 0)]) ^
        (Te4_0[byte(aTemp, 3)]);
}

ThreadError AesEcb::SetKey(const uint8_t *aKey, uint16_t aKeyLength)
{
    uint32_t *rk;

    assert(aKeyLength == 16);

    /* setup the forward key */
    rk = m_eK;
    LOAD32H(rk[0], aKey +  0);
    LOAD32H(rk[1], aKey +  4);
    LOAD32H(rk[2], aKey +  8);
    LOAD32H(rk[3], aKey + 12);

    for (int i = 0; i < 10; i++)
    {
        uint32_t temp  = rk[3];
        rk[4] = rk[0] ^ SetupMix(temp) ^ rcon[i];
        rk[5] = rk[1] ^ rk[4];
        rk[6] = rk[2] ^ rk[5];
        rk[7] = rk[3] ^ rk[6];
        rk += 4;
    }

    return kThreadError_None;
}

void AesEcb::Encrypt(const uint8_t *aPlainText, uint8_t *aCipherText) const
{
    uint32_t s0, s1, s2, s3, t0, t1, t2, t3;
    const uint32_t *rk;
    int Nr, r;

    Nr = 10;
    rk = m_eK;

    /*
     * map byte array block to cipher state
     * and add initial round key:
     */

    LOAD32H(s0, aPlainText  +  0);
    s0 ^= rk[0];
    LOAD32H(s1, aPlainText  +  4);
    s1 ^= rk[1];
    LOAD32H(s2, aPlainText  +  8);
    s2 ^= rk[2];
    LOAD32H(s3, aPlainText  + 12);
    s3 ^= rk[3];

    for (r = 0; ; r++)
    {
        rk += 4;
        t0 =
            Te0(byte(s0, 3)) ^
            Te1(byte(s1, 2)) ^
            Te2(byte(s2, 1)) ^
            Te3(byte(s3, 0)) ^
            rk[0];
        t1 =
            Te0(byte(s1, 3)) ^
            Te1(byte(s2, 2)) ^
            Te2(byte(s3, 1)) ^
            Te3(byte(s0, 0)) ^
            rk[1];
        t2 =
            Te0(byte(s2, 3)) ^
            Te1(byte(s3, 2)) ^
            Te2(byte(s0, 1)) ^
            Te3(byte(s1, 0)) ^
            rk[2];
        t3 =
            Te0(byte(s3, 3)) ^
            Te1(byte(s0, 2)) ^
            Te2(byte(s1, 1)) ^
            Te3(byte(s2, 0)) ^
            rk[3];

        if (r == Nr - 2)
        {
            break;
        }

        s0 = t0;
        s1 = t1;
        s2 = t2;
        s3 = t3;
    }

    rk += 4;

    /*
     * apply last round and
     * map cipher state to byte array block:
     */
    s0 =
        (Te4_3[byte(t0, 3)]) ^
        (Te4_2[byte(t1, 2)]) ^
        (Te4_1[byte(t2, 1)]) ^
        (Te4_0[byte(t3, 0)]) ^
        rk[0];
    STORE32H(s0, aCipherText);
    s1 =
        (Te4_3[byte(t1, 3)]) ^
        (Te4_2[byte(t2, 2)]) ^
        (Te4_1[byte(t3, 1)]) ^
        (Te4_0[byte(t0, 0)]) ^
        rk[1];
    STORE32H(s1, aCipherText + 4);
    s2 =
        (Te4_3[byte(t2, 3)]) ^
        (Te4_2[byte(t3, 2)]) ^
        (Te4_1[byte(t0, 1)]) ^
        (Te4_0[byte(t1, 0)]) ^
        rk[2];
    STORE32H(s2, aCipherText + 8);
    s3 =
        (Te4_3[byte(t3, 3)]) ^
        (Te4_2[byte(t0, 2)]) ^
        (Te4_1[byte(t1, 1)]) ^
        (Te4_0[byte(t2, 0)]) ^
        rk[3];
    STORE32H(s3, aCipherText + 12);
}

}  // namespace Crypto
}  // namespace Thread
