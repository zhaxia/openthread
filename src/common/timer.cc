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
#include <common/thread_error.h>
#include <common/timer.h>
#include <stdio.h>

namespace Thread {

Tasklet Timer::task_(&FireTimers, NULL);
Timer *Timer::head_ = NULL;
Timer *Timer::tail_ = NULL;
Alarm Timer::alarm_;

Timer::Timer(Callback callback, void *context) {
  callback_ = callback;
  context_ = context;
}

ThreadError Timer::StartAt(uint32_t t0, uint32_t dt) {
  t0_ = t0;
  dt_ = dt;
  return Add(this);
}

ThreadError Timer::Start(uint32_t dt) {
  return StartAt(GetNow(), dt);
}

ThreadError Timer::Stop() {
  return Remove(this);
}

bool Timer::IsRunning() const {
  return IsAdded(this);
}

uint32_t Timer::Gett0() const {
  return t0_;
}

uint32_t Timer::Getdt() const {
  return dt_;
}

uint32_t Timer::GetNow() {
  return alarm_.GetNow();
}

ThreadError Timer::Init() {
  dprintf("Timer init\n");
  return alarm_.Init();
}

ThreadError Timer::Add(Timer *timer) {
  VerifyOrExit(timer->next_ == NULL && tail_ != timer, ;);

  if (tail_ == NULL)
    head_ = timer;
  else
    tail_->next_ = timer;

  timer->next_ = NULL;
  tail_ = timer;

exit:
  SetAlarm();
  return kThreadError_None;
}

ThreadError Timer::Remove(Timer *timer) {
  VerifyOrExit(timer->next_ != NULL || tail_ == timer, ;);

  if (head_ == timer) {
    head_ = timer->next_;
    if (tail_ == timer)
      tail_ = NULL;
  } else {
    for (Timer *cur = head_; cur; cur = cur->next_) {
      if (cur->next_ == timer) {
        cur->next_ = timer->next_;
        if (tail_ == timer)
          tail_ = cur;
        break;
      }
    }
  }

  timer->next_ = NULL;
  SetAlarm();

exit:
  return kThreadError_None;
}

bool Timer::IsAdded(const Timer *timer) {
  if (head_ == timer)
    return true;

  for (Timer *cur = head_; cur; cur = cur->next_) {
    if (cur == timer)
      return true;
  }

  return false;
}

void Timer::SetAlarm() {
  uint32_t now = alarm_.GetNow();
  int32_t min_remaining = (1UL << 31) - 1;
  Timer *min_timer = NULL;

  if (head_ == NULL)
    return;

  for (Timer *timer = head_; timer; timer = timer->next_) {
    uint32_t elapsed = now - timer->t0_;
    int32_t remaining = timer->dt_ - elapsed;

    if (remaining < min_remaining) {
      min_remaining = remaining;
      min_timer = timer;
    }
  }

  if (min_timer != NULL) {
    if (min_remaining <= 0)
      task_.Post();
    else
      alarm_.StartAt(now, min_remaining);
  }
}

void Timer::HandleAlarm() {
  task_.Post();
}

void Timer::FireTimers(void *context) {
  uint32_t now = alarm_.GetNow();

  for (Timer *cur = head_; cur; cur = cur->next_) {
    uint32_t elapsed = now - cur->t0_;
    if (elapsed >= cur->dt_) {
      Remove(cur);
      cur->callback_(cur->context_);
      break;
    }
  }

  SetAlarm();
}

}  // namespace Thread
