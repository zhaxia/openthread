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

#ifndef CLI_NETDATA_H_
#define CLI_NETDATA_H_

#include <cli/cli_command.h>
#include <thread/thread_netif.h>

namespace Thread {
namespace Cli {

class NetData: public Command
{
public:
    explicit NetData(Server &server, ThreadNetif &netif);
    const char *GetName() final;
    void Run(int argc, char *argv[], Server &server) final;

private:
    enum
    {
        kMaxLocalOnMeshData = 4,
    };

    int PrintUsage(char *buf, uint16_t buf_length);
    int AddHasRoutePrefix(int argc, char *argv[], char *buf, uint16_t buf_length);
    int RemoveHasRoutePrefix(int argc, char *argv[], char *buf, uint16_t buf_length);
    int AddOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t buf_length);
    int RemoveOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t buf_length);
    int PrintLocalHasRoutePrefixes(char *buf, uint16_t buf_length);
    int PrintLocalOnMeshPrefixes(char *buf, uint16_t buf_length);
    int PrintContextIdReuseDelay(char *buf, uint16_t buf_length);

    Mle::MleRouter *m_mle;
    NetworkData::Local *m_network_data_local;
    NetworkData::Leader *m_network_data_leader;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_NETDATA_H_
