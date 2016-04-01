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

#ifndef CLI_IP_H_
#define CLI_IP_H_

#include <cli/cli_command.h>
#include <net/netif.h>

namespace Thread {
namespace Cli {

class Ip: public Command
{
public:
    explicit Ip(Server &server);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    int PrintUsage(char *buf, uint16_t buf_length);
    ThreadError AddAddress(int argc, char *argv[]);
    ThreadError DeleteAddress(int argc, char *argv[]);

    NetifUnicastAddress m_address;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_IP_H_
