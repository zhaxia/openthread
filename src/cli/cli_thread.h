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

#ifndef CLI_THREAD_H_
#define CLI_THREAD_H_

#include <cli/cli_command.h>
#include <thread/mle.h>
#include <thread/thread_netif.h>

namespace Thread {
namespace Cli {

class Thread: public Command
{
public:
    explicit Thread(Server &server, ThreadNetif &netif);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    int PrintUsage(char *buf, uint16_t buf_length);
    int PrintAddressCache(char *buf, uint16_t buf_length);
    int PrintChildren(char *buf, uint16_t buf_length);
    int PrintHoldTime(char *buf, uint16_t buf_length);
    int PrintKey(char *buf, uint16_t buf_length);
    int PrintKeySequence(char *buf, uint16_t buf_length);
    int PrintLeaderData(char *buf, uint16_t buf_length);
    int PrintMode(char *buf, uint16_t buf_length);
    int PrintRouters(char *buf, uint16_t buf_length);
    int PrintRoutes(char *buf, uint16_t buf_length);
    int PrintState(char *buf, uint16_t buf_length);

    Mle::MleRouter *m_mle;
    ThreadNetif *m_netif;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_THREAD_H_
