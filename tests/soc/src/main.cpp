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
#include <cli/cli_ifconfig.hpp>
#include <cli/cli_ip.hpp>
#include <cli/cli_mac.hpp>
#include <cli/cli_ping.hpp>
#include <cli/cli_netdata.hpp>
#include <cli/cli_route.hpp>
#include <cli/cli_shutdown.hpp>
#include <cli/cli_thread.hpp>
#include <platform/atomic.h>
#include <platform/posix/cli_posix.hpp>

extern "C" void sleep_start(void);
struct gengetopt_args_info args_info;

Thread::Cli::Socket sCliServer;
Thread::Cli::Ifconfig sCliIfconfig(sCliServer);
Thread::Cli::Ip sCliIp(sCliServer);
Thread::Cli::Mac sCliMac(sCliServer);
Thread::Cli::NetData sCliNetdata(sCliServer);
Thread::Cli::Ping sCliPing(sCliServer);
Thread::Cli::Route sCliRoute(sCliServer);
Thread::Cli::Shutdown sCliShutdown(sCliServer);
Thread::Cli::Thread sCliThread(sCliServer);

int main(int argc, char *argv[])
{
    uint32_t atomic_state;

    memset(&args_info, 0, sizeof(args_info));

    if (cmdline_parser(argc, argv, &args_info) != 0)
    {
        exit(1);
    }

    otInit(args_info.nodeid_arg);
    sCliServer.Start();

    while (1)
    {
        otProcessNextTasklet();

        atomic_state = ot_atomic_begin();

        if (!otAreTaskletsPending())
        {
            sleep_start();
        }

        ot_atomic_end(atomic_state);
    }

    return 0;
}
