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

#ifndef COMMON_TASKLET_H_
#define COMMON_TASKLET_H_

#include <common/thread_error.h>
#include <pthread.h>

namespace Thread {

class Tasklet {
  friend class TaskletScheduler;

 public:
  typedef void (*Callback)(void *context);
  Tasklet(Callback callback, void *context);
  ThreadError Post();

 private:
  Callback m_callback;
  void *m_context;
  Tasklet *m_next;
};

class TaskletScheduler {
 public:
  static ThreadError Post(Tasklet *tasklet);
  static void Run();

 private:
  static Tasklet *PopTasklet();
  static Tasklet *m_head;
  static Tasklet *m_tail;
};

}  // namespace Thread

#endif  // COMMON_TASKLET_H_
