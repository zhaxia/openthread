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

#include <common/random.h>

namespace Thread {

static uint32_t s_state = 1;

void Random::Init(uint32_t seed)
{
    s_state = seed;
}

uint32_t Random::Get()
{
    uint32_t mlcg, p, q;
    uint64_t tmpstate;

    tmpstate = static_cast<uint64_t>(33614) * static_cast<uint64_t>(s_state);
    q = tmpstate & 0xffffffff;
    q = q >> 1;
    p = tmpstate >> 32;
    mlcg = p + q;

    if (mlcg & 0x80000000)
    {
        mlcg &= 0x7fffffff;
        mlcg++;
    }

    s_state = mlcg;

    return mlcg;
}

}  // namespace Thread
