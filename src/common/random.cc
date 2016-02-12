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

#include <common/random.h>

namespace Thread {

static uint32_t state_ = 1;

void Random::Init(uint32_t seed) {
  state_ = seed;
}

uint32_t Random::Get() {
  uint32_t mlcg, p, q;
  uint64_t tmpstate;

  tmpstate = static_cast<uint64_t>(33614);
  tmpstate *= state_;
  q = tmpstate & 0xffffffff;
  q = q >> 1;
  p = tmpstate >> 32;
  mlcg = p + q;
  if (mlcg & 0x80000000) {
    mlcg ^= 0x7fffffff;
    mlcg++;
  }
  state_ = mlcg;

  return mlcg;
}

}  // namespace Thread
