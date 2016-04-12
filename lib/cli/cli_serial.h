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

#ifndef CLI_SERIAL_H_
#define CLI_SERIAL_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>

namespace Thread {
namespace Cli {

class Command;

class Serial: public Server
{
public:
    Serial();
    ThreadError Start() final;
    ThreadError Output(const char *buf, uint16_t buf_length) final;

    void HandleReceive(uint8_t *buf, uint16_t buf_length);
    void HandleSendDone();

private:
    enum
    {
        kRxBufferSize = 128,
    };

    ThreadError ProcessCommand();

    char m_rx_buffer[kRxBufferSize];
    uint16_t m_rx_length;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_SERIAL_H_
