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

#include <cli/cli.hpp>
#include <cli/cli_thread.hpp>
#include <common/code_utils.hpp>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {
namespace Cli {

static const char kName[] = "thread";

Thread::Thread(Server &server, ThreadNetif &netif):
    Command(server)
{
    mNetif = &netif;
    mMle = netif.GetMle();
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
             "  cache\r\n"
             "  children\r\n"
             "  key <key>\r\n"
             "  key_sequence [sequence]\r\n"
             "  leader_data\r\n"
             "  mode [rsdn]\r\n"
             "  network_id_timeout [timeout]\r\n"
             "  release_router [router_id]\r\n"
             "  router_uprade_threshold [threshold]\r\n"
             "  routers\r\n"
             "  routes\r\n"
             "  start\r\n"
             "  state [detached|child|router|leader]\r\n"
             "  stop\r\n"
             "  timeout [timeout]\r\n"
             "  weight [weight]\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintAddressCache(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    int count = 0;
    const AddressResolver *resolver = mNetif->GetAddressResolver();
    uint16_t numEntries;
    const AddressResolver::Cache *entries = resolver->GetCacheEntries(&numEntries);

    for (int i = 0; i < numEntries; i++)
    {
        if (entries[i].mState == AddressResolver::Cache::kStateInvalid)
        {
            continue;
        }

        entries[i].mTarget.ToString(cur, end - cur);
        cur += strlen(cur);
        snprintf(cur, end - cur, " %d ", entries[i].mState);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%04x ", entries[i].mRloc);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%d\r\n", entries[i].mTimeout);
        cur += strlen(cur);
        count++;
    }

    snprintf(cur, end - cur, "Total: %d\r\n", count);
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintChildren(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    int count = 0;
    Child *children;
    uint8_t numChildren;

    VerifyOrExit((children = mMle->GetChildren(&numChildren)) != NULL, ;);

    for (int i = 0; i < numChildren; i++)
    {
        if (children[i].mState == Neighbor::kStateInvalid)
        {
            continue;
        }

        snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x: ",
                 children[i].mMacAddr.mBytes[0], children[i].mMacAddr.mBytes[1],
                 children[i].mMacAddr.mBytes[2], children[i].mMacAddr.mBytes[3],
                 children[i].mMacAddr.mBytes[4], children[i].mMacAddr.mBytes[5],
                 children[i].mMacAddr.mBytes[6], children[i].mMacAddr.mBytes[7]);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%04x, ", children[i].mValid.mRloc16);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%d, ", children[i].mState);
        cur += strlen(cur);

        if (children[i].mMode & Mle::ModeTlv::kModeRxOnWhenIdle)
        {
            snprintf(cur, end - cur, "r");
            cur += strlen(cur);
        }

        if (children[i].mMode & Mle::ModeTlv::kModeSecureDataRequest)
        {
            snprintf(cur, end - cur, "s");
            cur += strlen(cur);
        }

        if (children[i].mMode & Mle::ModeTlv::kModeFFD)
        {
            snprintf(cur, end - cur, "d");
            cur += strlen(cur);
        }

        if (children[i].mMode & Mle::ModeTlv::kModeFullNetworkData)
        {
            snprintf(cur, end - cur, "n");
            cur += strlen(cur);
        }

        snprintf(cur, end - cur, "\r\n");
        cur += strlen(cur);
        count++;
    }

exit:
    snprintf(cur, end - cur, "Total: %d\r\n", count);
    cur += strlen(cur);
    return cur - buf;
}

int Thread::PrintKey(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    uint8_t key[16];
    uint8_t keyLength;
    mNetif->GetKeyManager()->GetMasterKey(key, &keyLength);

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

    snprintf(cur, end - cur, "%u\r\n", mNetif->GetKeyManager()->GetCurrentKeySequence());
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintLeaderData(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    const Mle::LeaderDataTlv *leader_data = mMle->GetLeaderDataTlv();

    snprintf(cur, end - cur, "partition_id = %08" PRIx32 "\r\n", leader_data->GetPartitionId());
    cur += strlen(cur);
    snprintf(cur, end - cur, "weighting = %d\r\n", leader_data->GetWeighting());
    cur += strlen(cur);
    snprintf(cur, end - cur, "version = %d\r\n", leader_data->GetDataVersion());
    cur += strlen(cur);
    snprintf(cur, end - cur, "stable_version = %d\r\n", leader_data->GetStableDataVersion());
    cur += strlen(cur);
    snprintf(cur, end - cur, "leader_router_id = %d\r\n", leader_data->GetRouterId());
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintMode(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    uint8_t mode = mMle->GetDeviceMode();

    if (mode & Mle::ModeTlv::kModeRxOnWhenIdle)
    {
        *cur++ = 'r';
    }

    if (mode & Mle::ModeTlv::kModeSecureDataRequest)
    {
        *cur++ = 's';
    }

    if (mode & Mle::ModeTlv::kModeFFD)
    {
        *cur++ = 'd';
    }

    if (mode & Mle::ModeTlv::kModeFullNetworkData)
    {
        *cur++ = 'n';
    }

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintRouters(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    int count = 0;
    Router *routers;
    uint8_t numRouters;

    VerifyOrExit((routers = mMle->GetRouters(&numRouters)) != NULL, ;);

    count = 0;

    for (int i = 0; i < numRouters; i++)
    {
        if (routers[i].mState == Neighbor::kStateInvalid)
        {
            continue;
        }

        snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x: ",
                 routers[i].mMacAddr.mBytes[0], routers[i].mMacAddr.mBytes[1],
                 routers[i].mMacAddr.mBytes[2], routers[i].mMacAddr.mBytes[3],
                 routers[i].mMacAddr.mBytes[4], routers[i].mMacAddr.mBytes[5],
                 routers[i].mMacAddr.mBytes[6], routers[i].mMacAddr.mBytes[7]);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%04x\r\n", routers[i].mValid.mRloc16);
        cur += strlen(cur);
        count++;
    }

exit:
    snprintf(cur, end - cur, "Total: %d\r\n", count);
    cur += strlen(cur);
    return cur - buf;
}

int Thread::PrintRoutes(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    int count = 0;
    Router *routers;
    uint8_t numRouters;

    VerifyOrExit((routers = mMle->GetRouters(&numRouters)) != NULL, ;);

    snprintf(cur, end - cur, "seq: %d\r\n", mMle->GetRouterIdSequence());
    cur += strlen(cur);

    snprintf(cur, end - cur, "mask: ");
    cur += strlen(cur);

    for (int i = 0; i < Mle::kMaxRouterId; i++)
    {
        if (routers[i].mAllocated)
        {
            snprintf(cur, end - cur, "%d ", i);
            cur += strlen(cur);
        }
    }

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    count = 0;

    switch (mMle->GetDeviceState())
    {
    case Mle::kDeviceStateDisabled:
    case Mle::kDeviceStateDetached:
        break;

    case Mle::kDeviceStateChild:
        snprintf(cur, end - cur, "%04x: %04x (0)\r\n",
                 Mac::kShortAddrBroadcast, routers->mValid.mRloc16);
        cur += strlen(cur);
        count++;
        break;

    case Mle::kDeviceStateRouter:
    case Mle::kDeviceStateLeader:
        for (int i = 0; i < numRouters; i++)
        {
            if (routers[i].mAllocated == false)
            {
                continue;
            }

            snprintf(cur, end - cur, "%d: %d, %d, %d, %u\r\n",
                     i, routers[i].mState, routers[i].mNextHop, routers[i].mCost,
                     (Timer::GetNow() - routers[i].mLastHeard) / 1000U);
            cur += strlen(cur);
            count++;
        }

        break;
    }

exit:
    snprintf(cur, end - cur, "Total: %d\r\n", count);
    cur += strlen(cur);
    return cur - buf;
}

int Thread::PrintState(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = buf + bufLength;

    switch (mMle->GetDeviceState())
    {
    case Mle::kDeviceStateDisabled:
        snprintf(cur, end - cur, "disabled\r\n");
        break;

    case Mle::kDeviceStateDetached:
        snprintf(cur, end - cur, "detached\r\n");
        break;

    case Mle::kDeviceStateChild:
        snprintf(cur, end - cur, "child\r\n");
        break;

    case Mle::kDeviceStateRouter:
        snprintf(cur, end - cur, "router\r\n");
        break;

    case Mle::kDeviceStateLeader:
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
    uint8_t mode = 0;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            ExitNow();
        }
        else if (strcmp(argv[i], "cache") == 0)
        {
            cur += PrintAddressCache(buf, sizeof(buf));
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "children") == 0)
        {
            cur += PrintChildren(buf, sizeof(buf));
            ExitNow(error = kThreadError_None);
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

            ExitNow(error = mNetif->GetKeyManager()->SetMasterKey(key, keyLength));
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
                ExitNow(error = mNetif->GetKeyManager()->SetCurrentKeySequence(val32));
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
                    mode |= Mle::ModeTlv::kModeRxOnWhenIdle;
                    break;

                case 's':
                    mode |= Mle::ModeTlv::kModeSecureDataRequest;
                    break;

                case 'd':
                    mode |= Mle::ModeTlv::kModeFFD;
                    break;

                case 'n':
                    mode |= Mle::ModeTlv::kModeFullNetworkData;
                    break;

                default:
                    ExitNow();
                }
            }

            ExitNow(error = mMle->SetDeviceMode(mode));
        }
        else if (strcmp(argv[i], "network_id_timeout") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", mMle->GetNetworkIdTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                ExitNow(error = mMle->SetNetworkIdTimeout(val32));
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
                ExitNow(error = mMle->ReleaseRouterId(val8));
            }
        }
        else if (strcmp(argv[i], "router_upgrade_threshold") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", mMle->GetNetworkIdTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                ExitNow(error = mMle->SetRouterUpgradeThreshold(val32));
            }
        }
        else if (strcmp(argv[i], "routers") == 0)
        {
            cur += PrintRouters(buf, sizeof(buf));
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "routes") == 0)
        {
            cur += PrintRoutes(buf, sizeof(buf));
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "start") == 0)
        {
            mNetif->Up();
            ExitNow(error = kThreadError_None);
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
                ExitNow(error = mMle->BecomeDetached());
            }
            else if (strcmp(argv[i], "child") == 0)
            {
                ExitNow(error = mMle->BecomeChild(Mle::kJoinSamePartition));
            }
            else if (strcmp(argv[i], "router") == 0)
            {
                ExitNow(error = mMle->BecomeRouter());
            }
            else if (strcmp(argv[i], "leader") == 0)
            {
                ExitNow(error = mMle->BecomeLeader());
            }
            else
            {
                ExitNow();
            }
        }
        else if (strcmp(argv[i], "stop") == 0)
        {
            ExitNow(error = mNetif->Down());
        }
        else if (strcmp(argv[i], "timeout") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%u\n", mMle->GetTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                ExitNow(error = mMle->SetTimeout(val32));
            }
        }
        else if (strcmp(argv[i], "weight") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", mMle->GetLeaderWeight());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val8 = strtol(argv[i], NULL, 0);
                ExitNow(error = mMle->SetLeaderWeight(val8));
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
