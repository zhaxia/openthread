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

#ifndef CODE_UTILS_HPP_
#define CODE_UTILS_HPP_

#include <common/debug.hpp>

namespace Thread {

#define SuccessOrExit(ERR)                      \
  do {                                          \
    if ((ERR) != 0) {                           \
      goto exit;                                \
    }                                           \
  } while (false)

#define VerifyOrExit(COND, ACTION) \
  do {                             \
    if (!(COND)) {                 \
      ACTION;                      \
      goto exit;                   \
    }                              \
  } while (false)

#define ExitNow(...)                            \
  do {                                          \
    __VA_ARGS__;                                \
    goto exit;                                  \
  } while (false)

}  // namespace Thread

#endif  // CODE_UTILS_HPP_
