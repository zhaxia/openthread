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
#include <common/code_utils.h>

namespace Thread {
namespace Cli {

ThreadError Server::Add(Command &command)
{
    ThreadError error = kThreadError_None;
    Command *cur;

    for (cur = m_commands; cur; cur = cur->GetNext())
    {
        if (cur == &command)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    if (m_commands == NULL)
    {
        m_commands = &command;
    }
    else
    {
        for (cur = m_commands; cur->GetNext(); cur = cur->GetNext()) {}

        cur->SetNext(command);
    }

exit:
    return error;
}

}  // namespace Cli
}  // namespace Thread
