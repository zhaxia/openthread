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
 * @file
 * @brief
 *   This file includes the platform abstraction for the alarm service.
 */

#ifndef ALARM_H_
#define ALARM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup alarm Alarm
 * @ingroup platform
 *
 * @brief
 *   This module includes the platform abstraction for the alarm service.
 *
 * @{
 *
 */

/**
 * Initialize the Alarm.
 */
void ot_alarm_init(void);

/**
 * Set the alarm to fire at @p aDt milliseconds after @p aT0.
 *
 * @param[in] aT0  The reference time.
 * @param[in] aDt  The time delay in milliseconds from @p aT0.
 */
void ot_alarm_start_at(uint32_t aT0, uint32_t aDt);

/**
 * Stop the alarm.
 */
void ot_alarm_stop(void);

/**
 * Get the current current time.
 *
 * @returns The current time in milliseconds.
 */
uint32_t ot_alarm_get_now(void);

/**
 * Signal that the alarm has fired.
 */
extern void ot_alarm_signal_fired(void);

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ALARM_H_
