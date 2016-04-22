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
 *   This file implements CLI commands for testing the timer.
 */

#include <stdlib.h>

#include <cli/cli_test.hpp>
#include <common/code_utils.hpp>
#include <common/encoding.hpp>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {
namespace Cli {

static const char kName[] = "test";

Test::Test(Server &server):
    Command(server),
    mTimer(&HandleTimer, this)
{
}

const char *Test::GetName()
{
    return kName;
}

int Test::PrintUsage(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

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
    mServer->Output(buf, cur - buf);
}

int Test::TestTimer(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    snprintf(cur, end - cur, "Test timer: start 1 sec\r\n");
    cur += strlen(cur);

    mTimer.Start(1000);

    return cur - buf;
}

void Test::Run(int argc, char *argv[], Server &server)
{
    ThreadError error = kThreadError_Error;
    char buf[512];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    mServer = &server;

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
