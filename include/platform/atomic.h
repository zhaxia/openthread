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
 *   This file includes the platform abstraction to support critical sections.
 */

#ifndef ATOMIC_H_
#define ATOMIC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup atomic Atomic
 * @ingroup platform
 *
 * @brief
 *   This module includes the platform abstraction to support critical sections.
 *
 * @{
 *
 */

/**
 * Begin critical section.
 */
uint32_t otAtomicBegin();

/**
 * End critical section.
 */
void otAtomicEnd(uint32_t state);

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // ATOMIC_H_
