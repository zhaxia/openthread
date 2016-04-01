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

#include <cli/cli_command.h>
#include <cli/cli_udp.h>
#include <common/code_utils.h>
#include <stdio.h>
#include <string.h>

namespace Thread {
namespace Cli {

Udp::Udp():
    m_socket(&HandleUdpReceive, this)
{
}

ThreadError Udp::Start()
{
    struct sockaddr_in6 sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin6_port = 7335;

    return m_socket.Bind(&sockaddr);
}

void Udp::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    Udp *obj = reinterpret_cast<Udp *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void Udp::HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    uint16_t payload_length = message.GetLength() - message.GetOffset();
    Message *reply;
    char buf[512];
    char *cmd;
    char *last;

    VerifyOrExit(payload_length <= sizeof(buf), ;);
    message.Read(message.GetOffset(), payload_length, buf);

    if (buf[payload_length - 1] == '\n')
    {
        buf[--payload_length] = '\0';
    }

    if (buf[payload_length - 1] == '\r')
    {
        buf[--payload_length] = '\0';
    }

    VerifyOrExit((cmd = strtok_r(buf, " ", &last)) != NULL, ;);

    m_peer = message_info;

    if (strncmp(cmd, "?", 1) == 0)
    {
        char *cur = buf;
        char *end = cur + sizeof(buf);

        snprintf(cur, end - cur, "%s", "Commands:\r\n");
        cur += strlen(cur);

        for (Command *command = m_commands; command; command = command->GetNext())
        {
            snprintf(cur, end - cur, "%s\r\n", command->GetName());
            cur += strlen(cur);
        }

        VerifyOrExit((reply = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
        reply->Append(buf, cur - buf);

        SuccessOrExit(error = m_socket.SendTo(*reply, m_peer));
    }
    else
    {
        int argc;
        char *argv[kMaxArgs];

        for (argc = 0; argc < kMaxArgs; argc++)
        {
            if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL)
            {
                break;
            }
        }

        for (Command *command = m_commands; command; command = command->GetNext())
        {
            if (strcmp(cmd, command->GetName()) == 0)
            {
                command->Run(argc, argv, *this);
                break;
            }
        }
    }

exit:

    if (error != kThreadError_None && reply != NULL)
    {
        Message::Free(*reply);
    }
}

ThreadError Udp::Output(const char *buf, uint16_t buf_length)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(buf_length));
    message->Write(0, buf_length, buf);
    SuccessOrExit(error = m_socket.SendTo(*message, m_peer));

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

}  // namespace Cli
}  // namespace Thread
