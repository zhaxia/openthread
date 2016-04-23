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
 *   This file implements CLI commands for configuring and managing Thread protocols.
 */

#include <stdlib.h>

#include <openthread.h>
#include <cli/cli.hpp>
#include <cli/cli_thread.hpp>
#include <common/code_utils.hpp>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {
namespace Cli {

static const char kName[] = "thread";

Thread::Thread(Server &server):
    Command(server)
{
}

const char *Thread::GetName()
{
    return kName;
}

int Thread::PrintUsage(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

    snprintf(cur, end - cur,
             "usage: thread\r\n"
             "  key <key>\r\n"
             "  key_sequence [sequence]\r\n"
             "  leader_data\r\n"
             "  mode [rsdn]\r\n"
             "  network_id_timeout [timeout]\r\n"
             "  release_router [router_id]\r\n"
             "  router_uprade_threshold [threshold]\r\n"
             "  start\r\n"
             "  state [detached|child|router|leader]\r\n"
             "  stop\r\n"
             "  timeout [timeout]\r\n"
             "  weight [weight]\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintKey(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    const uint8_t *key;
    uint8_t keyLength;
    key = otGetMasterKey(&keyLength);

    for (int i = 0; i < keyLength; i++)
    {
        snprintf(cur, end - cur, "%02x", key[i]);
        cur += strlen(cur);
    }

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintKeySequence(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    snprintf(cur, end - cur, "%u\r\n", otGetKeySequenceCounter());
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintLeaderData(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    snprintf(cur, end - cur, "partition_id = %08" PRIx32 "\r\n", otGetPartitionId());
    cur += strlen(cur);
    snprintf(cur, end - cur, "weighting = %d\r\n", otGetLeaderWeight());
    cur += strlen(cur);
    snprintf(cur, end - cur, "version = %d\r\n", otGetNetworkDataVersion());
    cur += strlen(cur);
    snprintf(cur, end - cur, "stable_version = %d\r\n", otGetStableNetworkDataVersion());
    cur += strlen(cur);
    snprintf(cur, end - cur, "leader_router_id = %d\r\n", otGetLeaderRouterId());
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintMode(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    otLinkModeConfig linkMode = otGetLinkMode();

    if (linkMode.mRxOnWhenIdle)
    {
        *cur++ = 'r';
    }

    if (linkMode.mSecureDataRequests)
    {
        *cur++ = 's';
    }

    if (linkMode.mDeviceType)
    {
        *cur++ = 'd';
    }

    if (linkMode.mNetworkData)
    {
        *cur++ = 'n';
    }

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintState(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    switch (otGetDeviceRole())
    {
    case kDeviceRoleDisabled:
        snprintf(cur, end - cur, "disabled\r\n");
        break;

    case kDeviceRoleDetached:
        snprintf(cur, end - cur, "detached\r\n");
        break;

    case kDeviceRoleChild:
        snprintf(cur, end - cur, "child\r\n");
        break;

    case kDeviceRoleRouter:
        snprintf(cur, end - cur, "router\r\n");
        break;

    case kDeviceRoleLeader:
        snprintf(cur, end - cur, "leader\r\n");
        break;
    }

    cur += strlen(cur);

    return cur - buf;
}

void Thread::Run(int argc, char *argv[], Server &server)
{
    ThreadError error = kThreadError_InvalidArgs;
    char buf[512];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    uint32_t val32;
    uint8_t val8;

    uint8_t key[16];
    int keyLength;
    otLinkModeConfig linkMode = {};

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            ExitNow();
        }
        else if (strcmp(argv[i], "key") == 0)
        {
            if (++i >= argc)
            {
                cur += PrintKey(cur, sizeof(buf));
                ExitNow(error = kThreadError_None);
            }

            if ((keyLength = hex2bin(argv[i], key, sizeof(key))) < 0)
            {
                ExitNow();
            }

            ExitNow(error = otSetMasterKey(key, keyLength));
        }
        else if (strcmp(argv[i], "key_sequence") == 0)
        {
            if (++i >= argc)
            {
                cur += PrintKeySequence(cur, sizeof(buf));
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                otSetKeySequenceCounter(val32);
            }
        }
        else if (strcmp(argv[i], "leader_data") == 0)
        {
            cur += PrintLeaderData(buf, sizeof(buf));
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "mode") == 0)
        {
            if (++i >= argc)
            {
                cur += PrintMode(cur, sizeof(buf));
                ExitNow(error = kThreadError_None);
            }

            for (char *arg = argv[i]; *arg != '\0'; arg++)
            {
                switch (*arg)
                {
                case 'r':
                    linkMode.mRxOnWhenIdle = 1;
                    break;

                case 's':
                    linkMode.mSecureDataRequests = 1;
                    break;

                case 'd':
                    linkMode.mDeviceType = 1;
                    break;

                case 'n':
                    linkMode.mNetworkData = 1;
                    break;

                default:
                    ExitNow();
                }
            }

            ExitNow(error = otSetLinkMode(linkMode));
        }
        else if (strcmp(argv[i], "network_id_timeout") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", otGetNetworkIdTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                otSetNetworkIdTimeout(val32);
                ExitNow(error = kThreadError_None);
            }
        }
        else if (strcmp(argv[i], "release_router") == 0)
        {
            if (++i >= argc)
            {
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val8 = strtol(argv[i], NULL, 0);
                ExitNow(error = otReleaseRouterId(val8));
            }
        }
        else if (strcmp(argv[i], "router_upgrade_threshold") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", otGetNetworkIdTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                otSetRouterUpgradeThreshold(val32);
                ExitNow(error = kThreadError_None);
            }
        }
        else if (strcmp(argv[i], "start") == 0)
        {
            ExitNow(error = otEnable());
        }
        else if (strcmp(argv[i], "state") == 0)
        {
            if (++i >= argc)
            {
                cur += PrintState(cur, sizeof(buf));
                ExitNow(error = kThreadError_None);
            }
            else if (strcmp(argv[i], "detached") == 0)
            {
                ExitNow(error = otBecomeDetached());
            }
            else if (strcmp(argv[i], "child") == 0)
            {
                ExitNow(error = otBecomeChild(kMleAttachSamePartition));
            }
            else if (strcmp(argv[i], "router") == 0)
            {
                ExitNow(error = otBecomeRouter());
            }
            else if (strcmp(argv[i], "leader") == 0)
            {
                ExitNow(error = otBecomeLeader());
            }
            else
            {
                ExitNow();
            }
        }
        else if (strcmp(argv[i], "stop") == 0)
        {
            ExitNow(error = otDisable());
        }
        else if (strcmp(argv[i], "timeout") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%u\n", otGetChildTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                otSetChildTimeout(val32);
                ExitNow(error = kThreadError_None);
            }
        }
        else if (strcmp(argv[i], "weight") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", otGetLocalLeaderWeight());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val8 = strtol(argv[i], NULL, 0);
                otSetLocalLeaderWeight(val8);
                ExitNow(error = kThreadError_None);
            }
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
