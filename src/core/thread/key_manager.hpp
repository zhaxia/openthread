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

#include <openthread-types.h>
#include <crypto/hmac_sha256.h>

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
    /**
     * This constructor initializes the object.
     *
     * @param[in]  aThreadNetif  A reference to the Thread network interface.
     *
     */
    explicit KeyManager(ThreadNetif &aThreadNetif);

    /**
     * This method returns a pointer to the Thread Master Key
     *
     * @param[out]  aKeyLength  A pointer where the key length value will be placed.
     *
     * @returns A pointer to the Thread Master Key.
     *
     */
    const uint8_t *GetMasterKey(uint8_t *aKeyLength) const;

    /**
     * This method sets the Thread Master Key.
     *
     * @param[in]  aKey        A pointer to the Thread Master Key.
     * @param[in]  aKeyLength  The length of @p aKey.
     *
     * @retval kThreadError_None         Successfully set the Thread Master Key.
     * @retval kThreadError_InvalidArgs  The @p aKeyLength value was invalid.
     *
     */
    ThreadError SetMasterKey(const void *aKey, uint8_t aKeyLength);

    /**
     * This method returns the current key sequence value.
     *
     * @returns The current key sequence value.
     *
     */
    uint32_t GetCurrentKeySequence() const;

    /**
     * This method sets the current key sequence value.
     *
     * @param[in]  aKeySequence  The key sequence value.
     *
     */
    void SetCurrentKeySequence(uint32_t aKeySequence);

    /**
     * This method returns a pointer to the current MAC key.
     *
     * @returns A pointer to the current MAC key.
     *
     */
    const uint8_t *GetCurrentMacKey() const;

    /**
     * This method returns a pointer to the current MLE key.
     *
     * @returns A pointer to the current MLE key.
     *
     */
    const uint8_t *GetCurrentMleKey() const;

    /**
     * This method indicates whether the previous key is valid.
     *
     * @retval TRUE   If the previous key is vaild.
     * @retval FALSE  If the previous key is not valid.
     *
     */
    bool IsPreviousKeyValid() const;

    /**
     * This method returns the previous key sequence value.
     *
     * @returns The previous key sequence value.
     *
     */
    uint32_t GetPreviousKeySequence() const;

    /**
     * This method returns a pointer to the previous MAC key.
     *
     * @returns A pointer to the previous MAC key.
     *
     */
    const uint8_t *GetPreviousMacKey() const;

    /**
     * This method returns a pointer to the previous MLE key.
     *
     * @returns A pointer to the previous MLE key.
     *
     */
    const uint8_t *GetPreviousMleKey() const;

    /**
     * This method returns a pointer to a temporary MAC key computed from the given key sequence.
     *
     * @param[in]  aKeySequence  The key sequence value.
     *
     * @returns A pointer to the temporary MAC key.
     *
     */
    const uint8_t *GetTemporaryMacKey(uint32_t aKeySequence);

    /**
     * This method returns a pointer to a temporary MLE key computed from the given key sequence.
     *
     * @param[in]  aKeySequence  The key sequence value.
     *
     * @returns A pointer to the temporary MLE key.
     *
     */
    const uint8_t *GetTemporaryMleKey(uint32_t aKeySequence);

    /**
     * This method returns the current MAC Frame Counter value.
     *
     * @returns The current MAC Frame Counter value.
     *
     */
    uint32_t GetMacFrameCounter() const;

    /**
     * This method increments the current MAC Frame Counter value.
     *
     * @returns The current MAC Frame Counter value.
     *
     */
    void IncrementMacFrameCounter();

    /**
     * This method returns the current MLE Frame Counter value.
     *
     * @returns The current MLE Frame Counter value.
     *
     */
    uint32_t GetMleFrameCounter() const;

    /**
     * This method increments the current MLE Frame Counter value.
     *
     * @returns The current MLE Frame Counter value.
     *
     */
    void IncrementMleFrameCounter();

private:
    enum
    {
        kMaxKeyLength = 16,
    };

    ThreadError ComputeKey(uint32_t aKeySequence, uint8_t *aKey);
    void UpdateNeighbors();

    uint8_t mMasterKey[kMaxKeyLength];
    uint8_t mMasterKeyLength;

    uint32_t mPreviousKeySequence;
    uint8_t mPreviousKey[otCryptoSha256Size];
    bool mPreviousKeyValid;

    uint32_t mCurrentKeySequence;
    uint8_t mCurrentKey[otCryptoSha256Size];

    uint8_t mTemporaryKey[otCryptoSha256Size];

    uint32_t mMacFrameCounter;
    uint32_t mMleFrameCounter;

    ThreadNetif &mNetif;
};

/**
 * @}
 */

}  // namespace Thread

#endif  // KEY_MANAGER_HPP_
