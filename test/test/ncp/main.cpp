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

#include <common/code_utils.h>
#include <common/message.h>
#include <common/random.h>
#include <common/tasklet.h>
#include <common/timer.h>
#include <ncp/ncp.h>
#include <platform/posix/cmdline.h>

struct gengetopt_args_info args_info;

Thread::Ncp ncp;

int main(int argc, char *argv[])
{
    memset(&args_info, 0, sizeof(args_info));

    if (cmdline_parser(argc, argv, &args_info) != 0)
    {
        exit(1);
    }

    Thread::Message::Init();
    Thread::Random::Init(args_info.eui64_arg);
    Thread::Timer::Init();

    ncp.Start();

    Thread::TaskletScheduler::Run();
    return 0;
}
