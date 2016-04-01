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
#include <common/tasklet.h>
#include <common/thread_error.h>
#include <platform/common/atomic.h>
#include <platform/common/sleep.h>

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
