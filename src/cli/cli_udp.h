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

#ifndef CLI_CLI_UDP_H_
#define CLI_CLI_UDP_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>
#include <common/message.h>
#include <net/socket.h>
#include <net/udp6.h>

namespace Thread {

class CliCommand;

class CliServerUdp: public CliServer {
 public:
  CliServerUdp();
  ThreadError Add(CliCommand *command) final;
  ThreadError Start(uint16_t port) final;
  ThreadError Output(const char *buf, uint16_t buf_length) final;

 private:
  static void RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info);
  void RecvFrom(Message *message, const Ip6MessageInfo *message_info);

  Udp6Socket socket_;
  Ip6MessageInfo peer_;
};

}  // namespace Thread

#endif  // CLI_CLI_UDP_H_
