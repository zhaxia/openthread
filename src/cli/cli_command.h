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

#ifndef CLI_COMMAND_H_
#define CLI_COMMAND_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>

namespace Thread {
namespace Cli {

class Command
{
public:
    explicit Command(Server &server);
    virtual const char *GetName() = 0;
    virtual void Run(int argc, char *argv[], Server &server) = 0;

    Command *GetNext() const { return m_next; }
    void SetNext(Command &command) { m_next = &command; }

private:
    Command *m_next = NULL;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_COMMAND_H_
