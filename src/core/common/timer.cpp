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
 *   This file implements a multiplexed timer service on top of the alarm abstraction.
 */

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>
#include <common/timer.hpp>
#include <platform/alarm.h>

namespace Thread {

static Tasklet  sTask(&TimerScheduler::FireTimers, NULL);
static Timer   *sHead = NULL;
static Timer   *sTail = NULL;

void TimerScheduler::Init(void)
{
    otAlarmInit();
}

void TimerScheduler::Add(Timer &aTimer)
{
    VerifyOrExit(aTimer.mNext == NULL && sTail != &aTimer, ;);

    if (sTail == NULL)
    {
        sHead = &aTimer;
    }
    else
    {
        sTail->mNext = &aTimer;
    }

    aTimer.mNext = NULL;
    sTail = &aTimer;

exit:
    SetAlarm();
}

void TimerScheduler::Remove(Timer &aTimer)
{
    VerifyOrExit(aTimer.mNext != NULL || sTail == &aTimer, ;);

    if (sHead == &aTimer)
    {
        sHead = aTimer.mNext;

        if (sTail == &aTimer)
        {
            sTail = NULL;
        }
    }
    else
    {
        for (Timer *cur = sHead; cur; cur = cur->mNext)
        {
            if (cur->mNext == &aTimer)
            {
                cur->mNext = aTimer.mNext;

                if (sTail == &aTimer)
                {
                    sTail = cur;
                }

                break;
            }
        }
    }

    aTimer.mNext = NULL;
    SetAlarm();

exit:
    {}
}

bool TimerScheduler::IsAdded(const Timer &aTimer)
{
    bool rval = false;

    if (sHead == &aTimer)
    {
        ExitNow(rval = true);
    }

    for (Timer *cur = sHead; cur; cur = cur->mNext)
    {
        if (cur == &aTimer)
        {
            ExitNow(rval = true);
        }
    }

exit:
    return rval;
}

void TimerScheduler::SetAlarm(void)
{
    uint32_t now = otAlarmGetNow();
    int32_t  minRemaining = (1UL << 31) - 1;
    uint32_t elapsed;
    int32_t  remaining;

    if (sHead == NULL)
    {
        otAlarmStop();
        ExitNow();
    }

    for (Timer *timer = sHead; timer; timer = timer->mNext)
    {
        elapsed = now - timer->mT0;
        remaining = timer->mDt - elapsed;

        if (remaining < minRemaining)
        {
            minRemaining = remaining;
        }
    }

    if (minRemaining <= 0)
    {
        sTask.Post();
    }
    else
    {
        otAlarmStartAt(now, minRemaining);
    }

exit:
    {}
}

extern "C" void otAlarmSignalFired(void)
{
    Thread::sTask.Post();
}

void TimerScheduler::FireTimers(void *aContext)
{
    uint32_t now = otAlarmGetNow();
    uint32_t elapsed;

    for (Timer *cur = sHead; cur; cur = cur->mNext)
    {
        elapsed = now - cur->mT0;

        if (elapsed >= cur->mDt)
        {
            Remove(*cur);
            cur->Fired();
            break;
        }
    }

    SetAlarm();
}

}  // namespace Thread
