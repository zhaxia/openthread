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
#include <stdlib.h>

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
