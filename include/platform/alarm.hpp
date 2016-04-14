/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *    @brief
 *      Defines the interface to the Alarm object that drives Thread
 *      timers.
 */

#ifndef ALARM_HPP_
#define ALARM_HPP_

#include <stdint.h>

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Alarm resources.  This will typically initilize
 * hardware resources that are used to implement the alarm.
 */
void alarm_init();

/**
 * Set the alarm to fire at a time delay relative from t0.
 *
 * @param[in] t0  The reference time.
 * @param[in] dt  The time delay in milliseconds.
 */
void alarm_start_at(uint32_t t0, uint32_t dt);

/**
 * Stop the alarm.
 */
void alarm_stop();

/**
 * Get the current current time.
 *
 * @return The current time.
 */
uint32_t alarm_get_now();

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread

#endif  // ALARM_HPP_
