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
 *   This file implements the tasklet scheduler.
 */

#include <common/code_utils.hpp>
#include <common/debug.hpp>
#include <common/tasklet.hpp>
#include <common/thread_error.hpp>
#include <platform/atomic.h>

namespace Thread {

static Tasklet *sHead = NULL;
static Tasklet *sTail = NULL;

Tasklet::Tasklet(Handler aHandler, void *aContext)
{
    mHandler = aHandler;
    mContext = aContext;
    mNext = NULL;
}

ThreadError Tasklet::Post(void)
{
    return TaskletScheduler::Post(*this);
}

ThreadError TaskletScheduler::Post(Tasklet &aTasklet)
{
    ThreadError error = kThreadError_None;
    uint32_t    state = otAtomicBegin();

    VerifyOrExit(sTail != &aTasklet && aTasklet.mNext == NULL, error = kThreadError_Busy);

    if (sTail == NULL)
    {
        sHead = &aTasklet;
        sTail = &aTasklet;
    }
    else
    {
        sTail->mNext = &aTasklet;
        sTail = &aTasklet;
    }

exit:
    otAtomicEnd(state);

    return error;
}

Tasklet *TaskletScheduler::PopTasklet(void)
{
    Tasklet *task = sHead;

    if (task != NULL)
    {
        sHead = sHead->mNext;

        if (sHead == NULL)
        {
            sTail = NULL;
        }

        task->mNext = NULL;
    }

    return task;
}

bool TaskletScheduler::AreTaskletsPending(void)
{
    return sHead != NULL;
}

void TaskletScheduler::RunNextTasklet(void)
{
    uint32_t  state;
    Tasklet  *task;

    state = otAtomicBegin();
    task = PopTasklet();
    otAtomicEnd(state);

    if (task != NULL)
    {
        task->RunTask();
    }
}

}  // namespace Thread
