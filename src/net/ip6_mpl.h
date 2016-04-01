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

#ifndef IP6_MPL_H_
#define IP6_MPL_H_

#include <common/message.h>
#include <common/thread_error.h>
#include <common/timer.h>
#include <net/ip6.h>

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

#endif  // NET_IP6_MPL_H_
