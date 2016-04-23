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
 *   This file implements CLI commands for calling the exit() command.
 */

#include <stdlib.h>

#include <cli/cli_shutdown.hpp>
#include <common/code_utils.hpp>

namespace Thread {
namespace Cli {

static const char kName[] = "shutdown";

Shutdown::Shutdown(Server &server):
    Command(server)
{
}

const char *Shutdown::GetName()
{
    return kName;
}

int Shutdown::PrintUsage(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

    snprintf(cur, end - cur, "usage: shutdown\r\n");
    cur += strlen(cur);

    return cur - buf;
}

void Shutdown::Run(int argc, char *argv[], Server &server)
{
    char buf[128];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    if (argc > 0)
    {
        cur += PrintUsage(cur, end - cur);
    }

    snprintf(cur, end - cur, "Done\r\n");
    cur += strlen(cur);
    server.Output(buf, cur - buf);

    if (argc == 0)
    {
        exit(0);
    }
}

}  // namespace Cli
}  // namespace Thread
