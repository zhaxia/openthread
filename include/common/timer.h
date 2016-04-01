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

#ifndef TIMER_H_
#define TIMER_H_

#include <common/tasklet.h>
#include <common/thread_error.h>
#include <stddef.h>
#include <stdint.h>

namespace Thread {

class Timer
{
public:
    typedef void (*Handler)(void *context);
    Timer(Handler handler, void *context);

    uint32_t Gett0() const;
    uint32_t Getdt() const;
    bool IsRunning() const;
    ThreadError Start(uint32_t dt);
    ThreadError StartAt(uint32_t t0, uint32_t dt);
    ThreadError Stop();

    static void Init();
    static uint32_t GetNow();
    static void FireTimers(void *context);

private:
    static ThreadError Add(Timer &timer);
    static ThreadError Remove(Timer &timer);
    static bool IsAdded(const Timer &timer);
    static void SetAlarm();

    void Fired() { m_handler(m_context); }

    Handler m_handler;
    void *m_context;
    uint32_t m_t0 = 0;
    uint32_t m_dt = 0;
    Timer *m_next = NULL;
};

}  // namespace Thread

#endif  // TIMER_H_
