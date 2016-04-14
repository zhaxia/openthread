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

#ifndef AES_CCM_HPP_
#define AES_CCM_HPP_

#include <stdint.h>

#include <common/thread_error.hpp>
#include <crypto/aes_ecb.hpp>

namespace Thread {
namespace Crypto {

class AesCcm
{
public:
    void Init(const AesEcb &ecb, uint32_t header_length, uint32_t plaintext_length, uint8_t tag_length,
              const void *nonce, uint8_t nonce_length);
    void Header(const void *header, uint32_t header_length);
    void Payload(void *plaintext, void *ciphertext, uint32_t length, bool encrypt);
    void Finalize(void *tag, uint8_t *tag_length);

private:
    const AesEcb *m_ecb;
    uint8_t m_block[16];
    uint8_t m_ctr[16];
    uint8_t m_ctrpad[16];
    uint8_t m_nonce_length;
    uint32_t m_header_length;
    uint32_t m_header_cur;
    uint32_t m_plaintext_length;
    uint32_t m_plaintext_cur;
    uint16_t m_block_length;
    uint16_t m_ctr_length;
    uint8_t m_tag_length;
};

}  // namespace Crypto
}  // namespace Thread

#endif  // AES_CCM_HPP_
