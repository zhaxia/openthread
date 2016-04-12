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
