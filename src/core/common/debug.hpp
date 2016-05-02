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
 *   This file includes functions for debugging.
 */

#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#if defined(OPENTHREAD_TARGET_DARWIN) || defined(OPENTHREAD_TARGET_LINUX)

#include <assert.h>

#else

#define assert(cond)                            \
  do {                                          \
    if (!(cond)) {                              \
      while (1) {}                              \
    }                                           \
  } while (0)

#endif

#endif  // DEBUG_HPP_
