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

#ifndef CLI_CLI_PING_H_
#define CLI_CLI_PING_H_

#include <cli/cli_command.h>
#include <net/icmp6.h>

namespace Thread {

class CliPing: public CliCommand {
 public:
  explicit CliPing(CliServer *cli_server);
  const char *GetName() final;
  void Run(int argc, char *argv[], CliServer *server) final;

 private:
  static void HandleEchoResponse(void *context, Message *message, const Ip6MessageInfo *message_info);
  void HandleEchoResponse(Message *message, const Ip6MessageInfo *message_info);

  int PrintUsage(char *buf, uint16_t buf_length);
  void EchoRequest();

  Icmp6::EchoClient echo_client_;
  uint16_t length_ = 0;
};

}  // namespace Thread

#endif  // CLI_CLI_PING_H_
