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

#include <stdlib.h>

#include <platform/posix/cmdline.h>

#include <openthread.h>
#include <platform/atomic.h>
#include <platform/posix/cli_posix.hpp>
#include <platform.h>

extern "C" void SleepStart(void);
struct gengetopt_args_info args_info;

Thread::Cli::Socket sCliServer;

int main(int argc, char *argv[])
{
    uint32_t atomic_state;

    memset(&args_info, 0, sizeof(args_info));

    if (cmdline_parser(argc, argv, &args_info) != 0)
    {
        exit(1);
    }

    hwAlarmInit();
    hwRadioInit();
    hwRandomInit();

    otInit();
    sCliServer.Start();

    while (1)
    {
        otProcessNextTasklet();

        atomic_state = otAtomicBegin();

        if (!otAreTaskletsPending())
        {
            SleepStart();
        }

        otAtomicEnd(atomic_state);
    }

    return 0;
}
