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

#include <platform/common/sleep.h>
#include <core/cpu.h>

#ifdef __cplusplus
extern "C" {
#endif

void sleep_start()
{
    __enable_irq();
    __WFI();
    __disable_irq();
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
