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
 *   This file contains definitions for performing AES-CCM computations.
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
    void Init(const AesEcb &ecb, uint32_t headerLength, uint32_t plaintextLength, uint8_t tagLength,
              const void *nonce, uint8_t nonceLength);
    void Header(const void *header, uint32_t headerLength);
    void Payload(void *plaintext, void *ciphertext, uint32_t length, bool encrypt);
    void Finalize(void *tag, uint8_t *tagLength);

private:
    const AesEcb *mEcb;
    uint8_t mBlock[16];
    uint8_t mCtr[16];
    uint8_t mCtrPad[16];
    uint8_t mNonceLength;
    uint32_t mHeaderLength;
    uint32_t mHeaderCur;
    uint32_t mPlaintextLength;
    uint32_t mPlaintextCur;
    uint16_t mBlockLength;
    uint16_t mCtrLength;
    uint8_t mTagLength;
};

}  // namespace Crypto
}  // namespace Thread

#endif  // AES_CCM_HPP_
