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
 *   This file includes definitions for MPL.
 */

#include <openthread-types.h>
#include <common/message.hpp>
#include <common/timer.hpp>
#include <net/ip6.hpp>

namespace Thread {
namespace Ip6 {

/**
 * @addtogroup core-ip6-mpl
 *
 * @brief
 *   This module includes definitions for MPL.
 *
 * @{
 *
 */

/**
 * This class implements MPL header generation and parsing.
 *
 */
class OptionMpl: public OptionHeader
{
public:
    enum
    {
        kType = 0x6d,    /* 01 1 01101 */
    };

    /**
     * This method initializes the MPL header.
     *
     */
    void Init() {
        OptionHeader::SetType(kType);
        OptionHeader::SetLength(sizeof(*this) - sizeof(OptionHeader));
    }

    /**
     * MPL Seed lengths.
     */
    enum SeedLength
    {
        kSeedLength0  = 0 << 6,  ///< 0-byte MPL Seed Length.
        kSeedLength2  = 1 << 6,  ///< 2-byte MPL Seed Length.
        kSeedLength8  = 2 << 6,  ///< 8-byte MPL Seed Length.
        kSeedLength16 = 3 << 6,  ///< 16-byte MPL Seed Length.
    };

    /**
     * This method returns the MPL Seed Length value.
     *
     * @returns The MPL Seed Length value.
     *
     */
    SeedLength GetSeedLength() { return static_cast<SeedLength>(mControl & kSeedLengthMask); }

    /**
     * This method sets the MPL Seed Length value.
     *
     * @param[in]  aSeedLength  The MPL Seed Length.
     *
     */
    void SetSeedLength(SeedLength aSeedLength) { mControl = (mControl & ~kSeedLengthMask) | aSeedLength; }

    /**
     * This method indicates whether or not the MPL M flag is set.
     *
     * @retval TRUE   If the MPL M flag is set.
     * @retval FALSE  If the MPL M flag is not set.
     *
     */
    bool IsMaxFlagSet() { return mControl & kMaxFlag; }

    /**
     * This method clears the MPL M flag.
     *
     */
    void ClearMaxFlag() { mControl &= ~kMaxFlag; }

    /**
     * This method sets the MPL M flag.
     *
     */
    void SetMaxFlag() { mControl |= kMaxFlag; }

    /**
     * This method returns the MPL Sequence value.
     *
     * @returns The MPL Sequence value.
     *
     */
    uint8_t GetSequence() const { return mSequence; }

    /**
     * This method sets the MPL Sequence value.
     *
     * @param[in]  aSequence  The MPL Sequence value.
     */
    void SetSequence(uint8_t aSequence) { mSequence = aSequence; }

    /**
     * This method returns the MPL Seed value.
     *
     * @returns The MPL Seed value.
     *
     */
    uint16_t GetSeed() const { return HostSwap16(mSeed); }

    /**
     * This method sets the MPL Seed value.
     *
     * @param[in]  aSeed  The MPL Seed value.
     */
    void SetSeed(uint16_t aSeed) { mSeed = HostSwap16(aSeed); }

private:
    enum
    {
        kSeedLengthMask = 3 << 6,
        kMaxFlag = 1 << 5,
    };
    uint8_t  mControl;
    uint8_t  mSequence;
    uint16_t mSeed;
} __attribute__((packed));

/**
 * This class implements MPL message processing.
 *
 */
class Mpl
{
public:
    /**
     * This constructor initializes the MPL object.
     *
     */
    Mpl(void);

    /**
     * This method initializes the MPL option.
     *
     * @param[in]  aOption  A reference to the MPL header to initialize.
     * @param[in]  aSeed    The MPL Seed value to use.
     *
     */
    void InitOption(OptionMpl &aOption, uint16_t aSeed);

    /**
     * This method processes an MPL option.
     *
     * @param[in]  aMessage  A reference to the message.
     *
     * @retval kThreadError_None  Successfully processed the MPL option.
     * @retval kThreadError_Drop  The MPL message is a duplicate and should be dropped.
     *
     */
    ThreadError ProcessOption(const Message &aMessage);

private:
    enum
    {
        kNumEntries = OPENTHREAD_CONFIG_MPL_CACHE_ENTRIES,
        kLifetime = OPENTHREAD_CONFIG_MPL_CACHE_ENTRY_LIFETIME,
    };

    static void HandleTimer(void *context);
    void HandleTimer();

    Timer mTimer;
    uint8_t mSequence;

    struct MplEntry
    {
        uint16_t mSeed;
        uint8_t mSequence;
        uint8_t mLifetime;
    };
    MplEntry mEntries[kNumEntries];
};

/**
 * @}
 *
 */

}  // namespace Ip6
}  // namespace Thread

#endif  // NET_IP6_MPL_HPP_
