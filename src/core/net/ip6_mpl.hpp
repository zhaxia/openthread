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

#ifndef IP6_MPL_HPP_
#define IP6_MPL_HPP_

#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <common/timer.hpp>
#include <net/ip6.hpp>

namespace Thread {

class Ip6OptionMpl: public Ip6OptionHeader
{
public:
    enum
    {
        kType = 0x6d,    /* 01 1 01101 */
    };

    void Init() {
        Ip6OptionHeader::SetType(kType);
        Ip6OptionHeader::SetLength(sizeof(*this) - sizeof(Ip6OptionHeader));
    }

    enum SeedLength
    {
        kSeedLength0 = 0 << 6,
        kSeedLength2 = 1 << 6,
        kSeedLength8 = 2 << 6,
        kSeedLength16 = 3 << 6,
    };
    SeedLength GetSeedLength() { return static_cast<SeedLength>(m_control & kSeedLengthMask); }
    void SetSeedLength(SeedLength seed_length) { m_control = (m_control & ~kSeedLengthMask) | seed_length; }

    bool IsMaxFlagSet() { return m_control & kMaxFlag; }
    void ClearMaxFlag() { m_control &= ~kMaxFlag; }
    void SetMaxFlag() { m_control |= kMaxFlag; }

    uint8_t GetSequence() const { return m_sequence; }
    void SetSequence(uint8_t sequence) { m_sequence = sequence; }

    uint16_t GetSeed() const { return HostSwap16(m_seed); }
    void SetSeed(uint16_t seed) { m_seed = HostSwap16(seed); }

private:
    enum
    {
        kSeedLengthMask = 3 << 6,
        kMaxFlag = 1 << 5,
    };
    uint8_t m_control;
    uint8_t m_sequence;
    uint16_t m_seed;
} __attribute__((packed));

class Ip6Mpl
{
public:
    Ip6Mpl();
    void InitOption(Ip6OptionMpl &option, uint16_t seed);
    ThreadError ProcessOption(const Message &message);

private:
    enum
    {
        kNumEntries = 32,
        kLifetime = 5,  // seconds
    };

    static void HandleTimer(void *context);
    void HandleTimer();

    Timer m_timer;
    uint8_t m_sequence = 0;

    struct MplEntry
    {
        uint16_t seed;
        uint8_t sequence;
        uint8_t lifetime;
    };
    MplEntry m_entries[kNumEntries];
};

}  // namespace Thread

#endif  // NET_IP6_MPL_HPP_
