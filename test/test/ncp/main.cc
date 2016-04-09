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

#include <common/code_utils.h>
#include <common/message.h>
#include <common/random.h>
#include <common/tasklet.h>
#include <common/timer.h>
#include <ncp/ncp.h>
#include <platform/posix/cmdline.h>
#include <stdlib.h>

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
