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

#ifndef CLI_UDP_H_
#define CLI_UDP_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>
#include <common/message.h>
#include <net/socket.h>
#include <net/udp6.h>

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

#endif  // CLI_UDP_H_
