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

#include <cli/cli_ifconfig.h>
#include <cli/cli_ip.h>
#include <cli/cli_mac.h>
#include <cli/cli_ping.h>
#include <cli/cli_netdata.h>
#include <cli/cli_route.h>
#include <cli/cli_shutdown.h>
#include <cli/cli_thread.h>
#include <common/random.h>
#include <common/message.h>
#include <common/tasklet.h>
#include <common/timer.h>
#include <platform/posix/cli_posix.h>
#include <platform/posix/cmdline.h>
#include <thread/thread_netif.h>

struct gengetopt_args_info args_info;

Thread::ThreadNetif thread_netif;

Thread::Cli::Socket cli_server;
Thread::Cli::Ifconfig cli_ifconfig(cli_server);
Thread::Cli::Ip cli_ip(cli_server);
Thread::Cli::Mac cli_mac(cli_server, thread_netif);
Thread::Cli::NetData cli_netdata(cli_server, thread_netif);
Thread::Cli::Ping cli_ping(cli_server);
Thread::Cli::Route cli_route(cli_server);
Thread::Cli::Shutdown cli_shutdown(cli_server);
Thread::Cli::Thread cli_thread(cli_server, thread_netif);

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

    thread_netif.Init();
    cli_server.Start();

    Thread::TaskletScheduler::Run();
    return 0;
}
