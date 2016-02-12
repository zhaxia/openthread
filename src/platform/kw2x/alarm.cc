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

#include <platform/kw2x/alarm.h>
#include <platform/kw2x/atomic.h>
#include <common/timer.h>
#include <core/cpu.hpp>

namespace Thread {

#define MAX_DELAY 32768

uint16_t Alarm::timer_hi_ = 0;
uint16_t Alarm::timer_lo_ = 0;
uint32_t Alarm::alarm_t0_ = 0;
uint32_t Alarm::alarm_dt_ = 0;
bool Alarm::is_running_ = false;

ThreadError Alarm::Init() {
  /* turn on the LPTMR clock */
  SIM_SCGC5 |= SIM_SCGC5_LPTIMER_MASK;
  /* disable LPTMR */
  LPTMR0_CSR &= ~LPTMR_CSR_TEN_MASK;
  /* 1ms tick period */
  LPTMR0_PSR = (LPTMR_PSR_PBYP_MASK | LPTMR_PSR_PCS(1));
  /* enable LPTMR IRQ */
  NVICICPR1 = 1 << 26;
  NVICISER1 = 1 << 26;

  LPTMR0_CMR = MAX_DELAY;

  timer_hi_ = 0;
  timer_lo_ = 0;
  is_running_ = false;

  /* enable LPTMR */
  LPTMR0_CSR = LPTMR_CSR_TCF_MASK | LPTMR_CSR_TIE_MASK | LPTMR_CSR_TFC_MASK;
  LPTMR0_CSR |= LPTMR_CSR_TEN_MASK;

  return kThreadError_None;
}

uint32_t Alarm::GetAlarm() const {
  return alarm_t0_ + alarm_dt_;
}

uint32_t Alarm::GetNow() {
  Atomic atomic;

  atomic.Begin();

  uint16_t timer_lo;
  LPTMR0_CNR = LPTMR0_CNR;
  timer_lo = LPTMR0_CNR;

  if (timer_lo < timer_lo_)
    timer_hi_++;
  timer_lo_ = timer_lo;
  atomic.End();

  return (static_cast<uint32_t>(timer_hi_) << 16) | timer_lo_;
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

  SetHardwareAlarm(now, remaining);
}

void Alarm::SetHardwareAlarm(uint16_t t0, uint16_t dt) {
  Atomic atomic;

  atomic.Begin();
  uint16_t now, elapsed;
  LPTMR0_CNR = LPTMR0_CNR;
  now = LPTMR0_CNR;
  elapsed = now - t0;

  if (elapsed >= dt) {
    LPTMR0_CMR = now + 2;
  } else {
    uint16_t remaining = dt - elapsed;
    if (remaining <= 2)
      LPTMR0_CMR = now + 2;
    else
      LPTMR0_CMR = now + remaining;
  }
  LPTMR0_CSR |= LPTMR_CSR_TCF_MASK;
  atomic.End();
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
  if (is_running_) {
    is_running_ = false;
    SetAlarm();
  }
  return kThreadError_None;
}

extern "C" void LPTMR_IrqHandler() {
  Alarm::InterruptHandler();
}

void Alarm::InterruptHandler() {
  Atomic atomic;

  atomic.Begin();
  LPTMR0_CSR |= LPTMR_CSR_TCF_MASK;
  GetNow();

  if (is_running_ && alarm_dt_ == 0) {
    is_running_ = false;
    Timer::HandleAlarm();
  } else {
    SetAlarm();
  }

  atomic.End();
}

}  // namespace Thread
