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
 *   This file includes definitions for computing HMACs.
 */

#ifndef HMAC_HPP_
#define HMAC_HPP_

#include <stdint.h>

#include <openthread-types.h>
#include <crypto/hash.hpp>

namespace Thread {
namespace Crypto {

/**
 * @addtogroup core-security
 *
 * @{
 *
 */

/**
 * This class implements HMAC computation.
 *
 */
class Hmac
{
public:
    explicit Hmac(Hash &aHash);

    /**
     * This method sets the key.
     *
     * @param[in]  aKey        A pointer to the key.
     * @param[in]  aKeyLength  The key length in bytes.
     *
     */
    void SetKey(const void *aKey, uint16_t aKeyLength);

    /**
     * This method initializes the HMAC computation.
     *
     */
    void Init(void);

    /**
     * This method inputs bytes into the HMAC computation.
     *
     * @param[in]  aBuf        A pointer to the input buffer.
     * @param[in]  aBufLength  The length of @p aBuf in bytes.
     *
     */
    void Input(const void *aBuf, uint16_t aBufLength);

    /**
     * This method finalizes the has computation.
     *
     * @parma[out]  aHash  A pointer to the output buffer.
     *
     */
    void Finalize(uint8_t *aHash);

private:
    enum
    {
        kMaxKeyLength = 64,
    };
    uint8_t mKey[kMaxKeyLength];
    uint8_t mKeyLength = 0;
    Hash *mHash;
};

/**
 * @}
 *
 */

}  // namespace Crypto
}  // namespace Thread

#endif  // CRYPTO_HMAC_HPP_
