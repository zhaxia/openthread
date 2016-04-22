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
 *   This file contains definitions for a CLI server on the serial service.
 */

#ifndef CLI_SERIAL_HPP_
#define CLI_SERIAL_HPP_

#include <cli/cli_server.hpp>
#include <common/thread_error.hpp>

namespace Thread {
namespace Cli {

class Command;

class Serial: public Server
{
public:
    Serial();
    ThreadError Start() final;
    ThreadError Output(const char *buf, uint16_t bufLength) final;

    void HandleReceive(uint8_t *buf, uint16_t bufLength);
    void HandleSendDone();

    static void ReceiveTask(void *context);

private:
    enum
    {
        kRxBufferSize = 128,
    };

    ThreadError ProcessCommand();
    void ReceiveTask();

    char mRxBuffer[kRxBufferSize];
    uint16_t mRxLength;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_SERIAL_HPP_
