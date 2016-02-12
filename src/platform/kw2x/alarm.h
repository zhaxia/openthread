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

#ifndef PLATFORM_KW2X_ALARM_H_
#define PLATFORM_KW2X_ALARM_H_

#include <platform/common/alarm_interface.h>
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
  static void InterruptHandler();

 protected:
  static void SetAlarm();
  static void SetHardwareAlarm(uint16_t t0, uint16_t dt);

  static uint16_t timer_hi_;
  static uint16_t timer_lo_;
  static uint32_t alarm_t0_;
  static uint32_t alarm_dt_;
  static bool is_running_;
};

}  // namespace Thread

#endif  // PLATFORM_KW2X_ALARM_H_
