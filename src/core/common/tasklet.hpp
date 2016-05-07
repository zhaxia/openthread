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
 *   This file includes definitions for tasklets and the tasklet scheduler.
 */

#ifndef TASKLET_HPP_
#define TASKLET_HPP_

#include <openthread-types.h>

namespace Thread {

/**
 * @addtogroup core-tasklet
 *
 * @brief
 *   This module includes definitions for tasklets and the tasklet scheduler.
 *
 * @{
 *
 */

/**
 * This class is used to represent a tasklet.
 *
 */
class Tasklet
{
    friend class TaskletScheduler;

public:
    /**
     * This function pointer is called when the tasklet is run.
     *
     * @param[in]  aContext  A pointer to arbitrary context information.
     *
     */
    typedef void (*Handler)(void *aContext);

    /**
     * This constructor creates a tasklet instance.
     *
     * @param[in]  aHandler  A pointer to a function that is called when the tasklet is run.
     * @param[in]  aContext  A pointer to arbitrary context information.
     *
     */
    Tasklet(Handler aHandler, void *aContext);

    /**
     * This method puts the tasklet on the run queue.
     *
     */
    ThreadError Post(void);

private:
    /**
     * This method is called when the tasklet is run.
     *
     */
    void RunTask(void) { mHandler(mContext); }

    Handler  mHandler;  ///< A pointer to a function that is called when the tasklet is run.
    void    *mContext;  ///< A pointer to arbitrary context information.
    Tasklet *mNext;     ///< A pointer to the next tasklet in the run queue.
};

/**
 * This class implements the tasklet scheduler.
 *
 */
class TaskletScheduler
{
public:
    /**
     * This static method enqueues a tasklet into the run queue.
     *
     * @param[in]  aTasklet  A reference to the tasklet to enqueue.
     *
     * @retval kThreadError_None  Successfully enqueued the tasklet.
     * @retval kThreadError_Busy  The tasklet was already enqueued.
     */
    static ThreadError Post(Tasklet &aTasklet);

    /**
     * This static method indicates whether or not there are tasklets pending.
     *
     * @retval TRUE   If there are tasklets pending.
     * @retval FALSE  If there are no tasklets pending.
     *
     */
    static bool AreTaskletsPending(void);

    /**
     * This static method runs the next tasklet.
     *
     */
    static void RunNextTasklet(void);

private:
    static Tasklet *PopTasklet(void);
    static Tasklet *sHead;
    static Tasklet *sTail;
};

/**
 * @}
 *
 */

}  // namespace Thread

#endif  // TASKLET_HPP_
