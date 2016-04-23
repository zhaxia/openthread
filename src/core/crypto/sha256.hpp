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
 *   This file contains definitions for computing SHA-256 hashes.
 */

#ifndef SHA256_HPP_
#define SHA256_HPP_

#include <stdint.h>

#include <common/thread_error.hpp>
#include <crypto/hash.hpp>

namespace Thread {
namespace Crypto {

class Sha256: public Hash
{
public:
    enum
    {
        kHashSize = 32,
    };

    uint16_t GetSize() const final;
    ThreadError Init() final;
    ThreadError Input(const void *buf, uint16_t bufLength) final;
    ThreadError Finalize(uint8_t *hash) final;

private:
    void PadMessage();
    void ProcessBlock();

    uint32_t mHash[kHashSize / sizeof(uint32_t)];
    uint32_t mLengthLo;
    uint32_t mLengthHi;
    uint8_t mBlockIndex;
    uint8_t mBlock[64];
};

}  // namespace Crypto
}  // namespace Thread

#endif  // CRYPTO_SHA256_HPP_
