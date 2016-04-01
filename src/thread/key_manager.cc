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
#include <crypto/sha256.h>
#include <thread/key_manager.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>

namespace Thread {

static const uint8_t kThreadString[] =
{
    'T', 'h', 'r', 'e', 'a', 'd',
};

KeyManager::KeyManager(ThreadNetif &netif)
{
    m_netif = &netif;
}

ThreadError KeyManager::GetMasterKey(void *key, uint8_t *key_length) const
{
    if (key)
    {
        memcpy(key, m_master_key, m_master_key_length);
    }

    if (key_length)
    {
        *key_length = m_master_key_length;
    }

    return kThreadError_None;
}

ThreadError KeyManager::SetMasterKey(const void *key, uint8_t key_length)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(key_length <= sizeof(m_master_key), error = kThreadError_InvalidArgs);
    memcpy(m_master_key, key, key_length);
    m_master_key_length = key_length;
    m_current_key_sequence = 0;
    ComputeKey(m_current_key_sequence, m_current_key);

exit:
    return error;
}

ThreadError KeyManager::ComputeKey(uint32_t key_sequence, uint8_t *key)
{
    Crypto::Sha256 sha256;
    Crypto::Hmac hmac(sha256);
    uint8_t key_sequence_bytes[4];

    hmac.SetKey(m_master_key, m_master_key_length);
    hmac.Init();

    key_sequence_bytes[0] = key_sequence >> 24;
    key_sequence_bytes[1] = key_sequence >> 16;
    key_sequence_bytes[2] = key_sequence >> 8;
    key_sequence_bytes[3] = key_sequence >> 0;
    hmac.Input(key_sequence_bytes, sizeof(key_sequence_bytes));
    hmac.Input(kThreadString, sizeof(kThreadString));

    hmac.Finalize(key);

    return kThreadError_None;
}

uint32_t KeyManager::GetCurrentKeySequence() const
{
    return m_current_key_sequence;
}

void KeyManager::UpdateNeighbors()
{
    uint8_t num_neighbors;
    Router *routers;
    Child *children;

    routers = m_netif->GetMle()->GetParent();
    routers->previous_key = true;

    routers = m_netif->GetMle()->GetRouters(&num_neighbors);

    for (int i = 0; i < num_neighbors; i++)
    {
        routers[i].previous_key = true;
    }

    children = m_netif->GetMle()->GetChildren(&num_neighbors);

    for (int i = 0; i < num_neighbors; i++)
    {
        children[i].previous_key = true;
    }
}

ThreadError KeyManager::SetCurrentKeySequence(uint32_t key_sequence)
{
    ThreadError error = kThreadError_None;

    m_previous_key_valid = true;
    m_previous_key_sequence = m_current_key_sequence;
    memcpy(m_previous_key, m_current_key, sizeof(m_previous_key));

    m_current_key_sequence = key_sequence;
    ComputeKey(m_current_key_sequence, m_current_key);

    m_mac_frame_counter = 0;
    m_mle_frame_counter = 0;

    UpdateNeighbors();

    return error;
}

const uint8_t *KeyManager::GetCurrentMacKey() const
{
    return m_current_key + 16;
}

const uint8_t *KeyManager::GetCurrentMleKey() const
{
    return m_current_key;
}

bool KeyManager::IsPreviousKeyValid() const
{
    return m_previous_key_valid;
}

uint32_t KeyManager::GetPreviousKeySequence() const
{
    return m_previous_key_sequence;
}

const uint8_t *KeyManager::GetPreviousMacKey() const
{
    return m_previous_key + 16;
}

const uint8_t *KeyManager::GetPreviousMleKey() const
{
    return m_previous_key;
}

const uint8_t *KeyManager::GetTemporaryMacKey(uint32_t key_sequence)
{
    ComputeKey(key_sequence, m_temporary_key);
    return m_temporary_key + 16;
}

const uint8_t *KeyManager::GetTemporaryMleKey(uint32_t key_sequence)
{
    ComputeKey(key_sequence, m_temporary_key);
    return m_temporary_key;
}

uint32_t KeyManager::GetMacFrameCounter() const
{
    return m_mac_frame_counter;
}

uint32_t KeyManager::GetMleFrameCounter() const
{
    return m_mle_frame_counter;
}

ThreadError KeyManager::IncrementMacFrameCounter()
{
    m_mac_frame_counter++;
    return kThreadError_None;
}

ThreadError KeyManager::IncrementMleFrameCounter()
{
    m_mle_frame_counter++;
    return kThreadError_None;
}

}  // namespace Thread
