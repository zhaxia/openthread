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
 *   This file includes definitions for computing hashes.
 */

#ifndef HASH_HPP_
#define HASH_HPP_

#include <stdint.h>

#include <openthread-types.h>

namespace Thread {
namespace Crypto {

/**
 * @addtogroup core-security
 *
 * @{
 *
 */

/**
 * This class implements hash computations.
 *
 */
class Hash
{
public:
    /**
     * This method returns the hash size.
     *
     * @returns The hash size.
     *
     */
    virtual uint16_t GetSize(void) const = 0;

    /**
     * This method initializes the hash computation.
     *
     */
    virtual void Init(void) = 0;

    /**
     * This method inputs data into the hash.
     *
     * @param[in]  aBuf        A pointer to the input buffer.
     * @param[in]  aBufLength  The length of @p aBuf in bytes.
     *
     */
    virtual void Input(const void *aBuf, uint16_t aBufLength) = 0;

    /**
     * This method finalizes the hash computation.
     *
     * @param[out]  aHash  A pointer to the output buffer.
     *
     */
    virtual void Finalize(uint8_t *aHash) = 0;
};

/**
 * @}
 *
 */

}  // namespace Crypto
}  // namespace Thread

#endif  // HASH_HPP_
