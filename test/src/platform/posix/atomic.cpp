/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <pthread.h>
#include <stdio.h>

#include <platform/atomic.hpp>
#include <common/code_utils.hpp>

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
