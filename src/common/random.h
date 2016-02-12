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

#ifndef COMMON_RANDOM_H_
#define COMMON_RANDOM_H_

#include <stdint.h>

namespace Thread {

class Random {
 public:
  static void Init(uint32_t seed);
  static uint32_t Get();
};

}  // namespace Thread

#endif  // COMMON_RANDOM_H_
