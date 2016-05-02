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
#include <crypto/sha256.hpp>
#include <string.h>

void TestSha256()
{
    static const struct
    {
        const char *msg;
        uint8_t hash[32];
    } tests[] =
    {
        {
            "abc",
            {
                0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
            }
        },
        {
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            {
                0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
                0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
                0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
                0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
            }
        },
        { NULL, {} },
    };

    Thread::Crypto::Sha256 sha256;
    uint8_t hash[64];

    for (int i = 0; tests[i].msg != NULL; i++)
    {
        sha256.Init();
        sha256.Input(tests[i].msg, strlen(tests[i].msg));
        sha256.Finalize(hash);

        VerifyOrQuit(memcmp(hash, tests[i].hash, sizeof(tests[i].hash)) == 0,
                     "SHA-256 test failed\n");
    }
}

int main()
{
    TestSha256();
    printf("All tests passed\n");
    return 0;
}
