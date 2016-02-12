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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#include <platform/da15100/alarm.h>
#include <platform/da15100/atomic.h>
#include <common/timer.h>
#include <core/cpu.hpp>
#include <cpu/CpuTick.hpp>

using nl::Thread::CpuTick;

namespace Thread {

#define CLOCK_TO_MSEC  (CPU_DEFAULT_CLOCK_HZ/1000)

static Alarm *alarm_;
static uint32_t counter_;

class CpuAlarmTick : public CpuTick {
  int fired() {
    alarm_->InterruptHandler();
    return ESUCCESS;
  }
} theTick;

ThreadError Alarm::Init() {
  alarm_ = this;
  counter_ = 0;
  is_running_ = false;

  theTick.init(CLOCK_TO_MSEC-1);
  theTick.start(ITimer::TIMER_REPEAT);

  return kThreadError_None;
}

uint32_t Alarm::GetAlarm() const {
  return alarm_t0_ + alarm_dt_;
}

uint32_t Alarm::GetNow() {
  return counter_;
}

bool Alarm::IsRunning() const {
  return is_running_;
}

ThreadError Alarm::Start(uint32_t dt) {
  return StartAt(GetNow(), dt);
}

ThreadError Alarm::StartAt(uint32_t t0, uint32_t dt) {
  Atomic atomic;

  atomic.Begin();
  alarm_t0_ = t0;
  alarm_dt_ = dt;
  is_running_ = true;
  atomic.End();

  return kThreadError_None;
}

ThreadError Alarm::Stop() {
  is_running_ = false;
  return kThreadError_None;
}

void Alarm::InterruptHandler() {
  Atomic atomic;
  bool fire = false;

  atomic.Begin();
  counter_++;

  if (is_running_) {
    uint32_t expires = alarm_t0_ + alarm_dt_;
    if (alarm_t0_ <= counter_) {
      if (expires >= alarm_t0_ && expires <= counter_)
        fire = true;
    } else {
      if (expires >= alarm_t0_ || expires <= counter_)
        fire = true;
    }

    if (fire) {
      is_running_ = false;
      Timer::HandleAlarm();
    }
  }
  atomic.End();
}

}  // namespace Thread
