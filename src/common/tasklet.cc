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
#include <stdio.h>
#include <sys/time.h>

namespace Thread {

Tasklet *TaskletScheduler::m_head = NULL;
Tasklet *TaskletScheduler::m_tail = NULL;

Tasklet::Tasklet(Callback callback, void *context) {
  m_callback = callback;
  m_context = context;
  m_next = NULL;
}

ThreadError Tasklet::Post() {
  return TaskletScheduler::Post(this);
}

ThreadError TaskletScheduler::Post(Tasklet *tasklet) {
  ThreadError error = kThreadError_None;

  Atomic atomic;
  atomic.Begin();

  VerifyOrExit(m_tail != tasklet && tasklet->m_next == NULL, error = kThreadError_Busy);
  if (m_tail == NULL) {
    m_head = tasklet;
    m_tail = tasklet;
  } else {
    m_tail->m_next = tasklet;
    m_tail = tasklet;
  }

exit:
  atomic.End();

  return error;
}

Tasklet *TaskletScheduler::PopTasklet() {
  Tasklet *task = m_head;
  if (task != NULL) {
    m_head = m_head->m_next;
    if (m_head == NULL)
      m_tail = NULL;
    task->m_next = NULL;
  }
  return task;
}

void TaskletScheduler::Run() {
  dprintf("Tasklet Scheduler Run\n");

  for (;;) {
    Atomic atomic;
    atomic.Begin();

    Tasklet *task;
    while ((task = TaskletScheduler::PopTasklet()) == NULL)
      Sleep::Begin();

    atomic.End();
    task->m_callback(task->m_context);
  }
}

}  // namespace Thread
