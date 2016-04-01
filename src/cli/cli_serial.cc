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
#include <cli/cli_serial.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <platform/common/uart.h>
#include <stdio.h>
#include <string.h>

namespace Thread {
namespace Cli {

static const uint8_t VT102_ERASE_EOL[] = "\033[K";
static const uint8_t CRNL[] = "\r\n";
static Serial *m_server;

Serial::Serial()
{
    m_server = this;
}

ThreadError Serial::Start()
{
    m_rx_length = 0;
    uart_start();
    return kThreadError_None;
}

extern "C" void uart_handle_receive(uint8_t *buf, uint16_t buf_length)
{
    m_server->HandleReceive(buf, buf_length);
}

void Serial::HandleReceive(uint8_t *buf, uint16_t buf_length)
{
    uint8_t *cur = buf;
    uint8_t *end = cur + buf_length;

    for (; cur < end; cur++)
    {
        switch (*cur)
        {
        case '\r':
            uart_send(CRNL, sizeof(CRNL));
            break;

        default:
            uart_send(cur, 1);
            break;
        }
    }

    while (buf_length > 0 && m_rx_length < kRxBufferSize)
    {
        switch (*buf)
        {
        case '\r':
        case '\n':
            if (m_rx_length > 0)
            {
                m_rx_buffer[m_rx_length] = '\0';
                ProcessCommand();
            }

            break;

        case '\b':
        case 127:
            if (m_rx_length > 0)
            {
                m_rx_buffer[--m_rx_length] = '\0';
                uart_send(VT102_ERASE_EOL, sizeof(VT102_ERASE_EOL));
            }

            break;

        default:
            m_rx_buffer[m_rx_length++] = *buf;
            break;
        }

        buf++;
        buf_length--;
    }
}

extern "C" void uart_handle_send_done()
{
    m_server->HandleSendDone();
}

void Serial::HandleSendDone()
{
}

ThreadError Serial::ProcessCommand()
{
    ThreadError error = kThreadError_None;
    uint16_t payload_length = m_rx_length;
    char *cmd;
    char *last;
    char *cur;
    char *end;
    int argc;
    char *argv[kMaxArgs];

    if (m_rx_buffer[payload_length - 1] == '\n')
    {
        m_rx_buffer[--payload_length] = '\0';
    }

    if (m_rx_buffer[payload_length - 1] == '\r')
    {
        m_rx_buffer[--payload_length] = '\0';
    }

    VerifyOrExit((cmd = strtok_r(m_rx_buffer, " ", &last)) != NULL, ;);

    if (strncmp(cmd, "?", 1) == 0)
    {
        cur = m_rx_buffer;
        end = cur + sizeof(m_rx_buffer);

        snprintf(cur, end - cur, "%s", "Commands:\r\n");
        cur += strlen(cur);

        for (Command *command = m_commands; command; command = command->GetNext())
        {
            snprintf(cur, end - cur, "%s\r\n", command->GetName());
            cur += strlen(cur);
        }

        SuccessOrExit(error = uart_send(reinterpret_cast<const uint8_t *>(m_rx_buffer), cur - m_rx_buffer));
    }
    else
    {
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
    m_rx_length = 0;

    return error;
}

ThreadError Serial::Output(const char *buf, uint16_t buf_length)
{
    return uart_send(reinterpret_cast<const uint8_t *>(buf), buf_length);
}

}  // namespace Cli
}  // namespace Thread
