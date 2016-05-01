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
#include <crypto/aes_ccm.hpp>
#include <string.h>

/**
 * Verifies test vectors from IEEE 802.15.4-2006 Annex C Section C.2.1
 */
void TestMacBeaconFrame()
{
    uint8_t key[] =
    {
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    };

    uint8_t test[] =
    {
        0x08, 0xD0, 0x84, 0x21, 0x43, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x02, 0x05, 0x00,
        0x00, 0x00, 0x55, 0xCF, 0x00, 0x00, 0x51, 0x52,
        0x53, 0x54, 0x22, 0x3B, 0xC1, 0xEC, 0x84, 0x1A,
        0xB5, 0x53
    };

    uint8_t encrypted[] =
    {
        0x08, 0xD0, 0x84, 0x21, 0x43, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x02, 0x05, 0x00,
        0x00, 0x00, 0x55, 0xCF, 0x00, 0x00, 0x51, 0x52,
        0x53, 0x54, 0x22, 0x3B, 0xC1, 0xEC, 0x84, 0x1A,
        0xB5, 0x53
    };

    uint8_t decrypted[] =
    {
        0x08, 0xD0, 0x84, 0x21, 0x43, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x02, 0x05, 0x00,
        0x00, 0x00, 0x55, 0xCF, 0x00, 0x00, 0x51, 0x52,
        0x53, 0x54, 0x22, 0x3B, 0xC1, 0xEC, 0x84, 0x1A,
        0xB5, 0x53
    };

    Thread::Crypto::AesCcm aesCcm;
    uint32_t headerLength = sizeof(test) - 8;
    uint32_t payloadLength = 0;
    uint8_t tagLength = 8;

    uint8_t nonce[] =
    {
        0xAC, 0xDE, 0x48, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x05, 0x02,
    };

    aesCcm.SetKey(key, sizeof(key));
    aesCcm.Init(headerLength, payloadLength, tagLength, nonce, sizeof(nonce));
    aesCcm.Header(test, headerLength);
    aesCcm.Finalize(test + headerLength, &tagLength);

    VerifyOrQuit(memcmp(test, encrypted, sizeof(encrypted)) == 0,
                 "TestMacBeaconFrame encrypt failed");

    aesCcm.Init(headerLength, payloadLength, tagLength, nonce, sizeof(nonce));
    aesCcm.Header(test, headerLength);
    aesCcm.Finalize(test + headerLength, &tagLength);

    VerifyOrQuit(memcmp(test, decrypted, sizeof(decrypted)) == 0,
                 "TestMacBeaconFrame decrypt failed");
}

/**
 * Verifies test vectors from IEEE 802.15.4-2006 Annex C Section C.2.1
 */
void TestMacDataFrame()
{
    uint8_t key[] =
    {
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    };

    uint8_t test[] =
    {
        0x69, 0xDC, 0x84, 0x21, 0x43, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x04, 0x05, 0x00,
        0x00, 0x00, 0x61, 0x62, 0x63, 0x64
    };

    uint8_t encrypted[] =
    {
        0x69, 0xDC, 0x84, 0x21, 0x43, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x04, 0x05, 0x00,
        0x00, 0x00, 0xD4, 0x3E, 0x02, 0x2B
    };

    uint8_t decrypted[] =
    {
        0x69, 0xDC, 0x84, 0x21, 0x43, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0x04, 0x05, 0x00,
        0x00, 0x00, 0x61, 0x62, 0x63, 0x64
    };

    Thread::Crypto::AesCcm aesCcm;
    uint32_t headerLength = sizeof(test) - 4;
    uint32_t payloadLength = 4;
    uint8_t tagLength = 0;

    uint8_t nonce[] =
    {
        0xAC, 0xDE, 0x48, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x05, 0x04,
    };

    aesCcm.SetKey(key, sizeof(key));
    aesCcm.Init(headerLength, payloadLength, tagLength, nonce, sizeof(nonce));
    aesCcm.Header(test, headerLength);
    aesCcm.Payload(test + headerLength, test + headerLength, payloadLength, true);
    aesCcm.Finalize(test + headerLength + payloadLength, &tagLength);

    dump("test", test, sizeof(test));
    dump("encrypted", encrypted, sizeof(encrypted));

    VerifyOrQuit(memcmp(test, encrypted, sizeof(encrypted)) == 0,
                 "TestMacDataFrame encrypt failed");

    aesCcm.Init(headerLength, payloadLength, tagLength, nonce, sizeof(nonce));
    aesCcm.Header(test, headerLength);
    aesCcm.Payload(test + headerLength, test + headerLength, payloadLength, false);
    aesCcm.Finalize(test + headerLength + payloadLength, &tagLength);

    VerifyOrQuit(memcmp(test, decrypted, sizeof(decrypted)) == 0,
                 "TestMacDataFrame decrypt failed");
}

/**
 * Verifies test vectors from IEEE 802.15.4-2006 Annex C Section C.2.3
 */
void TestMacCommandFrame()
{
    uint8_t key[] =
    {
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    };

    uint8_t test[] =
    {
        0x2B, 0xDC, 0x84, 0x21, 0x43, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0xFF, 0xFF, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x48, 0xDE, 0xAC, 0x06,
        0x05, 0x00, 0x00, 0x00, 0x01, 0xCE, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    uint32_t headerLength = 29, payloadLength = 1;
    uint8_t tagLength = 8;

    uint8_t encrypted[] =
    {
        0x2B, 0xDC, 0x84, 0x21, 0x43, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0xFF, 0xFF, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x48, 0xDE, 0xAC, 0x06,
        0x05, 0x00, 0x00, 0x00, 0x01, 0xD8, 0x4F, 0xDE,
        0x52, 0x90, 0x61, 0xF9, 0xC6, 0xF1,
    };

    uint8_t decrypted[] =
    {
        0x2B, 0xDC, 0x84, 0x21, 0x43, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x48, 0xDE, 0xAC, 0xFF, 0xFF, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x48, 0xDE, 0xAC, 0x06,
        0x05, 0x00, 0x00, 0x00, 0x01, 0xCE, 0x4F, 0xDE,
        0x52, 0x90, 0x61, 0xF9, 0xC6, 0xF1,
    };

    uint8_t nonce[] =
    {
        0xAC, 0xDE, 0x48, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x05, 0x06,
    };

    Thread::Crypto::AesCcm aesCcm;
    aesCcm.SetKey(key, sizeof(key));
    aesCcm.Init(headerLength, payloadLength, tagLength, nonce, sizeof(nonce));
    aesCcm.Header(test, headerLength);
    aesCcm.Payload(test + headerLength, test + headerLength, payloadLength, true);
    aesCcm.Finalize(test + headerLength + payloadLength, &tagLength);
    VerifyOrQuit(memcmp(test, encrypted, sizeof(encrypted)) == 0,
                 "TestMacCommandFrame encrypt failed\n");

    aesCcm.Init(headerLength, payloadLength, tagLength, nonce, sizeof(nonce));
    aesCcm.Header(test, headerLength);
    aesCcm.Payload(test + headerLength, test + headerLength, payloadLength, false);
    aesCcm.Finalize(test + headerLength + payloadLength, &tagLength);

    VerifyOrQuit(memcmp(test, decrypted, sizeof(decrypted)) == 0,
                 "TestMacCommandFrame decrypt failed\n");
}

int main()
{
    TestMacBeaconFrame();
    TestMacDataFrame();
    TestMacCommandFrame();
    printf("All tests passed\n");
    return 0;
}
