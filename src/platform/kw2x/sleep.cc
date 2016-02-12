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

#include <platform/kw2x/sleep.h>
#include <bsp/PLM/Interface/EmbeddedTypes.h>
#include <bsp/PLM/Interface/Interrupt.h>

namespace Thread {

/**
 * This method is call by TaskletScheduler::Run from within an Atomic
 * section, so it is important that the stub version of this 
 * forcibly turn on interrupts to allow I/O to take place.
 */
void Sleep::Begin() {
  EnableInterrupts();
  DisableInterrupts();
}

void Sleep::End() {
}

}  // namespace Thread
