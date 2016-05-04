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
 *   This file implements HMAC.
 */

#include <common/code_utils.hpp>
#include <common/debug.hpp>
#include <crypto/hmac.hpp>

namespace Thread {
namespace Crypto {

Hmac::Hmac(Hash &hash)
{
    mHash = &hash;
}

void Hmac::SetKey(const void *aKey, uint16_t aKeyLength)
{
    if (aKeyLength > kMaxKeyLength)
    {
        mHash->Init();
        mHash->Input(aKey, aKeyLength);
        mHash->Finalize(mKey);
        mKeyLength = mHash->GetSize();
    }
    else
    {
        memcpy(mKey, aKey, aKeyLength);
        mKeyLength = aKeyLength;
    }
}

void Hmac::Init(void)
{
    uint8_t pad[kMaxKeyLength];
    int i;

    for (i = 0; i < mKeyLength; i++)
    {
        pad[i] = mKey[i] ^ 0x36;
    }

    for (; i < 64; i++)
    {
        pad[i] = 0x36;
    }

    // start inner hash
    mHash->Init();
    mHash->Input(pad, sizeof(pad));
}

void Hmac::Input(const void *aBuf, uint16_t aBufLength)
{
    mHash->Input(aBuf, aBufLength);
}

void Hmac::Finalize(uint8_t *aHash)
{
    uint8_t pad[kMaxKeyLength];
    int i;

    // finish inner hash
    mHash->Finalize(aHash);

    // perform outer hash
    for (i = 0; i < mKeyLength; i++)
    {
        pad[i] = mKey[i] ^ 0x5c;
    }

    for (; i < 64; i++)
    {
        pad[i] = 0x5c;
    }

    mHash->Init();
    mHash->Input(pad, kMaxKeyLength);
    mHash->Input(aHash, mHash->GetSize());
    mHash->Finalize(aHash);
}

}  // namespace Crypto
}  // namespace Thread
