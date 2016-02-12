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

#include <platform/da15100/atomic.h>

namespace Thread {

void Atomic::Begin() { state_ = CpuIrq::critical_enter(); }
void Atomic::End()   { CpuIrq::critical_exit(state_); }

}  // namespace Thread
