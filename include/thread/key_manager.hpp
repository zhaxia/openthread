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
 *   This file includes definitions for Thread security material generation.
 */

#ifndef KEY_MANAGER_HPP_
#define KEY_MANAGER_HPP_

#include <stdint.h>

#include <common/thread_error.hpp>

namespace Thread {

class ThreadNetif;

/**
 * @addtogroup core-security
 *
 * @brief
 *   This module includes definitions for Thread security material generation.
 *
 * @{
 */

class KeyManager
{
public:
    explicit KeyManager(ThreadNetif &netif);
    const uint8_t *GetMasterKey(uint8_t *keyLength) const;
    ThreadError SetMasterKey(const void *key, uint8_t keyLength);

    uint32_t GetCurrentKeySequence() const;
    ThreadError SetCurrentKeySequence(uint32_t keySequence);
    const uint8_t *GetCurrentMacKey() const;
    const uint8_t *GetCurrentMleKey() const;

    bool IsPreviousKeyValid() const;
    uint32_t GetPreviousKeySequence() const;
    const uint8_t *GetPreviousMacKey() const;
    const uint8_t *GetPreviousMleKey() const;

    const uint8_t *GetTemporaryMacKey(uint32_t keySequence);
    const uint8_t *GetTemporaryMleKey(uint32_t keySequence);

    uint32_t GetMacFrameCounter() const;
    uint32_t GetMleFrameCounter() const;

    ThreadError IncrementMacFrameCounter();
    ThreadError IncrementMleFrameCounter();

private:
    ThreadError ComputeKey(uint32_t keySequence, uint8_t *key);
    void UpdateNeighbors();

    uint8_t mMasterKey[16];
    uint8_t mMasterKeyLength;

    uint32_t mPreviousKeySequence;
    uint8_t mPreviousKey[32];
    bool mPreviousKeyValid = false;

    uint32_t mCurrentKeySequence;
    uint8_t mCurrentKey[32];

    uint8_t mTemporaryKey[32];

    uint32_t mMacFrameCounter = 0;
    uint32_t mMleFrameCounter = 0;

    ThreadNetif *mNetif;
};

/**
 * @}
 */

}  // namespace Thread

#endif  // KEY_MANAGER_HPP_
