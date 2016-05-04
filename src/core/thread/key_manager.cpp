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
 *   This file implements Thread security material generation.
 */

#include <common/code_utils.hpp>
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

KeyManager::KeyManager(ThreadNetif &aThreadNetif):
    mNetif(aThreadNetif)
{
}

const uint8_t *KeyManager::GetMasterKey(uint8_t *aKeyLength) const
{
    if (aKeyLength)
    {
        *aKeyLength = mMasterKeyLength;
    }

    return mMasterKey;
}

ThreadError KeyManager::SetMasterKey(const void *aKey, uint8_t aKeyLength)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(aKeyLength <= sizeof(mMasterKey), error = kThreadError_InvalidArgs);
    memcpy(mMasterKey, aKey, aKeyLength);
    mMasterKeyLength = aKeyLength;
    mCurrentKeySequence = 0;
    ComputeKey(mCurrentKeySequence, mCurrentKey);

exit:
    return error;
}

ThreadError KeyManager::ComputeKey(uint32_t aKeySequence, uint8_t *aKey)
{
    Crypto::Sha256 sha256;
    Crypto::Hmac hmac(sha256);
    uint8_t keySequenceBytes[4];

    hmac.SetKey(mMasterKey, mMasterKeyLength);
    hmac.Init();

    keySequenceBytes[0] = aKeySequence >> 24;
    keySequenceBytes[1] = aKeySequence >> 16;
    keySequenceBytes[2] = aKeySequence >> 8;
    keySequenceBytes[3] = aKeySequence >> 0;
    hmac.Input(keySequenceBytes, sizeof(keySequenceBytes));
    hmac.Input(kThreadString, sizeof(kThreadString));

    hmac.Finalize(aKey);

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

    routers = mNetif.GetMle().GetParent();
    routers->mPreviousKey = true;

    routers = mNetif.GetMle().GetRouters(&numNeighbors);

    for (int i = 0; i < numNeighbors; i++)
    {
        routers[i].mPreviousKey = true;
    }

    children = mNetif.GetMle().GetChildren(&numNeighbors);

    for (int i = 0; i < numNeighbors; i++)
    {
        children[i].mPreviousKey = true;
    }
}

void KeyManager::SetCurrentKeySequence(uint32_t aKeySequence)
{
    mPreviousKeyValid = true;
    mPreviousKeySequence = mCurrentKeySequence;
    memcpy(mPreviousKey, mCurrentKey, sizeof(mPreviousKey));

    mCurrentKeySequence = aKeySequence;
    ComputeKey(mCurrentKeySequence, mCurrentKey);

    mMacFrameCounter = 0;
    mMleFrameCounter = 0;

    UpdateNeighbors();
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

const uint8_t *KeyManager::GetTemporaryMacKey(uint32_t aKeySequence)
{
    ComputeKey(aKeySequence, mTemporaryKey);
    return mTemporaryKey + 16;
}

const uint8_t *KeyManager::GetTemporaryMleKey(uint32_t aKeySequence)
{
    ComputeKey(aKeySequence, mTemporaryKey);
    return mTemporaryKey;
}

uint32_t KeyManager::GetMacFrameCounter() const
{
    return mMacFrameCounter;
}

void KeyManager::IncrementMacFrameCounter()
{
    mMacFrameCounter++;
}

uint32_t KeyManager::GetMleFrameCounter() const
{
    return mMleFrameCounter;
}

void KeyManager::IncrementMleFrameCounter()
{
    mMleFrameCounter++;
}

}  // namespace Thread
