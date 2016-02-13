/*
 *
 *    Copyright (c) 2016 Nest Labs, Inc.
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

#include <common/message.h>
#include <common/random.h>
#include <common/tasklet.h>
#include <common/timer.h>

using namespace Thread;

void openthread_init(void)
{
    Message::Init();
//    Random::Init(args_info.eui64_arg);
    Random::Init(1);
    Timer::Init();

    TaskletScheduler::Run();
}

