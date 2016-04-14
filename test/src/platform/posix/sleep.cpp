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

#include <platform/sleep.hpp>

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
