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

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>
#include <crypto/aes_ccm.hpp>

namespace Thread {
namespace Crypto {

void AesCcm::Init(const AesEcb &ecb, uint32_t headerLength, uint32_t plaintextLength, uint8_t tagLength,
                  const void *nonce, uint8_t nonceLength)
{
    const uint8_t *nonceBytes = reinterpret_cast<const uint8_t *>(nonce);
    uint8_t blockLength = 0;
    uint32_t len;
    uint8_t L;
    uint8_t i;

    mEcb = &ecb;

    // tagLength must be even
    tagLength &= ~1;

    if (tagLength > sizeof(mBlock))
    {
        tagLength = sizeof(mBlock);
    }

    L = 0;

    for (len = plaintextLength; len; len >>= 8)
    {
        L++;
    }

    if (L <= 1)
    {
        L = 2;
    }

    if (nonceLength > 13)
    {
        nonceLength = 13;
    }

    // increase L to match nonce len
    if (L < (15 - nonceLength))
    {
        L = 15 - nonceLength;
    }

    // decrease nonceLength to match L
    if (nonceLength > (15 - L))
    {
        nonceLength = 15 - L;
    }

    // setup initial block

    // write flags
    mBlock[0] = (static_cast<uint8_t>((headerLength != 0) << 6) |
                 static_cast<uint8_t>(((tagLength - 2) >> 1) << 3) |
                 static_cast<uint8_t>(L - 1));

    // write nonce
    for (i = 0; i < nonceLength; i++)
    {
        mBlock[1 + i] = nonceBytes[i];
    }

    // write len
    len = plaintextLength;

    for (i = sizeof(mBlock) - 1; i > nonceLength; i--)
    {
        mBlock[i] = len;
        len >>= 8;
    }

    // encrypt initial block
    mEcb->Encrypt(mBlock, mBlock);

    // process header
    if (headerLength > 0)
    {
        // process length
        if (headerLength < (65536U - 256U))
        {
            mBlock[blockLength++] ^= headerLength >> 8;
            mBlock[blockLength++] ^= headerLength >> 0;
        }
        else
        {
            mBlock[blockLength++] ^= 0xff;
            mBlock[blockLength++] ^= 0xfe;
            mBlock[blockLength++] ^= headerLength >> 24;
            mBlock[blockLength++] ^= headerLength >> 16;
            mBlock[blockLength++] ^= headerLength >> 8;
            mBlock[blockLength++] ^= headerLength >> 0;
        }
    }

    // init counter
    mCtr[0] = L - 1;

    for (i = 0; i < nonceLength; i++)
    {
        mCtr[1 + i] = nonceBytes[i];
    }

    for (i = i + 1; i < sizeof(mCtr); i++)
    {
        mCtr[i] = 0;
    }

    mNonceLength = nonceLength;
    mHeaderLength = headerLength;
    mHeaderCur = 0;
    mPlaintextLength = plaintextLength;
    mPlaintextCur = 0;
    mBlockLength = blockLength;
    mCtrLength = sizeof(mCtrPad);
    mTagLength = tagLength;
}

void AesCcm::Header(const void *header, uint32_t headerLength)
{
    const uint8_t *headerBytes = reinterpret_cast<const uint8_t *>(header);

    assert(mHeaderCur + headerLength <= mHeaderLength);

    // process header
    for (unsigned i = 0; i < headerLength; i++)
    {
        if (mBlockLength == sizeof(mBlock))
        {
            mEcb->Encrypt(mBlock, mBlock);
            mBlockLength = 0;
        }

        mBlock[mBlockLength++] ^= headerBytes[i];
    }

    mHeaderCur += headerLength;

    if (mHeaderCur == mHeaderLength)
    {
        // process remainder
        if (mBlockLength != 0)
        {
            mEcb->Encrypt(mBlock, mBlock);
        }

        mBlockLength = 0;
    }
}

void AesCcm::Payload(void *plaintext, void *ciphertext, uint32_t len, bool encrypt)
{
    uint8_t *plaintextBytes = reinterpret_cast<uint8_t *>(plaintext);
    uint8_t *ciphertextBytes = reinterpret_cast<uint8_t *>(ciphertext);
    uint8_t byte;

    assert(mPlaintextCur + len <= mPlaintextLength);

    for (unsigned i = 0; i < len; i++)
    {
        if (mCtrLength == 16)
        {
            for (int j = sizeof(mCtr) - 1; j > mNonceLength; j--)
            {
                if (++mCtr[j])
                {
                    break;
                }
            }

            mEcb->Encrypt(mCtr, mCtrPad);
            mCtrLength = 0;
        }

        if (encrypt)
        {
            byte = plaintextBytes[i];
            ciphertextBytes[i] = byte ^ mCtrPad[mCtrLength++];
        }
        else
        {
            byte = ciphertextBytes[i] ^ mCtrPad[mCtrLength++];
            plaintextBytes[i] = byte;
        }

        if (mBlockLength == sizeof(mBlock))
        {
            mEcb->Encrypt(mBlock, mBlock);
            mBlockLength = 0;
        }

        mBlock[mBlockLength++] ^= byte;
    }

    mPlaintextCur += len;

    if (mPlaintextCur >= mPlaintextLength)
    {
        if (mBlockLength != 0)
        {
            mEcb->Encrypt(mBlock, mBlock);
        }

        // reset counter
        for (uint8_t i = mNonceLength + 1; i < sizeof(mCtr); i++)
        {
            mCtr[i] = 0;
        }
    }
}

void AesCcm::Finalize(void *tag, uint8_t *tagLength)
{
    uint8_t *tagBytes = reinterpret_cast<uint8_t *>(tag);

    assert(mPlaintextCur == mPlaintextLength);

    if (mTagLength > 0)
    {
        mEcb->Encrypt(mCtr, mCtrPad);

        for (int i = 0; i < mTagLength; i++)
        {
            tagBytes[i] = mBlock[i] ^ mCtrPad[i];
        }
    }

    if (tagLength)
    {
        *tagLength = mTagLength;
    }
}

}  // namespace Crypto
}  // namespace Thread
