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
 */

/**
 *    @file
 *    @brief
 *      Defines the interface to the Atomic object that implements
 *      critical sections within the Thread stack.
 *
 *    @author    Jonathan Hui <jonhui@nestlabs.com>
 */

#ifndef PLATFORM_COMMON_ATOMIC_INTERFACE_H_
#define PLATFORM_COMMON_ATOMIC_INTERFACE_H_

#include <stdint.h>

namespace Thread {

/**
 * The interface to the Atomic object that implements critical
 * sections within the Thread stack.
 */
class AtomicInterface {
 public:
  /**
   * Begin critical section.
   */
  virtual void Begin() = 0;

  /**
   * End critical section.
   */
  virtual void End() = 0;
};

}  // namespace Thread

#endif  // PLATFORM_COMMON_ATOMIC_INTERFACE_H_
