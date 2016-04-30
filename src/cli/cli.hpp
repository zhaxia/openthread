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
 *   This file contains definitions for the CLI interpreter.
 */

#ifndef CLI_HPP_
#define CLI_HPP_

#include <cli/cli_server.hpp>

namespace Thread {

/**
 * @namespace Cli
 *
 * @brief
 *   This namespace contains definitions for the CLI interpreter.
 *
 */
namespace Cli {

/**
 * This method interprets a CLI command.
 *
 * @param[in]  aBuf        A pointer to a string.
 * @param[in]  aBufLength  The length of the string in bytes.
 * @param[in]  aServer     A reference to the CLI server.
 *
 */
void ProcessLine(char *aBuf, uint16_t aBufLength, Server &aServer);

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_HPP_
