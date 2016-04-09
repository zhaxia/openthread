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

#ifndef CLI_IFCONFIG_H_
#define CLI_IFCONFIG_H_

#include <cli/cli_command.h>

namespace Thread {
namespace Cli {

class Ifconfig: public Command
{
public:
    explicit Ifconfig(Server &server);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_IFCONFIG_H_
