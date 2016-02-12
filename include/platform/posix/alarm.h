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

#ifndef PLATFORM_POSIX_ALARM_H_
#define PLATFORM_POSIX_ALARM_H_

#include <platform/common/alarm_interface.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>

namespace Thread {

class Alarm: private AlarmInterface {
 public:
  ThreadError Init() final;
  uint32_t GetAlarm() const final;
  bool IsRunning() const final;
  ThreadError Start(uint32_t dt) final;
  ThreadError StartAt(uint32_t t0, uint32_t dt) final;
  ThreadError Stop() final;
  static uint32_t GetNow();

  void AlarmThread();
  static void *AlarmThread(void *arg);

 private:
  bool is_running_ = false;
  uint32_t alarm_ = 0;

  pthread_t thread_;
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond_ = PTHREAD_COND_INITIALIZER;
};

}  // namespace Thread

#endif  // PLATFORM_POSIX_ALARM_H_
