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

/**
 * @file
 *   This file implements MPL.
 */

#include <common/code_utils.hpp>
#include <common/message.hpp>
#include <net/ip6_mpl.hpp>

namespace Thread {
namespace Ip6 {

Mpl::Mpl():
    mTimer(&HandleTimer, this)
{
    memset(mEntries, 0, sizeof(mEntries));
    mSequence = 0;
}

void Mpl::InitOption(OptionMpl &aOption, uint16_t aSeed)
{
    aOption.Init();
    aOption.SetSeedLength(OptionMpl::kSeedLength2);
    aOption.SetSequence(mSequence++);
    aOption.SetSeed(aSeed);
}

ThreadError Mpl::ProcessOption(const Message &aMessage)
{
    ThreadError error = kThreadError_None;
    OptionMpl option;
    MplEntry *entry = NULL;
    int8_t diff;

    VerifyOrExit(aMessage.Read(aMessage.GetOffset(), sizeof(option), &option) == sizeof(option) &&
                 option.GetLength() == sizeof(OptionMpl) - sizeof(OptionHeader),
                 error = kThreadError_Drop);

    for (int i = 0; i < kNumEntries; i++)
    {
        if (mEntries[i].mLifetime == 0)
        {
            entry = &mEntries[i];
        }
        else if (mEntries[i].mSeed == option.GetSeed())
        {
            entry = &mEntries[i];
            diff = option.GetSequence() - entry->mSequence;

            if (diff <= 0)
            {
                error = kThreadError_Drop;
            }

            break;
        }
    }

    VerifyOrExit(entry != NULL, error = kThreadError_Drop);

    entry->mSeed = option.GetSeed();
    entry->mSequence = option.GetSequence();
    entry->mLifetime = kLifetime;
    mTimer.Start(1000);

exit:
    return error;
}

void Mpl::HandleTimer(void *aContext)
{
    Mpl *obj = reinterpret_cast<Mpl *>(aContext);
    obj->HandleTimer();
}

void Mpl::HandleTimer()
{
    bool startTimer = false;

    for (int i = 0; i < kNumEntries; i++)
    {
        if (mEntries[i].mLifetime > 0)
        {
            mEntries[i].mLifetime--;
            startTimer = true;
        }
    }

    if (startTimer)
    {
        mTimer.Start(1000);
    }
}

}  // namespace Ip6
}  // namespace Thread
