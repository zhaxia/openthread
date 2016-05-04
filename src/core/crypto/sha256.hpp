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
 *   This file includes definitions for computing SHA-256 hashes.
 */

#ifndef SHA256_HPP_
#define SHA256_HPP_

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
 * This class implements SHA-256.
 *
 */
class Sha256: public Hash
{
public:
    enum
    {
        kHashSize = 32,  ///< The hash size in bytes.
    };

    /**
     * This method returns the hash size.
     *
     * @returns The hash size.
     *
     */
    uint16_t GetSize(void) const final { return kHashSize; };

    /**
     * This method initializes the hash computation.
     *
     */
    void Init(void) final;

    /**
     * This method inputs data into the hash.
     *
     * @param[in]  aBuf        A pointer to the input buffer.
     * @param[in]  aBufLength  The length of @p aBuf in bytes.
     *
     */
    void Input(const void *aBuf, uint16_t aBufLength) final;

    /**
     * This method finalizes the hash computation.
     *
     * @param[out]  aHash  A pointer to the output buffer.
     *
     */
    void Finalize(uint8_t *aHash) final;

private:
    enum
    {
        kHashBlockSize = 64,  ///< The block size.
    };

    void PadMessage(void);
    void ProcessBlock(void);

    static const uint32_t K[];

    uint32_t mHash[kHashSize / sizeof(uint32_t)];
    uint32_t mLengthLo;
    uint32_t mLengthHi;
    uint8_t mBlockIndex;
    uint8_t mBlock[kHashBlockSize];
};

/**
 * @}
 *
 */

}  // namespace Crypto
}  // namespace Thread

#endif  // CRYPTO_SHA256_HPP_
