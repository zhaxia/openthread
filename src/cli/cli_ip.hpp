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
 *   This file contains definitions for the CLI commands that configure and manage IPv6 addresses.
 */

#ifndef CLI_IP_HPP_
#define CLI_IP_HPP_

#include <openthread.h>
#include <cli/cli_command.hpp>

namespace Thread {
namespace Cli {

class Ip: public Command
{
public:
    explicit Ip(Server &server);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    int PrintUsage(char *buf, uint16_t bufLength);
    ThreadError AddAddress(int argc, char *argv[]);
    ThreadError DeleteAddress(int argc, char *argv[]);

    struct otNetifAddress mAddress;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_IP_HPP_
