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
#include <common/message.hpp>
#include <net/ip6_mpl.hpp>

namespace Thread {

Ip6Mpl::Ip6Mpl():
    mTimer(&HandleTimer, this)
{
    memset(mEntries, 0, sizeof(mEntries));
}

void Ip6Mpl::InitOption(Ip6OptionMpl &option, uint16_t seed)
{
    option.Init();
    option.SetSeedLength(Ip6OptionMpl::kSeedLength2);
    option.SetSequence(mSequence++);
    option.SetSeed(seed);
}

ThreadError Ip6Mpl::ProcessOption(const Message &message)
{
    ThreadError error = kThreadError_None;
    Ip6OptionMpl option;
    MplEntry *entry = NULL;
    int8_t diff;

    VerifyOrExit(message.Read(message.GetOffset(), sizeof(option), &option) == sizeof(option) &&
                 option.GetLength() == sizeof(Ip6OptionMpl) - sizeof(Ip6OptionHeader),
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

void Ip6Mpl::HandleTimer(void *context)
{
    Ip6Mpl *obj = reinterpret_cast<Ip6Mpl *>(context);
    obj->HandleTimer();
}

void Ip6Mpl::HandleTimer()
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

}  // namespace Thread
