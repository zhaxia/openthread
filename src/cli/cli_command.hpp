/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 * @file
 *   This file contains definitions for the CLI commands.
 */

#ifndef CLI_COMMAND_HPP_
#define CLI_COMMAND_HPP_

#include <cli/cli_server.hpp>
#include <common/thread_error.hpp>

namespace Thread {
namespace Cli {

class Command
{
public:
    explicit Command(Server &server);
    virtual const char *GetName() = 0;
    virtual void Run(int argc, char *argv[], Server &server) = 0;

    Command *GetNext() const { return mNext; }
    void SetNext(Command &command) { mNext = &command; }

private:
    Command *mNext = NULL;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_COMMAND_HPP_
