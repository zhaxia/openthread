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

#ifndef CLI_SERVER_H_
#define CLI_SERVER_H_

#include <common/thread_error.h>
#include <common/message.h>

namespace Thread {
namespace Cli {

class Command;

class Server
{
public:
    ThreadError Add(Command &command);
    virtual ThreadError Start() = 0;
    virtual ThreadError Output(const char *buf, uint16_t buf_length) = 0;

protected:
    enum
    {
        kMaxArgs = 8,
    };
    Command *m_commands = NULL;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_SERVER_H_
