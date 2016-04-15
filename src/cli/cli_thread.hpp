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

#ifndef CLI_THREAD_HPP_
#define CLI_THREAD_HPP_

#include <cli/cli_command.hpp>
#include <thread/mle.hpp>
#include <thread/thread_netif.hpp>

namespace Thread {
namespace Cli {

class Thread: public Command
{
public:
    explicit Thread(Server &server, ThreadNetif &netif);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    int PrintUsage(char *buf, uint16_t bufLength);
    int PrintAddressCache(char *buf, uint16_t bufLength);
    int PrintChildren(char *buf, uint16_t bufLength);
    int PrintHoldTime(char *buf, uint16_t bufLength);
    int PrintKey(char *buf, uint16_t bufLength);
    int PrintKeySequence(char *buf, uint16_t bufLength);
    int PrintLeaderData(char *buf, uint16_t bufLength);
    int PrintMode(char *buf, uint16_t bufLength);
    int PrintRouters(char *buf, uint16_t bufLength);
    int PrintRoutes(char *buf, uint16_t bufLength);
    int PrintState(char *buf, uint16_t bufLength);

    Mle::MleRouter *mMle;
    ThreadNetif *mNetif;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_THREAD_HPP_
