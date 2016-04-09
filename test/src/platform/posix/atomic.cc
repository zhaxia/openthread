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

#include <pthread.h>
#include <stdio.h>
#include <platform/common/atomic.h>
#include <common/code_utils.h>

#ifdef __cplusplus
extern "C" {
#endif

pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_cond = PTHREAD_COND_INITIALIZER;

uint32_t atomic_begin()
{
    pthread_mutex_lock(&s_mutex);
    return 0;
}

void atomic_end(uint32_t state)
{
    pthread_mutex_unlock(&s_mutex);
    pthread_cond_signal(&s_cond);
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
