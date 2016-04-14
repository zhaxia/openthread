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

#ifndef TIMER_HPP_
#define TIMER_HPP_

#include <stddef.h>
#include <stdint.h>

#include <common/tasklet.hpp>
#include <common/thread_error.hpp>

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

#endif  // TIMER_HPP_
