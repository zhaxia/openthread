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

#include <openthread-types.h>
#include <cli/cli_server.hpp>
#include <common/tasklet.hpp>

namespace Thread {
namespace Cli {

/**
 * This class implements the CLI server on top of the serial platform abstraction.
 *
 */
class Serial: public Server
{
public:
    Serial(void);

    /**
     * This method starts the CLI server.
     *
     * @retval kThreadError_None  Successfully started the server.
     *
     */
    ThreadError Start(void);

    /**
     * This method delivers output to the client.
     *
     * @param[in]  aBuf        A pointer to a buffer.
     * @param[in]  aBufLength  Number of bytes in the buffer.
     *
     * @retval kThreadError_None  Successfully delivered output the client.
     *
     */
    ThreadError Output(const char *aBuf, uint16_t aBufLength);

    static Tasklet sReceiveTask;

private:
    enum
    {
        kRxBufferSize = 128,
    };

    static void ReceiveTask(void *aContext);

    ThreadError ProcessCommand(void);
    void ReceiveTask(void);

    char mRxBuffer[kRxBufferSize];
    uint16_t mRxLength;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_SERIAL_HPP_
