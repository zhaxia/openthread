/**
 * Internal shell tests for assessing platform level abstractions are working:
 *   timer, ...
 *
 *    @file    cli_test.cc
 *    @author  Martin Turon <mturon@nestlabs.com>
 *    @date    August 6, 2015
 *
 *
 *    Copyright (c) 2015 Nest Labs, Inc.  All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 */

#include <cli/cli_test.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {
namespace Cli {

static const char kName[] = "test";

Test::Test(Server &server):
    Command(server),
    m_timer(&HandleTimer, this)
{
}

const char *Test::GetName()
{
    return kName;
}

int Test::PrintUsage(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = cur + buf_length;

    snprintf(cur, end - cur,
             "usage: test\r\n"
             "  timer  - triggers a 1 sec timer\r\n"
            );
    cur += strlen(cur);

    return cur - buf;
}

/**
 * Triggered by the cli command and then outputs to
 * the shell server 1 second later.
 */
void Test::HandleTimer(void *context)
{
    Test *obj = reinterpret_cast<Test *>(context);
    obj->HandleTimer();
}

void Test::HandleTimer()
{
    char buf[256];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    snprintf(cur, end - cur, "Test timer: fired!\r\n");
    cur += strlen(cur);
    m_server->Output(buf, cur - buf);
}

int Test::TestTimer(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    snprintf(cur, end - cur, "Test timer: start 1 sec\r\n");
    cur += strlen(cur);

    m_timer.Start(1000);

    return cur - buf;
}

void Test::Run(int argc, char *argv[], Server &server)
{
    ThreadError error = kThreadError_Error;
    char buf[512];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    m_server = &server;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            ExitNow();

        }
        else if (strcmp(argv[i], "timer") == 0)
        {
            cur += TestTimer(buf, sizeof(buf));
            ExitNow(error = kThreadError_None);
        }
    }

exit:

    if (error != kThreadError_None)
    {
        cur += PrintUsage(cur, end - cur);
    }

    snprintf(cur, end - cur, "Done\r\n");
    cur += strlen(cur);
    server.Output(buf, cur - buf);
}

}  // namespace Cli
}  // namespace Thread
