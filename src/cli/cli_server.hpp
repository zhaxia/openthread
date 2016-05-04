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
 *   This file contains definitions for adding a CLI command to the CLI server.
 */

#ifndef CLI_SERVER_HPP_
#define CLI_SERVER_HPP_

#include <common/message.hpp>

namespace Thread {
namespace Cli {

/**
 * This class implements the CLI server.
 *
 */
class Server
{
public:
    /**
     * This method starts the CLI server.
     *
     * @retval kThreadError_None  Successfully started the server.
     *
     */
    virtual ThreadError Start() = 0;

    /**
     * This method delivers output to the client.
     *
     * @param[in]  aBuf        A pointer to a buffer.
     * @param[in]  aBufLength  Number of bytes in the buffer.
     *
     * @retval kThreadError_None  Successfully delivered output the client.
     *
     */
    virtual ThreadError Output(const char *aBuf, uint16_t aBufLength) = 0;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_SERVER_HPP_
