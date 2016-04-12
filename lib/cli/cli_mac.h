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

#ifndef CLI_MAC_H_
#define CLI_MAC_H_

#include <cli/cli_command.h>
#include <thread/thread_netif.h>

namespace Thread {
namespace Cli {

class Mac: public Command
{
public:
    explicit Mac(Server &server, ThreadNetif &netif);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    static void HandleActiveScanResult(void *context, Thread::Mac::ActiveScanResult *result);
    void HandleActiveScanResult(Thread::Mac::ActiveScanResult *result);

    int PrintUsage(char *buf, uint16_t buf_length);
    int PrintWhitelist(char *buf, uint16_t buf_length);
    int ProcessWhitelist(int argc, char *argv[], char *buf, uint16_t buf_length);

    Server *m_server;
    Thread::Mac::Mac *m_mac;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_MAC_H_
