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

/**
 * @file
 *   This file contains definitions for MPL.
 */

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
    SeedLength GetSeedLength() { return static_cast<SeedLength>(mControl & kSeedLengthMask); }
    void SetSeedLength(SeedLength seedLength) { mControl = (mControl & ~kSeedLengthMask) | seedLength; }

    bool IsMaxFlagSet() { return mControl & kMaxFlag; }
    void ClearMaxFlag() { mControl &= ~kMaxFlag; }
    void SetMaxFlag() { mControl |= kMaxFlag; }

    uint8_t GetSequence() const { return mSequence; }
    void SetSequence(uint8_t sequence) { mSequence = sequence; }

    uint16_t GetSeed() const { return HostSwap16(mSeed); }
    void SetSeed(uint16_t seed) { mSeed = HostSwap16(seed); }

private:
    enum
    {
        kSeedLengthMask = 3 << 6,
        kMaxFlag = 1 << 5,
    };
    uint8_t mControl;
    uint8_t mSequence;
    uint16_t mSeed;
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

    Timer mTimer;
    uint8_t mSequence = 0;

    struct MplEntry
    {
        uint16_t mSeed;
        uint8_t mSequence;
        uint8_t mLifetime;
    };
    MplEntry mEntries[kNumEntries];
};

}  // namespace Thread

#endif  // NET_IP6_MPL_HPP_
