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

#include <platform/common/atomic.h>
#include <platform/posix/alarm.h>
#include <common/code_utils.h>
#include <common/timer.h>
#include <common/thread_error.h>
#include <stdio.h>
#include <string.h>

namespace Thread {

static timeval start_;

ThreadError Alarm::Init() {
  gettimeofday(&start_, NULL);
  pthread_create(&thread_, NULL, AlarmThread, this);
  return kThreadError_None;
}

uint32_t Alarm::GetAlarm() const {
  return alarm_;
}

uint32_t Alarm::GetNow() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  timersub(&tv, &start_, &tv);

  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

bool Alarm::IsRunning() const {
  return is_running_;
}

ThreadError Alarm::Start(uint32_t dt) {
  return StartAt(GetNow(), dt);
}

ThreadError Alarm::StartAt(uint32_t t0, uint32_t dt) {
  pthread_mutex_lock(&mutex_);
  alarm_ = t0 + dt;
  is_running_ = true;
  pthread_mutex_unlock(&mutex_);
  pthread_cond_signal(&cond_);

  return kThreadError_None;
}

ThreadError Alarm::Stop() {
  pthread_mutex_lock(&mutex_);
  is_running_ = false;
  pthread_mutex_unlock(&mutex_);
  return kThreadError_None;
}

void *Alarm::AlarmThread(void *arg) {
  Alarm *alarm = reinterpret_cast<Alarm*>(arg);
  alarm->AlarmThread();
  return NULL;
}

void Alarm::AlarmThread() {
  while (1) {
    pthread_mutex_lock(&mutex_);

    if (!is_running_) {
      // alarm is not running, wait indefinitely
      pthread_cond_wait(&cond_, &mutex_);
      pthread_mutex_unlock(&mutex_);
    } else {
      // alarm is running
      int32_t remaining = alarm_ - GetNow();

      if (remaining > 0) {
        // alarm has not passed, wait
        struct timeval tva;
        struct timeval tvb;
        struct timespec ts;

        gettimeofday(&tva, NULL);
        tvb.tv_sec = remaining / 1000;
        tvb.tv_usec = (remaining % 1000) * 1000;
        timeradd(&tva, &tvb, &tva);

        ts.tv_sec = tva.tv_sec;
        ts.tv_nsec = tva.tv_usec * 1000;

        pthread_cond_timedwait(&cond_, &mutex_, &ts);
        pthread_mutex_unlock(&mutex_);
      } else {
        // alarm has passed, signal
        is_running_ = false;
        pthread_mutex_unlock(&mutex_);
        Timer::HandleAlarm();
      }
    }
  }
}

}  // namespace Thread
