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

#include <common/code_utils.h>
#include <common/thread_error.h>
#include <crypto/aes_ccm.h>

namespace Thread {
namespace Crypto {

void AesCcm::Init(const AesEcb &ecb, uint32_t header_length, uint32_t plaintext_length, uint8_t tag_length,
                  const void *nonce, uint8_t nonce_length)
{
    const uint8_t *nonce_bytes = reinterpret_cast<const uint8_t *>(nonce);
    uint8_t block_length = 0;
    uint32_t len;
    uint8_t L;
    uint8_t i;

    m_ecb = &ecb;

    // tag_length must be even
    tag_length &= ~1;

    if (tag_length > sizeof(m_block))
    {
        tag_length = sizeof(m_block);
    }

    L = 0;

    for (len = plaintext_length; len; len >>= 8)
    {
        L++;
    }

    if (L <= 1)
    {
        L = 2;
    }

    if (nonce_length > 13)
    {
        nonce_length = 13;
    }

    // increase L to match nonce len
    if (L < (15 - nonce_length))
    {
        L = 15 - nonce_length;
    }

    // decrease nonce_length to match L
    if (nonce_length > (15 - L))
    {
        nonce_length = 15 - L;
    }

    // setup initial block

    // write flags
    m_block[0] = (static_cast<uint8_t>((header_length != 0) << 6) |
                  static_cast<uint8_t>(((tag_length - 2) >> 1) << 3) |
                  static_cast<uint8_t>(L - 1));

    // write nonce
    for (i = 0; i < nonce_length; i++)
    {
        m_block[1 + i] = nonce_bytes[i];
    }

    // write len
    len = plaintext_length;

    for (i = sizeof(m_block) - 1; i > nonce_length; i--)
    {
        m_block[i] = len;
        len >>= 8;
    }

    // encrypt initial block
    m_ecb->Encrypt(m_block, m_block);

    // process header
    if (header_length > 0)
    {
        // process length
        if (header_length < (65536U - 256U))
        {
            m_block[block_length++] ^= header_length >> 8;
            m_block[block_length++] ^= header_length >> 0;
        }
        else
        {
            m_block[block_length++] ^= 0xff;
            m_block[block_length++] ^= 0xfe;
            m_block[block_length++] ^= header_length >> 24;
            m_block[block_length++] ^= header_length >> 16;
            m_block[block_length++] ^= header_length >> 8;
            m_block[block_length++] ^= header_length >> 0;
        }
    }

    // init counter
    m_ctr[0] = L - 1;

    for (i = 0; i < nonce_length; i++)
    {
        m_ctr[1 + i] = nonce_bytes[i];
    }

    for (i = i + 1; i < sizeof(m_ctr); i++)
    {
        m_ctr[i] = 0;
    }

    m_nonce_length = nonce_length;
    m_header_length = header_length;
    m_header_cur = 0;
    m_plaintext_length = plaintext_length;
    m_plaintext_cur = 0;
    m_block_length = block_length;
    m_ctr_length = sizeof(m_ctrpad);
    m_tag_length = tag_length;
}

void AesCcm::Header(const void *header, uint32_t header_length)
{
    const uint8_t *header_bytes = reinterpret_cast<const uint8_t *>(header);

    assert(m_header_cur + header_length <= m_header_length);

    // process header
    for (unsigned i = 0; i < header_length; i++)
    {
        if (m_block_length == sizeof(m_block))
        {
            m_ecb->Encrypt(m_block, m_block);
            m_block_length = 0;
        }

        m_block[m_block_length++] ^= header_bytes[i];
    }

    m_header_cur += header_length;

    if (m_header_cur == m_header_length)
    {
        // process remainder
        if (m_block_length != 0)
        {
            m_ecb->Encrypt(m_block, m_block);
        }

        m_block_length = 0;
    }
}

void AesCcm::Payload(void *plaintext, void *ciphertext, uint32_t len, bool encrypt)
{
    uint8_t *plaintext_bytes = reinterpret_cast<uint8_t *>(plaintext);
    uint8_t *ciphertext_bytes = reinterpret_cast<uint8_t *>(ciphertext);
    uint8_t byte;

    assert(m_plaintext_cur + len <= m_plaintext_length);

    for (unsigned i = 0; i < len; i++)
    {
        if (m_ctr_length == 16)
        {
            for (int j = sizeof(m_ctr) - 1; j > m_nonce_length; j--)
            {
                if (++m_ctr[j])
                {
                    break;
                }
            }

            m_ecb->Encrypt(m_ctr, m_ctrpad);
            m_ctr_length = 0;
        }

        if (encrypt)
        {
            byte = plaintext_bytes[i];
            ciphertext_bytes[i] = byte ^ m_ctrpad[m_ctr_length++];
        }
        else
        {
            byte = ciphertext_bytes[i] ^ m_ctrpad[m_ctr_length++];
            plaintext_bytes[i] = byte;
        }

        if (m_block_length == sizeof(m_block))
        {
            m_ecb->Encrypt(m_block, m_block);
            m_block_length = 0;
        }

        m_block[m_block_length++] ^= byte;
    }

    m_plaintext_cur += len;

    if (m_plaintext_cur >= m_plaintext_length)
    {
        if (m_block_length != 0)
        {
            m_ecb->Encrypt(m_block, m_block);
        }

        // reset counter
        for (uint8_t i = m_nonce_length + 1; i < sizeof(m_ctr); i++)
        {
            m_ctr[i] = 0;
        }
    }
}

void AesCcm::Finalize(void *tag, uint8_t *tag_length)
{
    uint8_t *tag_bytes = reinterpret_cast<uint8_t *>(tag);

    assert(m_plaintext_cur == m_plaintext_length);

    if (m_tag_length > 0)
    {
        m_ecb->Encrypt(m_ctr, m_ctrpad);

        for (int i = 0; i < m_tag_length; i++)
        {
            tag_bytes[i] = m_block[i] ^ m_ctrpad[i];
        }
    }

    if (tag_length)
    {
        *tag_length = m_tag_length;
    }
}

}  // namespace Crypto
}  // namespace Thread
