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

#ifndef CLI_MAC_H_
#define CLI_MAC_H_

#include <cli/cli_command.h>
#include <thread/thread_netif.h>

namespace Thread {
namespace Cli {

class Mac: public Command
{
public:
    explicit Mac(Server &server, ThreadNetif &netif);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    static void HandleActiveScanResult(void *context, Thread::Mac::ActiveScanResult *result);
    void HandleActiveScanResult(Thread::Mac::ActiveScanResult *result);

    int PrintUsage(char *buf, uint16_t buf_length);
    int PrintWhitelist(char *buf, uint16_t buf_length);
    int ProcessWhitelist(int argc, char *argv[], char *buf, uint16_t buf_length);

    Server *m_server;
    Thread::Mac::Mac *m_mac;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_MAC_H_
