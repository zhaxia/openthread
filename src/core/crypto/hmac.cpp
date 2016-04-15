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

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>
#include <crypto/hmac.hpp>

namespace Thread {
namespace Crypto {

Hmac::Hmac(Hash &hash)
{
    mHash = &hash;
}

ThreadError Hmac::SetKey(const void *key, uint16_t keyLength)
{
    if (keyLength > kMaxKeyLength)
    {
        mHash->Init();
        mHash->Input(key, keyLength);
        mHash->Finalize(mKey);
        mKeyLength = mHash->GetSize();
    }
    else
    {
        memcpy(mKey, key, keyLength);
        mKeyLength = keyLength;
    }

    return kThreadError_None;
}

ThreadError Hmac::Init()
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

    return kThreadError_None;
}

ThreadError Hmac::Input(const void *buf, uint16_t bufLength)
{
    return mHash->Input(buf, bufLength);
}

ThreadError Hmac::Finalize(uint8_t *hash)
{
    uint8_t pad[kMaxKeyLength];
    int i;

    // finish inner hash
    mHash->Finalize(hash);

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
    mHash->Input(hash, mHash->GetSize());
    mHash->Finalize(hash);

    return kThreadError_None;
}

}  // namespace Crypto
}  // namespace Thread
