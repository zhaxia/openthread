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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#ifndef CLI_TEST_H_
#define CLI_TEST_H_

#include <cli/cli_command.h>
#include <common/timer.h>

namespace Thread {
namespace Cli {

class Test: public Command
{
public:
    explicit Test(Server &server);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    int PrintUsage(char *buf, uint16_t buf_length);

    int TestTimer(char *buf, uint16_t buf_length);
    int TestPhyTx(char *buf, uint16_t buf_length);

    static void HandleTimer(void *context);
    void HandleTimer();
    Timer m_timer;

    Server *m_server;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_TEST_H_
