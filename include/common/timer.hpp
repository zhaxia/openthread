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
 *   This file includes definitions for the multiplexed timer service.
 */

#ifndef TIMER_HPP_
#define TIMER_HPP_

#include <stddef.h>
#include <stdint.h>

#include <common/tasklet.hpp>
#include <common/thread_error.hpp>
#include <platform/alarm.h>

namespace Thread {

class Timer;

/**
 * @addtogroup core-timer
 *
 * @brief
 *   This module includes definitions for the multiplexed timer service.
 *
 * @{
 *
 */

/**
 * This class implements the timer scheduler.
 *
 */
class TimerScheduler
{
    friend Timer;

public:
    /**
     * This static method initializes the timer service.
     *
     */
    static void Init(void);

    /**
     * This static method adds a timer instance to the timer scheduler.
     *
     * @param[in]  aTimer  A reference to the timer instance.
     *
     */
    static void Add(Timer &aTimer);

    /**
     * This static method removes a timer instance to the timer scheduler.
     *
     * @param[in]  aTimer  A reference to the timer instance.
     *
     */
    static void Remove(Timer &aTimer);

    /**
     * This static method returns whether or not the timer instance is already added.
     *
     * @retval TRUE   If the timer instance is already added.
     * @retval FALSE  If the timer instance is not added.
     *
     */
    static bool IsAdded(const Timer &aTimer);

    /**
     * This static method processes all running timers.
     *
     * @param[in]  aContext  A pointer to arbitrary context information.
     *
     */
    static void FireTimers(void *aContext);

private:
    /**
     * This static method
     */
    static void SetAlarm(void);
};

/**
 * This class implements a timer.
 *
 */
class Timer
{
    friend TimerScheduler;

public:
    /**
     * This function pointer is called when the timer expires.
     *
     * @param[in]  aContext  A pointer to arbitrary context information.
     */
    typedef void (*Handler)(void *aContext);

    /**
     * This constructor creates a timer instance.
     *
     * @param[in]  aHandler  A pointer to a function that is called when the timer expires.
     * @param[in]  aContext  A pointer to arbitrary context information.
     *
     */
    Timer(Handler aHandler, void *aContext) { mNext = NULL; mHandler = aHandler; mContext = aContext; }

    /**
     * This method returns the start time in milliseconds for the timer.
     *
     * @returns The start time in milliseconds.
     *
     */
    uint32_t Gett0(void) const { return mT0; }

    /**
     * This method returns the delta time in milliseconds for the timer.
     *
     * @returns The delta time.
     *
     */
    uint32_t Getdt(void) const { return mDt; }

    /**
     * This method indicates whether or not the timer instance is running.
     *
     * @retval TRUE   If the timer is running.
     * @retval FALSE  If the timer is not running.
     */
    bool IsRunning(void) const { return TimerScheduler::IsAdded(*this); }

    /**
     * This method schedules the timer to fire a @p dt milliseconds from now.
     *
     * @param[in]  aDt  The expire time in milliseconds from now.
     */
    void Start(uint32_t aDt) { StartAt(GetNow(), aDt); }

    /**
     * This method schedules the timer to fire at @p dt milliseconds from @p t0.
     *
     * @param[in]  aT0  The start time in milliseconds.
     * @param[in]  aDt  The expire time in milliseconds from @p t0.
     */
    void StartAt(uint32_t aT0, uint32_t aDt) { mT0 = aT0; mDt = aDt; TimerScheduler::Add(*this); }

    /**
     * This method stops the timer.
     *
     */
    void Stop(void) { TimerScheduler::Remove(*this); }

    /**
     * This static method returns the current time in milliseconds.
     *
     * @returns The current time in milliseconds.
     *
     */
    static uint32_t GetNow(void) { return otAlarmGetNow(); }

private:
    void Fired(void) { mHandler(mContext); }

    Handler   mHandler;   ///< A pointer to the function that is called when the timer expires.
    void     *mContext;   ///< A pointer to arbitrary context information.
    uint32_t  mT0;        ///< The start time of the timer in milliseconds.
    uint32_t  mDt;        ///< The time delay from the start time in milliseconds.
    Timer    *mNext;      ///< The next timer in the scheduler list.
};

/**
 * @}
 *
 */

}  // namespace Thread

#endif  // TIMER_HPP_
