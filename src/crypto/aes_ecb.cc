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
#include <crypto/aes_ecb.h>

namespace Thread {
namespace Crypto {

static uint32_t SetupMix(uint32_t temp)
{
    return
        (Te4_3[byte(temp, 2)]) ^
        (Te4_2[byte(temp, 1)]) ^
        (Te4_1[byte(temp, 0)]) ^
        (Te4_0[byte(temp, 3)]);
}

ThreadError AesEcb::SetKey(const uint8_t *key, uint16_t key_length)
{
    uint32_t *rk;

    assert(key_length == 16);

    /* setup the forward key */
    rk = m_eK;
    LOAD32H(rk[0], key +  0);
    LOAD32H(rk[1], key +  4);
    LOAD32H(rk[2], key +  8);
    LOAD32H(rk[3], key + 12);

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

void AesEcb::Encrypt(const uint8_t *pt, uint8_t *ct) const
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

    LOAD32H(s0, pt  +  0);
    s0 ^= rk[0];
    LOAD32H(s1, pt  +  4);
    s1 ^= rk[1];
    LOAD32H(s2, pt  +  8);
    s2 ^= rk[2];
    LOAD32H(s3, pt  + 12);
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
    STORE32H(s0, ct);
    s1 =
        (Te4_3[byte(t1, 3)]) ^
        (Te4_2[byte(t2, 2)]) ^
        (Te4_1[byte(t3, 1)]) ^
        (Te4_0[byte(t0, 0)]) ^
        rk[1];
    STORE32H(s1, ct + 4);
    s2 =
        (Te4_3[byte(t2, 3)]) ^
        (Te4_2[byte(t3, 2)]) ^
        (Te4_1[byte(t0, 1)]) ^
        (Te4_0[byte(t1, 0)]) ^
        rk[2];
    STORE32H(s2, ct + 8);
    s3 =
        (Te4_3[byte(t3, 3)]) ^
        (Te4_2[byte(t0, 2)]) ^
        (Te4_1[byte(t1, 1)]) ^
        (Te4_0[byte(t2, 0)]) ^
        rk[3];
    STORE32H(s3, ct + 12);
}

}  // namespace Crypto
}  // namespace Thread
