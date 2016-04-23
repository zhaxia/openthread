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
 *   This file implements the CLI commands for configuring and managing Thread Network Data.
 */

#include <stdlib.h>

#include <cli/cli_netdata.hpp>
#include <common/code_utils.hpp>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {
namespace Cli {

static const char kName[] = "netdata";

NetData::NetData(Server &server):
    Command(server)
{
}

const char *NetData::GetName()
{
    return kName;
}

int NetData::PrintUsage(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

    snprintf(cur, end - cur,
             "usage: netdata\r\n"
             "  prefix add <prefix>/<plen> <domain> [pvdcsr] [prf]\r\n"
             "  prefix remove <prefix>/<plen>\r\n"
             "  route add <prefix>/<plen> [s] [prf]\r\n"
             "  route remove <prefix>/<plen>\r\n"
             "  context_reuse_delay [delay]\r\n"
             "  register\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int NetData::PrintContextIdReuseDelay(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    snprintf(cur, end - cur, "%u\n", otGetContextIdReuseDelay());
    cur += strlen(cur);

    return cur - buf;
}

int NetData::AddHasRoutePrefix(int argc, char *argv[], char *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_Error;
    char *cur = buf;
    char *end = cur + bufLength;
    int argcur = 0;

    otExternalRouteConfig config = {};

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    if (otIp6AddressFromString(argv[argcur], &config.mPrefix.mPrefix) != kThreadError_None)
    {
        ExitNow();
    }

    config.mPrefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow();
    }

    if (++argcur < argc)
    {
        if (strcmp(argv[argcur], "s") == 0)
        {
            config.mStable = true;
        }
        else if (strcmp(argv[argcur], "high") == 0)
        {
            config.mPreference = 1;
        }
        else if (strcmp(argv[argcur], "med") == 0)
        {
            config.mPreference = 0;
        }
        else if (strcmp(argv[argcur], "low") == 0)
        {
            config.mPreference = -1;
        }
        else
        {
            ExitNow();
        }
    }

    SuccessOrExit(error = otAddExternalRoute(&config));

exit:

    if (error != kThreadError_None)
    {
        cur += PrintUsage(cur, end - cur);
    }

    return cur - buf;
}

int NetData::RemoveHasRoutePrefix(int argc, char *argv[], char *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_Error;
    char *cur = buf;
    char *end = cur + bufLength;
    int argcur = 0;

    struct otIp6Prefix prefix = {};

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    if (otIp6AddressFromString(argv[argcur], &prefix.mPrefix) != kThreadError_None)
    {
        ExitNow();
    }

    prefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow();
    }

    SuccessOrExit(error = otRemoveExternalRoute(&prefix));

exit:

    if (error != kThreadError_None)
    {
        cur += PrintUsage(cur, end - cur);
    }

    return cur - buf;
}

int NetData::AddOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_Error;
    char *cur = buf;
    char *end = cur + bufLength;
    int argcur = 0;

    otBorderRouterConfig config = {};

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    if (otIp6AddressFromString(argv[argcur], &config.mPrefix.mPrefix) != kThreadError_None)
    {
        ExitNow();
    }

    config.mPrefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow();
    }

    if (++argcur < argc)
    {
        for (char *arg = argv[argcur]; *arg != '\0'; arg++)
        {
            switch (*arg)
            {
            case 'p':
                config.mSlaacPreferred = true;
                break;

            case 'v':
                config.mSlaacValid = true;
                break;

            case 'd':
                config.mDhcp = true;
                break;

            case 'c':
                config.mConfigure = true;
                break;

            case 'r':
                config.mDefaultRoute = true;
                break;

            case 's':
                config.mStable = true;
                break;

            default:
                ExitNow();
            }
        }
    }

    if (++argcur < argc)
    {
        if (strcmp(argv[argcur], "high") == 0)
        {
            config.mPreference = 1;
        }
        else if (strcmp(argv[argcur], "med") == 0)
        {
            config.mPreference = 0;
        }
        else if (strcmp(argv[argcur], "low") == 0)
        {
            config.mPreference = -1;
        }
        else
        {
            ExitNow();
        }
    }

    SuccessOrExit(error = otAddBorderRouter(&config));

exit:

    if (error != kThreadError_None)
    {
        cur += PrintUsage(cur, end - cur);
    }

    return cur - buf;
}

int NetData::RemoveOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_Error;
    char *cur = buf;
    char *end = cur + bufLength;
    int argcur = 0;

    struct otIp6Prefix prefix = {};

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    if (otIp6AddressFromString(argv[argcur], &prefix.mPrefix) != kThreadError_None)
    {
        ExitNow();
    }

    prefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow();
    }

    SuccessOrExit(error = otRemoveBorderRouter(&prefix));

exit:

    if (error != kThreadError_None)
    {
        cur += PrintUsage(cur, end - cur);
    }

    return cur - buf;
}

void NetData::Run(int argc, char *argv[], Server &server)
{
    ThreadError error = kThreadError_InvalidArgs;
    char buf[512];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    uint32_t delay;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            ExitNow();
        }
        else if (strcmp(argv[i], "prefix") == 0)
        {
            VerifyOrExit(++i < argc, ;);

            if (strcmp(argv[i], "add") == 0)
            {
                i++;
                cur += AddOnMeshPrefix(argc - i, &argv[i], cur, end - cur);
                ExitNow(error = kThreadError_None);
            }
            else if (strcmp(argv[i], "remove") == 0)
            {
                i++;
                cur += RemoveOnMeshPrefix(argc - i, &argv[i], cur, end - cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                ExitNow();
            }
        }
        else if (strcmp(argv[i], "route") == 0)
        {
            VerifyOrExit(++i < argc, ;);

            if (strcmp(argv[i], "add") == 0)
            {
                i++;
                cur += AddHasRoutePrefix(argc - i, &argv[i], cur, end - cur);
                ExitNow(error = kThreadError_None);
            }
            else if (strcmp(argv[i], "remove") == 0)
            {
                i++;
                cur += RemoveHasRoutePrefix(argc - i, &argv[i], cur, end - cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                ExitNow();
            }
        }
        else if (strcmp(argv[i], "context_reuse_delay") == 0)
        {
            if (++i >= argc)
            {
                cur += PrintContextIdReuseDelay(buf, sizeof(buf));
            }
            else
            {
                delay = strtol(argv[i], NULL, 0);
                otSetContextIdReuseDelay(delay);
            }

            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "register") == 0)
        {
            ExitNow(error = otSendServerData());
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
