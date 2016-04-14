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

static Tasklet *s_head = NULL;
static Tasklet *s_tail = NULL;

Tasklet::Tasklet(Handler handler, void *context)
{
    m_handler = handler;
    m_context = context;
    m_next = NULL;
}

ThreadError Tasklet::Post()
{
    return TaskletScheduler::Post(*this);
}

ThreadError TaskletScheduler::Post(Tasklet &tasklet)
{
    ThreadError error = kThreadError_None;
    uint32_t state = atomic_begin();

    VerifyOrExit(s_tail != &tasklet && tasklet.m_next == NULL, error = kThreadError_Busy);

    if (s_tail == NULL)
    {
        s_head = &tasklet;
        s_tail = &tasklet;
    }
    else
    {
        s_tail->m_next = &tasklet;
        s_tail = &tasklet;
    }

exit:
    atomic_end(state);

    return error;
}

Tasklet *TaskletScheduler::PopTasklet()
{
    Tasklet *task = s_head;

    if (task != NULL)
    {
        s_head = s_head->m_next;

        if (s_head == NULL)
        {
            s_tail = NULL;
        }

        task->m_next = NULL;
    }

    return task;
}

void TaskletScheduler::Run()
{
    uint32_t state;
    dprintf("Tasklet Scheduler Run\n");

    for (;;)
    {
        state = atomic_begin();

        Tasklet *task;

        while ((task = TaskletScheduler::PopTasklet()) == NULL)
        {
            sleep_start();
        }

        atomic_end(state);
        task->RunTask();
    }
}

}  // namespace Thread
