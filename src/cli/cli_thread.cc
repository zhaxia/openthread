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

#include <cli/cli.h>
#include <cli/cli_thread.h>
#include <common/code_utils.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {
namespace Cli {

static const char kName[] = "thread";

Thread::Thread(Server &server, ThreadNetif &netif):
    Command(server)
{
    m_netif = &netif;
    m_mle = netif.GetMle();
}

const char *Thread::GetName()
{
    return kName;
}

int Thread::PrintUsage(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = cur + buf_length;

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

int Thread::PrintAddressCache(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    int count = 0;
    const AddressResolver *resolver = m_netif->GetAddressResolver();
    uint16_t num_entries;
    const AddressResolver::Cache *entries = resolver->GetCacheEntries(&num_entries);

    for (int i = 0; i < num_entries; i++)
    {
        if (entries[i].state == AddressResolver::Cache::kStateInvalid)
        {
            continue;
        }

        entries[i].target.ToString(cur, end - cur);
        cur += strlen(cur);
        snprintf(cur, end - cur, " %d ", entries[i].state);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%04x ", entries[i].rloc);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%d\r\n", entries[i].timeout);
        cur += strlen(cur);
        count++;
    }

    snprintf(cur, end - cur, "Total: %d\r\n", count);
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintChildren(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    int count = 0;
    Child *children;
    uint8_t num_children;

    VerifyOrExit((children = m_mle->GetChildren(&num_children)) != NULL, ;);

    for (int i = 0; i < num_children; i++)
    {
        if (children[i].state == Neighbor::kStateInvalid)
        {
            continue;
        }

        snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x: ",
                 children[i].mac_addr.bytes[0], children[i].mac_addr.bytes[1],
                 children[i].mac_addr.bytes[2], children[i].mac_addr.bytes[3],
                 children[i].mac_addr.bytes[4], children[i].mac_addr.bytes[5],
                 children[i].mac_addr.bytes[6], children[i].mac_addr.bytes[7]);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%04x, ", children[i].valid.rloc16);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%d, ", children[i].state);
        cur += strlen(cur);

        if (children[i].mode & Mle::ModeTlv::kModeRxOnWhenIdle)
        {
            snprintf(cur, end - cur, "r");
            cur += strlen(cur);
        }

        if (children[i].mode & Mle::ModeTlv::kModeSecureDataRequest)
        {
            snprintf(cur, end - cur, "s");
            cur += strlen(cur);
        }

        if (children[i].mode & Mle::ModeTlv::kModeFFD)
        {
            snprintf(cur, end - cur, "d");
            cur += strlen(cur);
        }

        if (children[i].mode & Mle::ModeTlv::kModeFullNetworkData)
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

int Thread::PrintKey(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    uint8_t key[16];
    uint8_t key_length;
    m_netif->GetKeyManager()->GetMasterKey(key, &key_length);

    for (int i = 0; i < key_length; i++)
    {
        snprintf(cur, end - cur, "%02x", key[i]);
        cur += strlen(cur);
    }

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintKeySequence(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    snprintf(cur, end - cur, "%d\r\n", m_netif->GetKeyManager()->GetCurrentKeySequence());
    cur += strlen(cur);

    return cur - buf;
}

int Thread::PrintLeaderData(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    const Mle::LeaderDataTlv *leader_data = m_mle->GetLeaderDataTlv();

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

int Thread::PrintMode(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    uint8_t mode = m_mle->GetDeviceMode();

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

int Thread::PrintRouters(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    int count = 0;
    Router *routers;
    uint8_t num_routers;

    VerifyOrExit((routers = m_mle->GetRouters(&num_routers)) != NULL, ;);

    count = 0;

    for (int i = 0; i < num_routers; i++)
    {
        if (routers[i].state == Neighbor::kStateInvalid)
        {
            continue;
        }

        snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x: ",
                 routers[i].mac_addr.bytes[0], routers[i].mac_addr.bytes[1],
                 routers[i].mac_addr.bytes[2], routers[i].mac_addr.bytes[3],
                 routers[i].mac_addr.bytes[4], routers[i].mac_addr.bytes[5],
                 routers[i].mac_addr.bytes[6], routers[i].mac_addr.bytes[7]);
        cur += strlen(cur);
        snprintf(cur, end - cur, "%04x\r\n", routers[i].valid.rloc16);
        cur += strlen(cur);
        count++;
    }

exit:
    snprintf(cur, end - cur, "Total: %d\r\n", count);
    cur += strlen(cur);
    return cur - buf;
}

int Thread::PrintRoutes(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    int count = 0;
    Router *routers;
    uint8_t num_routers;

    VerifyOrExit((routers = m_mle->GetRouters(&num_routers)) != NULL, ;);

    snprintf(cur, end - cur, "seq: %d\r\n", m_mle->GetRouterIdSequence());
    cur += strlen(cur);

    snprintf(cur, end - cur, "mask: ");
    cur += strlen(cur);

    for (int i = 0; i < Mle::kMaxRouterId; i++)
    {
        if (routers[i].allocated)
        {
            snprintf(cur, end - cur, "%d ", i);
            cur += strlen(cur);
        }
    }

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    count = 0;

    switch (m_mle->GetDeviceState())
    {
    case Mle::kDeviceStateDisabled:
    case Mle::kDeviceStateDetached:
        break;

    case Mle::kDeviceStateChild:
        snprintf(cur, end - cur, "%04x: %04x (0)\r\n",
                 Mac::kShortAddrBroadcast, routers->valid.rloc16);
        cur += strlen(cur);
        count++;
        break;

    case Mle::kDeviceStateRouter:
    case Mle::kDeviceStateLeader:
        for (int i = 0; i < num_routers; i++)
        {
            if (routers[i].allocated == false)
            {
                continue;
            }

            snprintf(cur, end - cur, "%d: %d, %d, %d, %d\r\n",
                     i, routers[i].state, routers[i].nexthop, routers[i].cost,
                     (Timer::GetNow() - routers[i].last_heard) / 1000U);
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

int Thread::PrintState(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    switch (m_mle->GetDeviceState())
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
    int key_length;
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

            if ((key_length = hex2bin(argv[i], key, sizeof(key))) < 0)
            {
                ExitNow();
            }

            ExitNow(error = m_netif->GetKeyManager()->SetMasterKey(key, key_length));
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
                ExitNow(error = m_netif->GetKeyManager()->SetCurrentKeySequence(val32));
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

            ExitNow(error = m_mle->SetDeviceMode(mode));
        }
        else if (strcmp(argv[i], "network_id_timeout") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", m_mle->GetNetworkIdTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                ExitNow(error = m_mle->SetNetworkIdTimeout(val32));
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
                ExitNow(error = m_mle->ReleaseRouterId(val8));
            }
        }
        else if (strcmp(argv[i], "router_upgrade_threshold") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", m_mle->GetNetworkIdTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                ExitNow(error = m_mle->SetRouterUpgradeThreshold(val32));
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
            m_netif->Up();
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
                ExitNow(error = m_mle->BecomeDetached());
            }
            else if (strcmp(argv[i], "child") == 0)
            {
                ExitNow(error = m_mle->BecomeChild(Mle::kJoinSamePartition));
            }
            else if (strcmp(argv[i], "router") == 0)
            {
                ExitNow(error = m_mle->BecomeRouter());
            }
            else if (strcmp(argv[i], "leader") == 0)
            {
                ExitNow(error = m_mle->BecomeLeader());
            }
            else
            {
                ExitNow();
            }
        }
        else if (strcmp(argv[i], "stop") == 0)
        {
            ExitNow(error = m_netif->Down());
        }
        else if (strcmp(argv[i], "timeout") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", m_mle->GetTimeout());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val32 = strtol(argv[i], NULL, 0);
                ExitNow(error = m_mle->SetTimeout(val32));
            }
        }
        else if (strcmp(argv[i], "weight") == 0)
        {
            if (++i >= argc)
            {
                snprintf(cur, end - cur, "%d\n", m_mle->GetLeaderWeight());
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }
            else
            {
                val8 = strtol(argv[i], NULL, 0);
                ExitNow(error = m_mle->SetLeaderWeight(val8));
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
