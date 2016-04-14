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
#include <crypto/hmac.hpp>

namespace Thread {
namespace Crypto {

Hmac::Hmac(Hash &hash)
{
    m_hash = &hash;
}

ThreadError Hmac::SetKey(const void *key, uint16_t key_length)
{
    if (key_length > kMaxKeyLength)
    {
        m_hash->Init();
        m_hash->Input(key, key_length);
        m_hash->Finalize(m_key);
        m_key_length = m_hash->GetSize();
    }
    else
    {
        memcpy(m_key, key, key_length);
        m_key_length = key_length;
    }

    return kThreadError_None;
}

ThreadError Hmac::Init()
{
    uint8_t pad[kMaxKeyLength];
    int i;

    for (i = 0; i < m_key_length; i++)
    {
        pad[i] = m_key[i] ^ 0x36;
    }

    for (; i < 64; i++)
    {
        pad[i] = 0x36;
    }

    // start inner hash
    m_hash->Init();
    m_hash->Input(pad, sizeof(pad));

    return kThreadError_None;
}

ThreadError Hmac::Input(const void *buf, uint16_t buf_length)
{
    return m_hash->Input(buf, buf_length);
}

ThreadError Hmac::Finalize(uint8_t *hash)
{
    uint8_t pad[kMaxKeyLength];
    int i;

    // finish inner hash
    m_hash->Finalize(hash);

    // perform outer hash
    for (i = 0; i < m_key_length; i++)
    {
        pad[i] = m_key[i] ^ 0x5c;
    }

    for (; i < 64; i++)
    {
        pad[i] = 0x5c;
    }

    m_hash->Init();
    m_hash->Input(pad, kMaxKeyLength);
    m_hash->Input(hash, m_hash->GetSize());
    m_hash->Finalize(hash);

    return kThreadError_None;
}

}  // namespace Crypto
}  // namespace Thread
