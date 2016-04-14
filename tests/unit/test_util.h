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

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <common/thread_error.hpp>
#include <stdio.h>
#include <stdlib.h>

#define SuccessOrQuit(ERR, MSG)                 \
  do { \
    if ((ERR) != kThreadError_None)     \
    { \
      fprintf(stderr, "%s FAILED: ", __FUNCTION__); \
      fputs(MSG, stderr); \
      exit(-1); \
    } \
  } while (0)

#define VerifyOrQuit(TST, MSG) \
  do { \
    if (!(TST)) \
    { \
      fprintf(stderr, "%s FAILED: ", __FUNCTION__); \
      fputs(MSG, stderr); \
      exit(-1); \
    } \
  } while (0)

#endif
