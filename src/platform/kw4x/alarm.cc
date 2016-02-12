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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#include <platform/kw4x/alarm.h>
#include <platform/kw4x/atomic.h>
#include <common/timer.h>
#include <core/cpu.hpp>

#include <cpu/CpuTick.hpp>

using namespace nl::Thread;

namespace Thread {

static Alarm *alarm_;

/// LED0 
CpuGpio pinLed0(CPU_GPIO_DEFAULT_LED_PORT, CPU_GPIO_DEFAULT_LED_PIN, 1);

#define MAX_DELAY 32768
#define CLOCK_TO_MSEC  (CPU_DEFAULT_CLOCK_HZ/1028)

static uint16_t timer_hi_;
static uint16_t timer_lo_;

class CpuAlarmTick : public CpuTick {
  int fired() {
    alarm_->InterruptHandler();
    return ESUCCESS;
  }
} theTick;

ThreadError Alarm::Init()
{
  alarm_ = this;

  timer_hi_ = 0;
  timer_lo_ = 0;
  is_running_ = false;

  // Init interval to 1 [ms].
  theTick.init(CLOCK_TO_MSEC-1);
  //theTick.setCallback(MsTick_IrqHandler);
  theTick.start(ITimer::TIMER_REPEAT);

  pinLed0.init();

  return kThreadError_None;
}

uint32_t Alarm::GetAlarm() const {
  return alarm_t0_ + alarm_dt_;
}

uint32_t Alarm::GetNow() {
  return (static_cast<uint32_t>(timer_hi_) << 16) | timer_lo_;
}

uint32_t Alarm::GetNext() {
  Atomic atomic;

  atomic.Begin();

  uint16_t timer_lo = timer_lo_;
  timer_lo++;

  if (timer_lo < timer_lo_)
    timer_hi_++;
  timer_lo_ = timer_lo;

  atomic.End();

  return GetNow();
}

bool Alarm::IsRunning() const {
  return is_running_;
}

ThreadError Alarm::Start(uint32_t dt) {
  return StartAt(GetNow(), dt);
}

void Alarm::SetAlarm() {
  uint32_t now = GetNow(), remaining = MAX_DELAY;

  if (is_running_) {
    uint32_t expires = alarm_t0_ + alarm_dt_;
    remaining = expires - now;

    if (alarm_t0_ <= now) {
      if (expires >= alarm_t0_ && expires <= now)
        remaining = 0;
    } else {
      if (expires >= alarm_t0_ || expires <= now)
        remaining = 0;
    }

    if (remaining > MAX_DELAY) {
      alarm_t0_ = now + MAX_DELAY;
      alarm_dt_ = remaining - MAX_DELAY;
      remaining = MAX_DELAY;
    } else {
      alarm_t0_ += alarm_dt_;
      alarm_dt_ = 0;
    }
  }
}

ThreadError Alarm::StartAt(uint32_t t0, uint32_t dt) {
  Atomic atomic;

  atomic.Begin();
  alarm_t0_ = t0;
  alarm_dt_ = dt;
  is_running_ = true;
  SetAlarm();
  atomic.End();

  return kThreadError_None;
}

ThreadError Alarm::Stop() {
  return kThreadError_None;
}

void Alarm::InterruptHandler() {
  Atomic atomic;

  atomic.Begin();
  GetNext();

  if (is_running_ && alarm_dt_ == 0) {
    is_running_ = false;
    Timer::HandleAlarm();
    //callbacks_->Fired();
  } else {
    SetAlarm();
  }
  pinLed0.toggle();
  atomic.End();
}

}  // namespace Thread
