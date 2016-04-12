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

#ifndef CLI_PING_H_
#define CLI_PING_H_

#include <cli/cli_command.h>
#include <net/icmp6.h>

namespace Thread {
namespace Cli {

class Ping: public Command
{
public:
    explicit Ping(Server &server);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    static void HandleEchoResponse(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleEchoResponse(Message &message, const Ip6MessageInfo &message_info);

    int PrintUsage(char *buf, uint16_t buf_length);
    void EchoRequest();

    sockaddr_in6 m_sockaddr;
    Server *m_server;
    Icmp6Echo m_icmp6_echo;
    uint16_t m_length = 0;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_PING_H_
