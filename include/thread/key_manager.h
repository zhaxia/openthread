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

#ifndef KEY_MANAGER_H_
#define KEY_MANAGER_H_

#include <stdint.h>
#include <common/thread_error.h>

namespace Thread {

class ThreadNetif;

class KeyManager
{
public:
    explicit KeyManager(ThreadNetif &netif);
    ThreadError GetMasterKey(void *key, uint8_t *key_length) const;
    ThreadError SetMasterKey(const void *key, uint8_t key_length);

    uint32_t GetCurrentKeySequence() const;
    ThreadError SetCurrentKeySequence(uint32_t key_sequence);
    const uint8_t *GetCurrentMacKey() const;
    const uint8_t *GetCurrentMleKey() const;

    bool IsPreviousKeyValid() const;
    uint32_t GetPreviousKeySequence() const;
    const uint8_t *GetPreviousMacKey() const;
    const uint8_t *GetPreviousMleKey() const;

    const uint8_t *GetTemporaryMacKey(uint32_t key_sequence);
    const uint8_t *GetTemporaryMleKey(uint32_t key_sequence);

    uint32_t GetMacFrameCounter() const;
    uint32_t GetMleFrameCounter() const;

    ThreadError IncrementMacFrameCounter();
    ThreadError IncrementMleFrameCounter();

private:
    ThreadError ComputeKey(uint32_t key_sequence, uint8_t *key);
    void UpdateNeighbors();

    uint8_t m_master_key[16];
    uint8_t m_master_key_length;

    uint32_t m_previous_key_sequence;
    uint8_t m_previous_key[32];
    bool m_previous_key_valid = false;

    uint32_t m_current_key_sequence;
    uint8_t m_current_key[32];

    uint8_t m_temporary_key[32];

    uint32_t m_mac_frame_counter = 0;
    uint32_t m_mle_frame_counter = 0;

    ThreadNetif *m_netif;
};

}  // namespace Thread

#endif  // KEY_MANAGER_H_
