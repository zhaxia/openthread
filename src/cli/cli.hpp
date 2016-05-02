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
 *   This file contains definitions for the CLI interpreter.
 */

#ifndef CLI_HPP_
#define CLI_HPP_

#include <stdarg.h>

#include <cli/cli_server.hpp>
#include <net/icmp6.hpp>

namespace Thread {

/**
 * @namespace Thread::Cli
 *
 * @brief
 *   This namespace contains definitions for the CLI interpreter.
 *
 */
namespace Cli {

/**
 * This class implements the response buffer for CLI command results.
 *
 */
class ResponseBuffer
{
public:
    /**
     * This method initializes the response buffer.
     *
     */
    void Init(void) { mEnd = mBuffer; }

    /**
     * This method appends output according to the format string.
     *
     * @param[in]  fmt  A pointer to the NULL-terminated format string.
     * @param[in]  ...  Arguments for the format specification.
     *
     */
    void Append(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(mEnd, sizeof(mBuffer) - (mEnd - mBuffer), fmt, ap);
        va_end(ap);
        mEnd += strlen(mEnd);
    }

    /**
     * This method returns a pointer to the response string.
     *
     * @returns A pointer to the response string.
     *
     */
    const char *GetResponse(void) { return mBuffer; }

    /**
     * This method returns the length of the response string.
     *
     * @returns The length to the response string.
     *
     */
    uint16_t GetResponseLength(void) { return mEnd - mBuffer; }

private:
    char mBuffer[512];
    char *mEnd;
};

/**
 * This structure represents a CLI command.
 *
 */
struct Command
{
    const char *mName;                         ///< A pointer to the command string.
    void (*mCommand)(int argc, char *argv[]);  ///< A function pointer to process the command.
};

/**
 * This class implements the CLI interpreter.
 *
 */
class Interpreter
{
public:
    /**
     * This method interprets a CLI command.
     *
     * @param[in]  aBuf        A pointer to a string.
     * @param[in]  aBufLength  The length of the string in bytes.
     * @param[in]  aServer     A reference to the CLI server.
     *
     */
    static void ProcessLine(char *aBuf, uint16_t aBufLength, Server &aServer);

private:
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
    static ThreadError ProcessIpAddrAdd(int argc, char *argv[]);
    static ThreadError ProcessIpAddrDel(int argc, char *argv[]);
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
    static ThreadError ProcessPrefixAdd(int argc, char *argv[]);
    static ThreadError ProcessPrefixRemove(int argc, char *argv[]);
    static void ProcessReleaseRouterId(int argc, char *argv[]);
    static void ProcessRoute(int argc, char *argv[]);
    static ThreadError ProcessRouteAdd(int argc, char *argv[]);
    static ThreadError ProcessRouteRemove(int argc, char *argv[]);
    static void ProcessRouterUpgradeThreshold(int argc, char *argv[]);
    static void ProcessRloc16(int argc, char *argv[]);
    static void ProcessShutdown(int argc, char *argv[]);
    static void ProcessStart(int argc, char *argv[]);
    static void ProcessState(int argc, char *argv[]);
    static void ProcessStop(int argc, char *argv[]);
    static void ProcessWhitelist(int argc, char *argv[]);

    static void HandleEchoResponse(void *aContext, Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    static int Hex2Bin(const char *aHex, uint8_t *aBin, uint16_t aBinLength);
    static ThreadError ParseLong(char *argv, long &value);

    static const struct Command sCommands[];
    static ResponseBuffer sResponse;

    static otNetifAddress sAddress;

    static Ip6::SockAddr sSockAddr;
    static Ip6::IcmpEcho sIcmpEcho;
    static Server *sServer;
    static uint8_t sEchoRequest[];
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_HPP_
