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

#ifndef CLI_ROUTE_H_
#define CLI_ROUTE_H_

#include <cli/cli_command.h>
#include <net/ip6.h>
#include <net/ip6_routes.h>

namespace Thread {
namespace Cli {

class Route: public Command
{
public:
    explicit Route(Server &server);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    int PrintUsage(char *buf, uint16_t buf_length);
    int AddRoute(int argc, char *argv[], char *buf, uint16_t buf_length);

    Ip6Route m_route;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_ROUTE_H_
