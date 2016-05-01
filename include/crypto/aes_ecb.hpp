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
 *   This file includes definitions for performing AES-ECB computations.
 */

#ifndef AES_ECB_HPP_
#define AES_ECB_HPP_

#include <stdint.h>

#include <common/thread_error.hpp>
#include <crypto/aes.hpp>

namespace Thread {
namespace Crypto {

/**
 * @addtogroup core-security
 *
 * @{
 *
 */

/**
 * This class implements AES ECB computation.
 *
 */
class AesEcb
{
public:
    /**
     * This method sets the key.
     *
     * @param[in]  aKey        A pointer to the key.
     * @param[in]  aKeyLength  Length of the key in bytes.
     *
     */
    ThreadError SetKey(const uint8_t *aKey, uint16_t aKeyLength);

    /**
     * This method encrypts data.
     *
     * @param[in]   aPlainText  A pointer to the plaintext.
     * @param[out]  aCipherText  A pointer to the ciphertext.
     *
     */
    void Encrypt(const uint8_t *aPlainText, uint8_t *aCipherText) const;

private:
    uint32_t m_eK[44];
};

/**
 * @}
 *
 */

}  // namespace Crypto
}  // namespace Thread

#endif  // AES_ECB_HPP_
