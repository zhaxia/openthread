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
#include <crypto/sha256.hpp>
#include <thread/key_manager.hpp>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>

namespace Thread {

static const uint8_t kThreadString[] =
{
    'T', 'h', 'r', 'e', 'a', 'd',
};

KeyManager::KeyManager(ThreadNetif &netif)
{
    mNetif = &netif;
}

const uint8_t *KeyManager::GetMasterKey(uint8_t *keyLength) const
{
    if (keyLength)
    {
        *keyLength = mMasterKeyLength;
    }

    return mMasterKey;
}

ThreadError KeyManager::SetMasterKey(const void *key, uint8_t keyLength)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(keyLength <= sizeof(mMasterKey), error = kThreadError_InvalidArgs);
    memcpy(mMasterKey, key, keyLength);
    mMasterKeyLength = keyLength;
    mCurrentKeySequence = 0;
    ComputeKey(mCurrentKeySequence, mCurrentKey);

exit:
    return error;
}

ThreadError KeyManager::ComputeKey(uint32_t keySequence, uint8_t *key)
{
    Crypto::Sha256 sha256;
    Crypto::Hmac hmac(sha256);
    uint8_t keySequence_bytes[4];

    hmac.SetKey(mMasterKey, mMasterKeyLength);
    hmac.Init();

    keySequence_bytes[0] = keySequence >> 24;
    keySequence_bytes[1] = keySequence >> 16;
    keySequence_bytes[2] = keySequence >> 8;
    keySequence_bytes[3] = keySequence >> 0;
    hmac.Input(keySequence_bytes, sizeof(keySequence_bytes));
    hmac.Input(kThreadString, sizeof(kThreadString));

    hmac.Finalize(key);

    return kThreadError_None;
}

uint32_t KeyManager::GetCurrentKeySequence() const
{
    return mCurrentKeySequence;
}

void KeyManager::UpdateNeighbors()
{
    uint8_t numNeighbors;
    Router *routers;
    Child *children;

    routers = mNetif->GetMle()->GetParent();
    routers->mPreviousKey = true;

    routers = mNetif->GetMle()->GetRouters(&numNeighbors);

    for (int i = 0; i < numNeighbors; i++)
    {
        routers[i].mPreviousKey = true;
    }

    children = mNetif->GetMle()->GetChildren(&numNeighbors);

    for (int i = 0; i < numNeighbors; i++)
    {
        children[i].mPreviousKey = true;
    }
}

ThreadError KeyManager::SetCurrentKeySequence(uint32_t keySequence)
{
    ThreadError error = kThreadError_None;

    mPreviousKeyValid = true;
    mPreviousKeySequence = mCurrentKeySequence;
    memcpy(mPreviousKey, mCurrentKey, sizeof(mPreviousKey));

    mCurrentKeySequence = keySequence;
    ComputeKey(mCurrentKeySequence, mCurrentKey);

    mMacFrameCounter = 0;
    mMleFrameCounter = 0;

    UpdateNeighbors();

    return error;
}

const uint8_t *KeyManager::GetCurrentMacKey() const
{
    return mCurrentKey + 16;
}

const uint8_t *KeyManager::GetCurrentMleKey() const
{
    return mCurrentKey;
}

bool KeyManager::IsPreviousKeyValid() const
{
    return mPreviousKeyValid;
}

uint32_t KeyManager::GetPreviousKeySequence() const
{
    return mPreviousKeySequence;
}

const uint8_t *KeyManager::GetPreviousMacKey() const
{
    return mPreviousKey + 16;
}

const uint8_t *KeyManager::GetPreviousMleKey() const
{
    return mPreviousKey;
}

const uint8_t *KeyManager::GetTemporaryMacKey(uint32_t keySequence)
{
    ComputeKey(keySequence, mTemporaryKey);
    return mTemporaryKey + 16;
}

const uint8_t *KeyManager::GetTemporaryMleKey(uint32_t keySequence)
{
    ComputeKey(keySequence, mTemporaryKey);
    return mTemporaryKey;
}

uint32_t KeyManager::GetMacFrameCounter() const
{
    return mMacFrameCounter;
}

uint32_t KeyManager::GetMleFrameCounter() const
{
    return mMleFrameCounter;
}

ThreadError KeyManager::IncrementMacFrameCounter()
{
    mMacFrameCounter++;
    return kThreadError_None;
}

ThreadError KeyManager::IncrementMleFrameCounter()
{
    mMleFrameCounter++;
    return kThreadError_None;
}

}  // namespace Thread
