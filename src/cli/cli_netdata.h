/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
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
