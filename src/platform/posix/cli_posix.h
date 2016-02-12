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

#ifndef PLATFORM_POSIX_CLI_POSIX_H_
#define PLATFORM_POSIX_CLI_POSIX_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>
#include <sys/socket.h>
#include <pthread.h>

namespace Thread {

class CliCommand;

class CliServerPosix: public CliServer {
 public:
  CliServerPosix();
  ThreadError Add(CliCommand *command) final;
  ThreadError Start(uint16_t port) final;
  ThreadError Output(const char *buf, uint16_t buf_length) final;

 private:
  static void *ReceiveThread(void *arg);
  void *ReceiveThread();

  static void ReceivedTask(void *context);
  void ReceivedTask();

  Tasklet received_task_;

  pthread_t thread_;
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condition_variable_ = PTHREAD_COND_INITIALIZER;
  int sockfd_;
  struct sockaddr sockaddr_;
  socklen_t socklen_;
};

}  // namespace Thread

#endif  // PLATFORM_POSIX_CLI_POSIX_H_
