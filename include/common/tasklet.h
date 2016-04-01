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

#ifndef TASKLET_H_
#define TASKLET_H_

#include <common/thread_error.h>

namespace Thread {

class Tasklet
{
    friend class TaskletScheduler;

public:
    typedef void (*Handler)(void *context);
    Tasklet(Handler handler, void *context);
    ThreadError Post();

private:
    void RunTask() { m_handler(m_context); }

    Handler m_handler;
    void *m_context;
    Tasklet *m_next;
};

class TaskletScheduler
{
public:
    static ThreadError Post(Tasklet &tasklet);
    static void Run();

private:
    static Tasklet *PopTasklet();
};

}  // namespace Thread

#endif  // TASKLET_H_
