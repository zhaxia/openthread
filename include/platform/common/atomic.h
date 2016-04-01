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

#ifndef ATOMIC_H_
#define ATOMIC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Begin critical section.
 */
uint32_t atomic_begin();

/**
 * End critical section.
 */
void atomic_end(uint32_t state);

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // ATOMIC_H_
