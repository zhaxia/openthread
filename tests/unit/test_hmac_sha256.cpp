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

#include "test_util.h"
#include <common/debug.hpp>
#include <crypto/hmac.hpp>
#include <crypto/sha256.hpp>
#include <string.h>

void TestHmacSha256()
{
    static const struct
    {
        const char *key;
        const char *data;
        uint8_t hash[64];
    } tests[] =
    {
        {
            "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
            "Hi There",
            {
                0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
                0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
                0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
                0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7,
            },
        },
        {
            NULL,
            NULL,
            {},
        },
    };

    Thread::Crypto::Sha256 sha256;
    Thread::Crypto::Hmac hmac(sha256);
    uint8_t hash[64];

    for (int i = 0; tests[i].key != NULL; i++)
    {
        hmac.SetKey(tests[i].key, strlen(tests[i].key));
        hmac.Init();
        hmac.Input(tests[i].data, strlen(tests[i].data));
        hmac.Finalize(hash);

        VerifyOrQuit(memcmp(hash, tests[i].hash, sizeof(tests[i].hash)) == 0,
                     "HMAC-SHA-256 failed\n");
    }
}

int main()
{
    TestHmacSha256();
    printf("All tests passed\n");
    return 0;
}
