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

#include <common/code_utils.h>
#include <common/thread_error.h>
#include <common/timer.h>
#include <platform/common/alarm.h>

namespace Thread {

static Tasklet s_task(&Timer::FireTimers, NULL);
static Timer *s_head = NULL;
static Timer *s_tail = NULL;

Timer::Timer(Handler handler, void *context)
{
    m_handler = handler;
    m_context = context;
}

ThreadError Timer::StartAt(uint32_t t0, uint32_t dt)
{
    m_t0 = t0;
    m_dt = dt;
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
    return m_t0;
}

uint32_t Timer::Getdt() const
{
    return m_dt;
}

uint32_t Timer::GetNow()
{
    return alarm_get_now();
}

void Timer::Init()
{
    dprintf("Timer init\n");
    alarm_init();
}

ThreadError Timer::Add(Timer &timer)
{
    VerifyOrExit(timer.m_next == NULL && s_tail != &timer, ;);

    if (s_tail == NULL)
    {
        s_head = &timer;
    }
    else
    {
        s_tail->m_next = &timer;
    }

    timer.m_next = NULL;
    s_tail = &timer;

exit:
    SetAlarm();
    return kThreadError_None;
}

ThreadError Timer::Remove(Timer &timer)
{
    VerifyOrExit(timer.m_next != NULL || s_tail == &timer, ;);

    if (s_head == &timer)
    {
        s_head = timer.m_next;

        if (s_tail == &timer)
        {
            s_tail = NULL;
        }
    }
    else
    {
        for (Timer *cur = s_head; cur; cur = cur->m_next)
        {
            if (cur->m_next == &timer)
            {
                cur->m_next = timer.m_next;

                if (s_tail == &timer)
                {
                    s_tail = cur;
                }

                break;
            }
        }
    }

    timer.m_next = NULL;
    SetAlarm();

exit:
    return kThreadError_None;
}

bool Timer::IsAdded(const Timer &timer)
{
    bool rval = false;

    if (s_head == &timer)
    {
        ExitNow(rval = true);
    }

    for (Timer *cur = s_head; cur; cur = cur->m_next)
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
    uint32_t now = alarm_get_now();
    int32_t min_remaining = (1UL << 31) - 1;
    uint32_t elapsed;
    int32_t remaining;

    if (s_head == NULL)
    {
        alarm_stop();
        ExitNow();
    }

    for (Timer *timer = s_head; timer; timer = timer->m_next)
    {
        elapsed = now - timer->m_t0;
        remaining = timer->m_dt - elapsed;

        if (remaining < min_remaining)
        {
            min_remaining = remaining;
        }
    }

    if (min_remaining <= 0)
    {
        s_task.Post();
    }
    else
    {
        alarm_start_at(now, min_remaining);
    }

exit:
    {}
}

extern "C" void alarm_fired()
{
    Thread::s_task.Post();
}

void Timer::FireTimers(void *context)
{
    uint32_t now = alarm_get_now();
    uint32_t elapsed;

    for (Timer *cur = s_head; cur; cur = cur->m_next)
    {
        elapsed = now - cur->m_t0;

        if (elapsed >= cur->m_dt)
        {
            Remove(*cur);
            cur->Fired();
            break;
        }
    }

    SetAlarm();
}

}  // namespace Thread
