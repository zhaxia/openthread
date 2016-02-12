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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#ifndef PLATFORM_KW4X_ATOMIC_H_
#define PLATFORM_KW4X_ATOMIC_H_

#include <core/cpu.hpp>
#include <platform/common/atomic_interface.h>

namespace Thread {

class Atomic : public AtomicInterface {
 public:
  void Begin() final;
  void End() final;

 private:
  uint32_t state_;
};

}  // namespace Thread

#endif  // PLATFORM_KW4X_ATOMIC_H_
