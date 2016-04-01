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

#ifndef CODE_UTILS_H_
#define CODE_UTILS_H_

#include <common/debug.h>

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

#endif  // CODE_UTILS_H_
