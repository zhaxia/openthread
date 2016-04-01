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

#ifndef CLI_SERIAL_H_
#define CLI_SERIAL_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>

namespace Thread {
namespace Cli {

class Command;

class Serial: public Server
{
public:
    Serial();
    ThreadError Start() final;
    ThreadError Output(const char *buf, uint16_t buf_length) final;

    void HandleReceive(uint8_t *buf, uint16_t buf_length);
    void HandleSendDone();

private:
    enum
    {
        kRxBufferSize = 128,
    };

    ThreadError ProcessCommand();

    char m_rx_buffer[kRxBufferSize];
    uint16_t m_rx_length;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_SERIAL_H_
