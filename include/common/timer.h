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

#ifndef COMMON_TIMER_H_
#define COMMON_TIMER_H_

#include <common/tasklet.h>
#include <common/thread_error.h>
#include <platform/common/alarm.h>
#include <stdint.h>

namespace Thread {

class Timer {
 public:
  typedef void (*Callback)(void *context);
  Timer(Callback callback, void *context);

  uint32_t Gett0() const;
  uint32_t Getdt() const;
  bool IsRunning() const;
  ThreadError Start(uint32_t dt);
  ThreadError StartAt(uint32_t t0, uint32_t dt);
  ThreadError Stop();

  static ThreadError Init();
  static uint32_t GetNow();
  static void HandleAlarm();

 private:
  static ThreadError Add(Timer *timer);
  static ThreadError Remove(Timer *timer);
  static bool IsAdded(const Timer *timer);
  static void SetAlarm();
  static void FireTimers(void *context);

  static Tasklet task_;
  static Timer *head_;
  static Timer *tail_;
  static Alarm alarm_;

  Callback callback_;
  void *context_;
  uint32_t t0_ = 0;
  uint32_t dt_ = 0;
  Timer *next_ = NULL;
};

}  // namespace Thread

#endif  // COMMON_TIMER_H_
