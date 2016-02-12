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
 */

/**
 *    @file
 *    @brief
 *      Defines the interface to the Alarm object that drives Thread
 *      timers.
 *
 *    @author    Jonathan Hui <jonhui@nestlabs.com>
 */

#ifndef PLATFORM_COMMON_ALARM_INTERFACE_H_
#define PLATFORM_COMMON_ALARM_INTERFACE_H_

#include <common/thread_error.h>
#include <stdint.h>

namespace Thread {

/**
 * The interface to the Alarm object that drives Thread timers.
 */
class AlarmInterface {
 public:
  /**
   * Initialize the Alarm resources.  This will typically initilize
   * hardware resources that are used to implement the alarm.
   *
   * @retval Thread::kThreadError_None Initialization was
   * successful.
   * @retval Thread::kThreadError_Error Initialization was not
   * successful.
   */
  virtual ThreadError Init() = 0;

  /**
   * @return If an alarm is pending, the time that the alarm will
   * fire.  Otherwise, the time that the previous alarm fired.
   */
  virtual uint32_t GetAlarm() const = 0;

  /**
   * @retval true   If an alarm is pending.
   * @retval false  If an alarm is not pending.
   */
  virtual bool IsRunning() const = 0;

  /**
   * Set the alarm to fire at a time delay relative from now.
   *
   * @param[in] dt  The time delay in milliseconds.
   *
   * @retval Thread::kThreadError_None The alarm was successfully
   * started.
   * @retval Thread::kThreadError_Error The alarm was not successfully
   * started.
   */
  virtual ThreadError Start(uint32_t dt) = 0;

  /**
   * Set the alarm to fire at a time delay relative from t0.
   *
   * @param[in] t0  The reference time.
   * @param[in] dt  The time delay in milliseconds.
   *
   * @retval Thread::kThreadError_None The alarm was successfully
   * started.
   * @retval Thread::kThreadError_Error The alarm was not successfully
   * started.
   */
  virtual ThreadError StartAt(uint32_t t0, uint32_t dt) = 0;

  /**
   * Stop the alarm.
   *
   * @retval Thread::kThreadError_None The alarm was successfully
   * stopped.
   * @retval Thread::kThreadError_Error The alarm was not successfully
   * stopped.
   */
  virtual ThreadError Stop() = 0;

  /**
   * Get the current current time.
   *
   * @return The current time.
   */
  static uint32_t GetNow();
};

}  // namespace Thread

#endif  // PLATFORM_COMMON_ALARM_INTERFACE_H_
