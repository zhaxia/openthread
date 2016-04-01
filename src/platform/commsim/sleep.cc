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
#include <platform/common/sleep.h>

#ifdef __cplusplus
extern "C" {
#endif

extern pthread_mutex_t s_mutex;
extern pthread_cond_t s_cond;

void sleep_start()
{
    pthread_cond_wait(&s_cond, &s_mutex);
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
