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

#include <platform/kw2x/atomic.h>
#include <bsp/PLM/Interface/EmbeddedTypes.h>
#include <bsp/PLM/Interface/Interrupt.h>

namespace Thread {

void Atomic::Begin() {
  state_ = IntDisableAll();
  // asm volatile("MRS %0,PRIMASK" : "=r" (state_));
  // asm volatile("CPSID i");
}

void Atomic::End() {
  IntRestoreAll(state_);
  // asm volatile("MSR PRIMASK, %0" : : "r" (state_));
}

}  // namespace Thread
