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
 *   This file implements the CLI interpreter.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openthread.h>

#include "cli.hpp"
#include <common/encoding.hpp>
#include <net/icmp6.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Cli {

enum
{
    kMaxArgs = 8,
};

static void ProcessHelp(int argc, char *argv[]);
static void ProcessChannel(int argc, char *argv[]);
static void ProcessChildTimeout(int argc, char *argv[]);
static void ProcessContextIdReuseDelay(int argc, char *argv[]);
static void ProcessExtAddress(int argc, char *argv[]);
static void ProcessExtPanId(int argc, char *argv[]);
static void ProcessIpAddr(int argc, char *argv[]);
static void ProcessKeySequence(int argc, char *argv[]);
static void ProcessLeaderWeight(int argc, char *argv[]);
static void ProcessMasterKey(int argc, char *argv[]);
static void ProcessMode(int argc, char *argv[]);
static void ProcessNetworkDataRegister(int argc, char *argv[]);
static void ProcessNetworkIdTimeout(int argc, char *argv[]);
static void ProcessNetworkName(int argc, char *argv[]);
static void ProcessPanId(int argc, char *argv[]);
static void ProcessPing(int argc, char *argv[]);
static void ProcessPrefix(int argc, char *argv[]);
static void ProcessReleaseRouterId(int argc, char *argv[]);
static void ProcessRoute(int argc, char *argv[]);
static void ProcessRouterUpgradeThreshold(int argc, char *argv[]);
static void ProcessRloc16(int argc, char *argv[]);
static void ProcessShutdown(int argc, char *argv[]);
static void ProcessStart(int argc, char *argv[]);
static void ProcessState(int argc, char *argv[]);
static void ProcessStop(int argc, char *argv[]);
static void ProcessWhitelist(int argc, char *argv[]);

struct Command
{
    const char *mName;
    void (*mCommand)(int argc, char *argv[]);
};

static struct Command sCommands[] =
{
    { "help", &ProcessHelp },
    { "channel", &ProcessChannel },
    { "childtimeout", &ProcessChildTimeout },
    { "contextreusedelay", &ProcessContextIdReuseDelay },
    { "extaddr", &ProcessExtAddress },
    { "extpanid", &ProcessExtPanId },
    { "ipaddr", &ProcessIpAddr },
    { "keysequence", &ProcessKeySequence },
    { "leaderweight", &ProcessLeaderWeight },
    { "masterkey", &ProcessMasterKey },
    { "mode", &ProcessMode },
    { "netdataregister", &ProcessNetworkDataRegister },
    { "networkidtimeout", &ProcessNetworkIdTimeout },
    { "networkname", &ProcessNetworkName },
    { "panid", &ProcessPanId },
    { "ping", &ProcessPing },
    { "prefix", &ProcessPrefix },
    { "releaserouterid", &ProcessReleaseRouterId },
    { "rloc16", &ProcessRloc16 },
    { "route", &ProcessRoute },
    { "routerupgradethreshold", &ProcessRouterUpgradeThreshold },
    { "shutdown", &ProcessShutdown },
    { "start", &ProcessStart },
    { "state", &ProcessState },
    { "stop", &ProcessStop },
    { "whitelist", &ProcessWhitelist },
};

static int hex2bin(const char *hex, uint8_t *bin, uint16_t binLength)
{
    uint16_t hexLength = strlen(hex);
    const char *hexEnd = hex + hexLength;
    uint8_t *cur = bin;
    uint8_t numChars = hexLength & 1;
    uint8_t byte = 0;

    if ((hexLength + 1) / 2 > binLength)
    {
        return -1;
    }

    while (hex < hexEnd)
    {
        if ('A' <= *hex && *hex <= 'F')
        {
            byte |= 10 + (*hex - 'A');
        }
        else if ('a' <= *hex && *hex <= 'f')
        {
            byte |= 10 + (*hex - 'a');
        }
        else if ('0' <= *hex && *hex <= '9')
        {
            byte |= *hex - '0';
        }
        else
        {
            return -1;
        }

        hex++;
        numChars++;

        if (numChars >= 2)
        {
            numChars = 0;
            *cur++ = byte;
            byte = 0;
        }
        else
        {
            byte <<= 4;
        }
    }

    return cur - bin;
}

class ResponseBuffer
{
public:
    void Init() { mEnd = mBuffer; }

    void Append(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(mEnd, sizeof(mBuffer) - (mEnd - mBuffer), fmt, ap);
        va_end(ap);
        mEnd += strlen(mEnd);
    }

    const char *GetResponse() { return mBuffer; }

    uint16_t GetResponseLength() { return mEnd - mBuffer; }

private:
    char mBuffer[512];
    char *mEnd;
};

static ResponseBuffer sResponse;

ThreadError ParseLong(char *argv, long &value)
{
    char *endptr;
    value = strtol(argv, &endptr, 0);
    return (*endptr == '\0') ? kThreadError_None : kThreadError_Parse;
}

void ProcessHelp(int argc, char *argv[])
{
    for (int i = 0; i < sizeof(sCommands) / sizeof(sCommands[0]); i++)
    {
        sResponse.Append("%s\n", sCommands[i].mName);
    }
}

void ProcessChannel(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetChannel());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetChannel(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessChildTimeout(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetChildTimeout());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetChildTimeout(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessContextIdReuseDelay(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetContextIdReuseDelay());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetContextIdReuseDelay(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessExtAddress(int argc, char *argv[])
{
    const uint8_t *extAddress = otGetExtendedAddress();
    sResponse.Append("%02x%02x%02x%02x%02x%02x%02x%02x\n",
                     extAddress[0], extAddress[1], extAddress[2], extAddress[3],
                     extAddress[4], extAddress[5], extAddress[6], extAddress[7]);
    sResponse.Append("Done\n");
}

void ProcessExtPanId(int argc, char *argv[])
{
    if (argc == 0)
    {
        const uint8_t *extPanId = otGetExtendedPanId();
        sResponse.Append("%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
                         extPanId[0], extPanId[1], extPanId[2], extPanId[3],
                         extPanId[4], extPanId[5], extPanId[6], extPanId[7]);
    }
    else
    {
        uint8_t extPanId[8];

        VerifyOrExit(hex2bin(argv[0], extPanId, sizeof(extPanId)) >= 0, ;);

        otSetExtendedPanId(extPanId);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

static otNetifAddress sAddress;

ThreadError ProcessIpAddrAdd(int argc, char *argv[])
{
    ThreadError error;

    VerifyOrExit(argc > 0, error = kThreadError_Parse);

    SuccessOrExit(error = otIp6AddressFromString(argv[0], &sAddress.mAddress));
    sAddress.mPrefixLength = 64;
    sAddress.mPreferredLifetime = 0xffffffff;
    sAddress.mValidLifetime = 0xffffffff;
    error = otAddUnicastAddress(&sAddress);

exit:
    return error;
}

ThreadError ProcessIpAddrDel(int argc, char *argv[])
{
    ThreadError error;
    struct otIp6Address address;

    VerifyOrExit(argc > 0, error = kThreadError_Parse);

    SuccessOrExit(error = otIp6AddressFromString(argv[0], &address));
    VerifyOrExit(otIsIp6AddressEqual(&address, &sAddress.mAddress), error = kThreadError_Parse);
    error = otRemoveUnicastAddress(&sAddress);

exit:
    return error;
}

void ProcessIpAddr(int argc, char *argv[])
{
    if (argc == 0)
    {
        for (const otNetifAddress *addr = otGetUnicastAddresses(); addr; addr = addr->mNext)
        {
            sResponse.Append("%x:%x:%x:%x:%x:%x:%x:%x\n",
                             HostSwap16(addr->mAddress.m16[0]), HostSwap16(addr->mAddress.m16[1]),
                             HostSwap16(addr->mAddress.m16[2]), HostSwap16(addr->mAddress.m16[3]),
                             HostSwap16(addr->mAddress.m16[4]), HostSwap16(addr->mAddress.m16[5]),
                             HostSwap16(addr->mAddress.m16[6]), HostSwap16(addr->mAddress.m16[7]));
        }
    }
    else
    {
        if (strcmp(argv[0], "add") == 0)
        {
            SuccessOrExit(ProcessIpAddrAdd(argc - 1, argv + 1));
        }
        else if (strcmp(argv[0], "del") == 0)
        {
            SuccessOrExit(ProcessIpAddrDel(argc - 1, argv + 1));
        }
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessKeySequence(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetKeySequenceCounter());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetKeySequenceCounter(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessLeaderWeight(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetLocalLeaderWeight());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetLocalLeaderWeight(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessMasterKey(int argc, char *argv[])
{
    uint8_t keyLength;

    if (argc == 0)
    {
        const uint8_t *key = otGetMasterKey(&keyLength);

        for (int i = 0; i < keyLength; i++)
        {
            sResponse.Append("%02x", key[i]);
        }

        sResponse.Append("\n");
    }
    else
    {
        uint8_t key[16];

        VerifyOrExit((keyLength = hex2bin(argv[0], key, sizeof(key))) >= 0, ;);
        SuccessOrExit(otSetMasterKey(key, keyLength));
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessMode(int argc, char *argv[])
{
    otLinkModeConfig linkMode = {};

    if (argc == 0)
    {
        linkMode = otGetLinkMode();

        if (linkMode.mRxOnWhenIdle)
        {
            sResponse.Append("r");
        }

        if (linkMode.mSecureDataRequests)
        {
            sResponse.Append("s");
        }

        if (linkMode.mDeviceType)
        {
            sResponse.Append("d");
        }

        if (linkMode.mNetworkData)
        {
            sResponse.Append("n");
        }

        sResponse.Append("\n");
    }
    else
    {
        for (char *arg = argv[0]; *arg != '\0'; arg++)
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

        SuccessOrExit(otSetLinkMode(linkMode));
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessNetworkDataRegister(int argc, char *argv[])
{
    SuccessOrExit(otSendServerData());
    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessNetworkIdTimeout(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetNetworkIdTimeout());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetNetworkIdTimeout(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessNetworkName(int argc, char *argv[])
{
    if (argc == 0)
    {
        sResponse.Append("%s\n", otGetNetworkName());
    }
    else
    {
        SuccessOrExit(otSetNetworkName(argv[0]));
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessPanId(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetPanId());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetPanId(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

static void HandleEchoResponse(void *aContext, Message &aMessage, const Ip6::MessageInfo &aMessageInfo);

static Ip6::SockAddr sSockAddr;
static Ip6::IcmpEcho sIcmpEcho(&HandleEchoResponse, NULL);
static Server *sServer;
static uint8_t sEchoRequest[2024];

void HandleEchoResponse(void *aContext, Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    Ip6::IcmpHeader icmp6Header;

    aMessage.Read(aMessage.GetOffset(), sizeof(icmp6Header), &icmp6Header);

    sResponse.Init();
    sResponse.Append("%d bytes from ", aMessage.GetLength() - aMessage.GetOffset());
    sResponse.Append("%x:%x:%x:%x:%x:%x:%x:%x",
                     HostSwap16(aMessageInfo.GetPeerAddr().m16[0]), HostSwap16(aMessageInfo.GetPeerAddr().m16[1]),
                     HostSwap16(aMessageInfo.GetPeerAddr().m16[2]), HostSwap16(aMessageInfo.GetPeerAddr().m16[3]),
                     HostSwap16(aMessageInfo.GetPeerAddr().m16[4]), HostSwap16(aMessageInfo.GetPeerAddr().m16[5]),
                     HostSwap16(aMessageInfo.GetPeerAddr().m16[6]), HostSwap16(aMessageInfo.GetPeerAddr().m16[7]));
    sResponse.Append(": icmp_seq=%d hlim=%d\n", icmp6Header.GetSequence(), aMessageInfo.mHopLimit);

    sServer->Output(sResponse.GetResponse(), sResponse.GetResponseLength());
}

void ProcessPing(int argc, char *argv[])
{
    long length = 8;

    VerifyOrExit(argc > 0, ;);

    memset(&sSockAddr, 0, sizeof(sSockAddr));
    SuccessOrExit(sSockAddr.GetAddress().FromString(argv[0]));
    sSockAddr.mScopeId = 1;

    if (argc > 1)
    {
        SuccessOrExit(ParseLong(argv[1], length));
    }

    sIcmpEcho.SendEchoRequest(sSockAddr, sEchoRequest, length);

    sResponse.Init();

exit:
    {}
}

static ThreadError ProcessPrefixAdd(int argc, char *argv[])
{
    ThreadError error = kThreadError_None;
    otBorderRouterConfig config = {};
    int argcur = 0;

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    SuccessOrExit(error = otIp6AddressFromString(argv[argcur], &config.mPrefix.mPrefix));

    config.mPrefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow(error = kThreadError_Parse);
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
            ExitNow(error = kThreadError_Parse);
        }
    }

    error = otAddBorderRouter(&config);

exit:
    return error;
}

static ThreadError ProcessPrefixRemove(int argc, char *argv[])
{
    ThreadError error = kThreadError_None;
    struct otIp6Prefix prefix = {};
    int argcur = 0;

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    SuccessOrExit(error = otIp6AddressFromString(argv[argcur], &prefix.mPrefix));

    prefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow(error = kThreadError_Parse);
    }

    error = otRemoveBorderRouter(&prefix);

exit:
    return error;
}

void ProcessPrefix(int argc, char *argv[])
{
    if (strcmp(argv[0], "add") == 0)
    {
        SuccessOrExit(ProcessPrefixAdd(argc - 1, argv + 1));
    }
    else if (strcmp(argv[0], "remove") == 0)
    {
        SuccessOrExit(ProcessPrefixRemove(argc - 1, argv + 1));
    }
    else
    {
        ExitNow();
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessReleaseRouterId(int argc, char *argv[])
{
    long value;

    if (argc != 0)
    {
        SuccessOrExit(ParseLong(argv[0], value));
        SuccessOrExit(otReleaseRouterId(value));
        sResponse.Append("Done\n");
    }

exit:
    {}
}

void ProcessRloc16(int argc, char *argv[])
{
    sResponse.Append("%04x\n", otGetRloc16());
    sResponse.Append("Done\n");
}

static ThreadError ProcessRouteAdd(int argc, char *argv[])
{
    ThreadError error = kThreadError_None;
    otExternalRouteConfig config = {};
    int argcur = 0;

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    SuccessOrExit(error = otIp6AddressFromString(argv[argcur], &config.mPrefix.mPrefix));

    config.mPrefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow(error = kThreadError_Parse);
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
            ExitNow(error = kThreadError_Parse);
        }
    }

    error = otAddExternalRoute(&config);

exit:
    return error;
}

static ThreadError ProcessRouteRemove(int argc, char *argv[])
{
    ThreadError error = kThreadError_None;
    struct otIp6Prefix prefix = {};
    int argcur = 0;

    char *prefixLengthStr;
    char *endptr;

    if ((prefixLengthStr = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLengthStr++ = '\0';

    SuccessOrExit(error = otIp6AddressFromString(argv[argcur], &prefix.mPrefix));

    prefix.mLength = strtol(prefixLengthStr, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow(error = kThreadError_Parse);
    }

    error = otRemoveExternalRoute(&prefix);

exit:
    return error;
}

void ProcessRoute(int argc, char *argv[])
{
    if (strcmp(argv[0], "add") == 0)
    {
        SuccessOrExit(ProcessRouteAdd(argc - 1, argv + 1));
    }
    else if (strcmp(argv[0], "remove") == 0)
    {
        SuccessOrExit(ProcessRouteRemove(argc - 1, argv + 1));
    }
    else
    {
        ExitNow();
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessRouterUpgradeThreshold(int argc, char *argv[])
{
    long value;

    if (argc == 0)
    {
        sResponse.Append("%d\n", otGetRouterUpgradeThreshold());
    }
    else
    {
        SuccessOrExit(ParseLong(argv[0], value));
        otSetRouterUpgradeThreshold(value);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessShutdown(int argc, char *argv[])
{
    sResponse.Append("Done\n");
    sServer->Output(sResponse.GetResponse(), sResponse.GetResponseLength());
    exit(0);
}

void ProcessStart(int argc, char *argv[])
{
    SuccessOrExit(otEnable());
    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessState(int argc, char *argv[])
{
    if (argc == 0)
    {
        switch (otGetDeviceRole())
        {
        case kDeviceRoleDisabled:
            sResponse.Append("disabled\r\n");
            break;

        case kDeviceRoleDetached:
            sResponse.Append("detached\r\n");
            break;

        case kDeviceRoleChild:
            sResponse.Append("child\r\n");
            break;

        case kDeviceRoleRouter:
            sResponse.Append("router\r\n");
            break;

        case kDeviceRoleLeader:
            sResponse.Append("leader\r\n");
            break;
        }
    }
    else
    {
        if (strcmp(argv[0], "detached") == 0)
        {
            SuccessOrExit(otBecomeDetached());
        }
        else if (strcmp(argv[0], "child") == 0)
        {
            SuccessOrExit(otBecomeChild(kMleAttachSamePartition));
        }
        else if (strcmp(argv[0], "router") == 0)
        {
            SuccessOrExit(otBecomeRouter());
        }
        else if (strcmp(argv[0], "leader") == 0)
        {
            SuccessOrExit(otBecomeLeader());
        }
        else
        {
            ExitNow();
        }
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessStop(int argc, char *argv[])
{
    SuccessOrExit(otDisable());
    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessWhitelist(int argc, char *argv[])
{
    int argcur = 0;
    uint8_t extAddr[8];
    int8_t rssi;

    if (argcur >= argc)
    {
        ;
    }
    else if (strcmp(argv[argcur], "add") == 0)
    {
        VerifyOrExit(++argcur < argc, ;);
        VerifyOrExit(hex2bin(argv[argcur], extAddr, sizeof(extAddr)) == sizeof(extAddr), ;);

        if (++argcur < argc)
        {
            rssi = strtol(argv[argcur], NULL, 0);
            VerifyOrExit(otAddMacWhitelistRssi(extAddr, rssi) == kThreadError_None, ;);
        }
        else
        {
            otAddMacWhitelist(extAddr);
            VerifyOrExit(otAddMacWhitelist(extAddr) == kThreadError_None, ;);
        }
    }
    else if (strcmp(argv[argcur], "clear") == 0)
    {
        otClearMacWhitelist();
    }
    else if (strcmp(argv[argcur], "disable") == 0)
    {
        otDisableMacWhitelist();
    }
    else if (strcmp(argv[argcur], "enable") == 0)
    {
        otEnableMacWhitelist();
    }
    else if (strcmp(argv[argcur], "remove") == 0)
    {
        VerifyOrExit(++argcur < argc, ;);
        VerifyOrExit(hex2bin(argv[argcur], extAddr, sizeof(extAddr)) == sizeof(extAddr), ;);
        otRemoveMacWhitelist(extAddr);
    }

    sResponse.Append("Done\n");

exit:
    {}
}

void ProcessLine(char *aBuf, uint16_t aBufLength, Server &aServer)
{
    char *argv[kMaxArgs];
    char *cmd;
    int argc;
    char *last;

    sServer = &aServer;

    VerifyOrExit((cmd = strtok_r(aBuf, " ", &last)) != NULL, ;);

    for (argc = 0; argc < kMaxArgs; argc++)
    {
        if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL)
        {
            break;
        }
    }

    sResponse.Init();

    for (int i = 0; i < sizeof(sCommands) / sizeof(sCommands[0]); i++)
    {
        if (strcmp(cmd, sCommands[i].mName) == 0)
        {
            sCommands[i].mCommand(argc, argv);
            break;
        }
    }

    if (sResponse.GetResponseLength() > 0)
    {
        aServer.Output(sResponse.GetResponse(), sResponse.GetResponseLength());
    }

exit:
    {}
}

}  // namespace Cli
}  // namespace Thread
