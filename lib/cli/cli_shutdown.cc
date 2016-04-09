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

#include <cli/cli_shutdown.h>
#include <common/code_utils.h>
#include <stdlib.h>

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

int Shutdown::PrintUsage(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = cur + buf_length;

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
