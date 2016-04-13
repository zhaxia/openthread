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

#ifndef SHA256_H_
#define SHA256_H_

#include <stdint.h>
#include <common/thread_error.h>
#include <crypto/hash.h>

namespace Thread {
namespace Crypto {

class Sha256: public Hash
{
public:
    uint16_t GetSize() const final;
    ThreadError Init() final;
    ThreadError Input(const void *buf, uint16_t buf_length) final;
    ThreadError Finalize(uint8_t *hash) final;

private:
    void PadMessage();
    void ProcessBlock();

    enum
    {
        kHashSize = 32,
    };
    uint32_t m_hash[kHashSize / sizeof(uint32_t)];
    uint32_t m_length_lo;
    uint32_t m_length_hi;
    uint8_t m_block_index;
    uint8_t m_block[64];
};

}  // namespace Crypto
}  // namespace Thread

#endif  // CRYPTO_SHA256_H_
