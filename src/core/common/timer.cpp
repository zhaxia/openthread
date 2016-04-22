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

static Tasklet s_task(&Timer::FireTimers, NULL);
static Timer *sHead = NULL;
static Timer *sTail = NULL;

Timer::Timer(Handler handler, void *context)
{
    mHandler = handler;
    mContext = context;
}

ThreadError Timer::StartAt(uint32_t t0, uint32_t dt)
{
    mT0 = t0;
    mDt = dt;
    return Add(*this);
}

ThreadError Timer::Start(uint32_t dt)
{
    return StartAt(GetNow(), dt);
}

ThreadError Timer::Stop()
{
    return Remove(*this);
}

bool Timer::IsRunning() const
{
    return IsAdded(*this);
}

uint32_t Timer::Gett0() const
{
    return mT0;
}

uint32_t Timer::Getdt() const
{
    return mDt;
}

uint32_t Timer::GetNow()
{
    return ot_alarm_get_now();
}

void Timer::Init()
{
    dprintf("Timer init\n");
    ot_alarm_init();
}

ThreadError Timer::Add(Timer &timer)
{
    VerifyOrExit(timer.mNext == NULL && sTail != &timer, ;);

    if (sTail == NULL)
    {
        sHead = &timer;
    }
    else
    {
        sTail->mNext = &timer;
    }

    timer.mNext = NULL;
    sTail = &timer;

exit:
    SetAlarm();
    return kThreadError_None;
}

ThreadError Timer::Remove(Timer &timer)
{
    VerifyOrExit(timer.mNext != NULL || sTail == &timer, ;);

    if (sHead == &timer)
    {
        sHead = timer.mNext;

        if (sTail == &timer)
        {
            sTail = NULL;
        }
    }
    else
    {
        for (Timer *cur = sHead; cur; cur = cur->mNext)
        {
            if (cur->mNext == &timer)
            {
                cur->mNext = timer.mNext;

                if (sTail == &timer)
                {
                    sTail = cur;
                }

                break;
            }
        }
    }

    timer.mNext = NULL;
    SetAlarm();

exit:
    return kThreadError_None;
}

bool Timer::IsAdded(const Timer &timer)
{
    bool rval = false;

    if (sHead == &timer)
    {
        ExitNow(rval = true);
    }

    for (Timer *cur = sHead; cur; cur = cur->mNext)
    {
        if (cur == &timer)
        {
            ExitNow(rval = true);
        }
    }

exit:
    return rval;
}

void Timer::SetAlarm()
{
    uint32_t now = ot_alarm_get_now();
    int32_t minRemaining = (1UL << 31) - 1;
    uint32_t elapsed;
    int32_t remaining;

    if (sHead == NULL)
    {
        ot_alarm_stop();
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
        s_task.Post();
    }
    else
    {
        ot_alarm_start_at(now, minRemaining);
    }

exit:
    {}
}

extern "C" void ot_alarm_signal_fired()
{
    Thread::s_task.Post();
}

void Timer::FireTimers(void *context)
{
    uint32_t now = ot_alarm_get_now();
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
