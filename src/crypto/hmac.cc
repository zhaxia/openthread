/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#include <common/code_utils.h>
#include <common/thread_error.h>
#include <crypto/hmac.h>

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
