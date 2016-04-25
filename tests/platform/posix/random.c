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

/**
 * @file
 *   This file implements a pseudo-random number generator.
 *
 * @warning
 *   This implementation is not a true random number generator and does @em satisfy the Thread requirements.
 */

#include <platform/random.h>
#include <platform/posix/cmdline.h>

extern struct gengetopt_args_info args_info;

static uint32_t sState = 1;

void ot_random_init()
{
    sState = args_info.nodeid_arg;
}

uint32_t ot_random_get()
{
    uint32_t mlcg, p, q;
    uint64_t tmpstate;

    tmpstate = (uint64_t)33614 * (uint64_t)sState;
    q = tmpstate & 0xffffffff;
    q = q >> 1;
    p = tmpstate >> 32;
    mlcg = p + q;

    if (mlcg & 0x80000000)
    {
        mlcg &= 0x7fffffff;
        mlcg++;
    }

    sState = mlcg;

    return mlcg;
}
