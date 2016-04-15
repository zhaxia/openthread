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
#include <cli/cli_mac.hpp>
#include <common/code_utils.hpp>

namespace Thread {
namespace Cli {

static const char kName[] = "mac";

Mac::Mac(Server &server, ThreadNetif &netif):
    Command(server)
{
    mMac = netif.GetMac();
    mServer = &server;
}

const char *Mac::GetName()
{
    return kName;
}

int Mac::PrintUsage(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

    snprintf(cur, end - cur,
             "usage: mac\r\n"
             "  addr16\r\n"
             "  addr64\r\n"
             "  channel [channel]\r\n"
             "  name [name]\r\n"
             "  panid [panid]\r\n"
             "  xpanid [xpanid]\r\n"
             "  scan [results]\r\n"
             "  whitelist [add|disable|enable|remove]\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Mac::PrintWhitelist(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

    Thread::Mac::Whitelist *whitelist = mMac->GetWhitelist();
    int length = whitelist->GetMaxEntries();

    if (whitelist->IsEnabled())
    {
        snprintf(cur, end - cur, "Enabled\r\n");
    }
    else
    {
        snprintf(cur, end - cur, "Disabled\r\n");
    }

    cur += strlen(cur);

    for (int i = 0; i < length; i++)
    {
        const uint8_t *addr = whitelist->GetAddress(i);

        if (addr == NULL)
        {
            continue;
        }

        snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
                 addr[0], addr[1], addr[2], addr[3],
                 addr[4], addr[5], addr[6], addr[7]);
        cur += strlen(cur);
    }

    return cur - buf;
}

int Mac::ProcessWhitelist(int argc, char *argv[], char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;
    int argcur = 0;
    int rval;

    Thread::Mac::Address64 macaddr;
    int entry;
    int8_t rssi;

    if (argcur >= argc)
    {
        cur += PrintWhitelist(cur, end - cur);
    }
    else if (strcmp(argv[argcur], "add") == 0)
    {
        VerifyOrExit(++argcur < argc, rval = -1);
        VerifyOrExit(hex2bin(argv[argcur], macaddr.mBytes, sizeof(macaddr.mBytes)) == sizeof(macaddr.mBytes), rval = -1);
        VerifyOrExit((entry = mMac->GetWhitelist()->Add(macaddr)) >= 0, rval = -1);

        if (++argcur < argc)
        {
            rssi = strtol(argv[argcur], NULL, 0);
            mMac->GetWhitelist()->SetRssi(entry, rssi);
        }
    }
    else if (strcmp(argv[argcur], "clear") == 0)
    {
        mMac->GetWhitelist()->Clear();
    }
    else if (strcmp(argv[argcur], "disable") == 0)
    {
        mMac->GetWhitelist()->Disable();
    }
    else if (strcmp(argv[argcur], "enable") == 0)
    {
        mMac->GetWhitelist()->Enable();
    }
    else if (strcmp(argv[argcur], "remove") == 0)
    {
        VerifyOrExit(++argcur < argc, rval = -1);
        VerifyOrExit(hex2bin(argv[argcur], macaddr.mBytes, sizeof(macaddr.mBytes)) == sizeof(macaddr.mBytes), rval = -1);
        VerifyOrExit(mMac->GetWhitelist()->Remove(macaddr) == kThreadError_None, rval = -1);
    }

    rval = cur - buf;

exit:
    return rval;
}

void Mac::Run(int argc, char *argv[], Server &server)
{
    ThreadError error = kThreadError_Error;
    char buf[2048];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            ExitNow(error = kThreadError_InvalidArgs);
        }
        else if (strcmp(argv[i], "addr16") == 0)
        {
            Thread::Mac::Address16 addr16 = mMac->GetAddress16();
            snprintf(cur, end - cur, "%04x\r\n", addr16);
            cur += strlen(cur);
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "addr64") == 0)
        {
            const Thread::Mac::Address64 *addr64 = mMac->GetAddress64();
            snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
                     addr64->mBytes[0], addr64->mBytes[1], addr64->mBytes[2], addr64->mBytes[3],
                     addr64->mBytes[4], addr64->mBytes[5], addr64->mBytes[6], addr64->mBytes[7]);
            cur += strlen(cur);
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "channel") == 0)
        {
            if (++i >= argc)
            {
                uint8_t channel = mMac->GetChannel();
                snprintf(cur, end - cur, "%d\r\n", channel);
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }

            uint8_t channel;
            channel = strtol(argv[i], NULL, 0);
            mMac->SetChannel(channel);
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "name") == 0)
        {
            if (++i >= argc)
            {
                const char *name = mMac->GetNetworkName();
                snprintf(cur, end - cur, "%s\r\n", name);
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }

            mMac->SetNetworkName(argv[i]);
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "panid") == 0)
        {
            if (++i >= argc)
            {
                uint16_t panid = mMac->GetPanId();
                snprintf(cur, end - cur, "%04x\r\n", panid);
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }

            uint16_t panid;
            panid = strtol(argv[i], NULL, 0);
            mMac->SetPanId(panid);
            ExitNow(error = kThreadError_None);
        }
        else if (strcmp(argv[i], "scan") == 0)
        {
            mMac->ActiveScan(Thread::Mac::kMacScanDefaultInterval, Thread::Mac::kMacScanChannelMaskAllChannels,
                             &HandleActiveScanResult, this);
            snprintf(cur, end - cur, "| Network Name     | Extended PAN     | PAN  | MAC Address      | Ch | dBm |\r\n");
            cur += strlen(cur);
            server.Output(buf, cur - buf);
            return;
        }
        else if (strcmp(argv[i], "whitelist") == 0)
        {
            i++;
            cur += ProcessWhitelist(argc - i, &argv[i], cur, end - cur);
        }
        else if (strcmp(argv[i], "xpanid") == 0)
        {
            if (++i >= argc)
            {
                const uint8_t *xpanid = mMac->GetExtendedPanId();
                snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
                         xpanid[0], xpanid[1], xpanid[2], xpanid[3], xpanid[4], xpanid[5], xpanid[6], xpanid[7]);
                cur += strlen(cur);
                ExitNow(error = kThreadError_None);
            }

            uint8_t xpanid[8];
            memset(xpanid, 0, sizeof(xpanid));

            VerifyOrExit(hex2bin(argv[i], xpanid, sizeof(xpanid)) >= 0, error = kThreadError_InvalidArgs);

            mMac->SetExtendedPanId(xpanid);
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

void Mac::HandleActiveScanResult(void *context, Thread::Mac::ActiveScanResult *result)
{
    Mac *obj = reinterpret_cast<Mac *>(context);
    obj->HandleActiveScanResult(result);
}

void Mac::HandleActiveScanResult(Thread::Mac::ActiveScanResult *result)
{
    char buf[256];
    char *cur = buf;
    char *end = cur + sizeof(buf);
    uint8_t *bytes;

    if (result != NULL)
    {
        snprintf(cur, end - cur, "| %-16s ", result->mNetworkName);
        cur += strlen(cur);

        bytes = result->mExtPanid;
        snprintf(cur, end - cur, "| %02x%02x%02x%02x%02x%02x%02x%02x ",
                 bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
        cur += strlen(cur);

        snprintf(cur, end - cur, "| %04x ", result->mPanid);
        cur += strlen(cur);

        bytes = result->mExtAddr;
        snprintf(cur, end - cur, "| %02x%02x%02x%02x%02x%02x%02x%02x ",
                 bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
        cur += strlen(cur);

        snprintf(cur, end - cur, "| %02d ", result->mChannel);
        cur += strlen(cur);

        snprintf(cur, end - cur, "| %03d ", result->mRssi);
        cur += strlen(cur);

        snprintf(cur, end - cur, "|\r\n");
        cur += strlen(cur);
    }
    else
    {
        snprintf(cur, end - cur, "Done\r\n");
        cur += strlen(cur);
    }

    mServer->Output(buf, cur - buf);
}

}  // namespace Cli
}  // namespace Thread
