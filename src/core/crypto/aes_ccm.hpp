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
 *   This file includes definitions for performing AES-CCM computations.
 */

#ifndef AES_CCM_HPP_
#define AES_CCM_HPP_

#include <stdint.h>

#include <openthread-types.h>
#include <crypto/aes_ecb.hpp>

namespace Thread {
namespace Crypto {

/**
 * @addtogroup core-security
 *
 * @{
 *
 */

/**
 * This class implements AES CCM computation.
 *
 */
class AesCcm: public AesEcb
{
public:
    /**
     * This method initializes the AES CCM computation.
     *
     * @param[in]  aHeaderLength     Length of header in bytes.
     * @param[in]  aPlainTextLength  Length of plaintext in bytes.
     * @param[in]  aTagLength        Length of tag in bytes.
     * @param[in]  aNonce            A pointer to the nonce.
     * @param[in]  aNonceLength      Length of nonce in bytes.
     *
     */
    void Init(uint32_t aHeaderLength, uint32_t aPlainTextLength, uint8_t aTagLength,
              const void *aNonce, uint8_t aNonceLength);

    /**
     * This method processes the header.
     *
     * @param[in]  aHeader        A pointer to the header.
     * @param[in]  aHeaderLength  Lenth of header in bytes.
     *
     */
    void Header(const void *aHeader, uint32_t aHeaderLength);

    /**
     * This method processes the payload.
     *
     * @param[inout]  aPlainText   A pointer to the plaintext.
     * @param[inout]  aCipherText  A pointer to the ciphertext.
     * @param[in]     aLength      Payload length in bytes.
     * @param[in]     aEncrypt     TRUE on encrypt and FALSE on decrypt.
     *
     */
    void Payload(void *aPlainText, void *aCipherText, uint32_t aLength, bool aEncrypt);

    /**
     * This method generates the tag.
     *
     * @param[out]  aTag        A pointer to the tag.
     * @param[out]  aTagLength  Length of the tag in bytes.
     *
     */
    void Finalize(void *aTag, uint8_t *aTagLength);

private:
    uint8_t mBlock[kAesBlockSize];
    uint8_t mCtr[kAesBlockSize];
    uint8_t mCtrPad[kAesBlockSize];
    uint8_t mNonceLength;
    uint32_t mHeaderLength;
    uint32_t mHeaderCur;
    uint32_t mPlainTextLength;
    uint32_t mPlainTextCur;
    uint16_t mBlockLength;
    uint16_t mCtrLength;
    uint8_t mTagLength;
};

/**
 * @}
 *
 */

}  // namespace Crypto
}  // namespace Thread

#endif  // AES_CCM_HPP_
