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
 *   This file includes the platform-specific initializers.
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This method initializes the alarm service used by OpenThread.
 *
 */
void hwAlarmInit(void);

/**
 * This method initializes the radio service used by OpenThread.
 *
 */
void hwRadioInit(void);

/**
 * This method initializes the random number service used by OpenThread.
 *
 */
void hwRandomInit(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PLATFORM_H_
