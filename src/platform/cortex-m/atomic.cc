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

#include <platform/common/atomic.h>
#include <core/cpu.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t atomic_begin()
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

void atomic_end(uint32_t state)
{
    __set_PRIMASK(state);
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
