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
 *   This file includes the platform abstraction for true random number generation.
 */

#ifndef RANDOM_H_
#define RANDOM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup random Random
 * @ingroup platform
 *
 * @brief
 *   This module includes the platform abstraction to support critical sections.
 *
 * @{
 *
 */

/**
 * Initialize the true random number generator.
 *
 */
void otRandomInit(void);

/**
 * Get a 32-bit true random value.
 *
 * @returns A 32-bit true random value.
 *
 */
uint32_t otRandomGet(void);

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // RANDOM_H_
