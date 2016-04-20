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
#include <common/tasklet.hpp>
#include <common/thread_error.hpp>
#include <platform/atomic.hpp>
#include <platform/sleep.hpp>

namespace Thread {

static Tasklet *sHead = NULL;
static Tasklet *sTail = NULL;

Tasklet::Tasklet(Handler handler, void *context)
{
    mHandler = handler;
    mContext = context;
    mNext = NULL;
}

ThreadError Tasklet::Post()
{
    return TaskletScheduler::Post(*this);
}

ThreadError TaskletScheduler::Post(Tasklet &tasklet)
{
    ThreadError error = kThreadError_None;
    uint32_t state = atomic_begin();

    VerifyOrExit(sTail != &tasklet && tasklet.mNext == NULL, error = kThreadError_Busy);

    if (sTail == NULL)
    {
        sHead = &tasklet;
        sTail = &tasklet;
    }
    else
    {
        sTail->mNext = &tasklet;
        sTail = &tasklet;
    }

exit:
    atomic_end(state);

    return error;
}

Tasklet *TaskletScheduler::PopTasklet()
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

bool TaskletScheduler::AreTaskletsPending()
{
    return sHead != NULL;
}

void TaskletScheduler::RunNextTasklet()
{
    uint32_t state;
    Tasklet *task;

    state = atomic_begin();
    task = PopTasklet();
    atomic_end(state);

    if (task != NULL)
    {
        task->RunTask();
    }
}

}  // namespace Thread
