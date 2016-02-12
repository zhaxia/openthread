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

#ifndef CLI_CLI_NETDATA_H_
#define CLI_CLI_NETDATA_H_

#include <cli/cli_command.h>
#include <thread/thread_netif.h>

namespace Thread {

class CliNetData: public CliCommand {
 public:
  explicit CliNetData(CliServer *server, ThreadNetif *netif);
  const char *GetName() final;
  void Run(int argc, char *argv[], CliServer *server) final;

 private:
  enum {
    kMaxLocalOnMeshData = 4,
  };

  int PrintUsage(char *buf, uint16_t buf_length);
  int AddOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t buf_length);
  int RemoveOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t buf_length);
  int PrintLocalOnMeshPrefixes(char *buf, uint16_t buf_length);
  int PrintContextIdReuseDelay(char *buf, uint16_t buf_length);

  Mle *mle_;
  NetworkDataLocal *network_data_;
  NetworkDataLeader *network_data_leader_;
};

}  // namespace Thread

#endif  // CLI_CLI_NETDATA_H_
