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

#ifndef CLI_UDP_HPP_
#define CLI_UDP_HPP_

#include <cli/cli_server.hpp>
#include <common/thread_error.hpp>
#include <common/message.hpp>
#include <net/socket.hpp>
#include <net/udp6.hpp>

namespace Thread {
namespace Cli {

class Command;

class Udp: public Server
{
public:
    Udp();
    ThreadError Start() final;
    ThreadError Output(const char *buf, uint16_t buf_length) final;

private:
    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);

    Udp6Socket m_socket;
    Ip6MessageInfo m_peer;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_UDP_HPP_
