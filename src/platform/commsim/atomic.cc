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

#include <platform/posix/alarm.h>
#include <platform/posix/atomic.h>
#include <common/code_utils.h>
#include <pthread.h>
#include <stdio.h>

namespace Thread {

static pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_ = PTHREAD_COND_INITIALIZER;

void Atomic::Begin() {
  pthread_mutex_lock(&mutex_);
}

void Atomic::End() {
  pthread_mutex_unlock(&mutex_);
  pthread_cond_signal(&cond_);
}

void Atomic::TimedWait() {
  pthread_cond_wait(&cond_, &mutex_);
}

}  // namespace Thread
