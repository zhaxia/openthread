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

#include "test_util.h"
#include <common/debug.hpp>
#include <string.h>

#include <crypto/hmac_sha256.h>

void TestHmacSha256()
{
    static const struct
    {
        const char *key;
        const char *data;
        uint8_t hash[otCryptoSha256Size];
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

    uint8_t hash[otCryptoSha256Size];

    for (int i = 0; tests[i].key != NULL; i++)
    {
        otCryptoHmacSha256Start(tests[i].key, strlen(tests[i].key));
        otCryptoHmacSha256Update(tests[i].data, strlen(tests[i].data));
        otCryptoHmacSha256Finish(hash);

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
