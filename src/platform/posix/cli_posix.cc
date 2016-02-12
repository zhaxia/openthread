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

#include <platform/posix/cli_posix.h>
#include <platform/posix/cmdline.h>
#include <cli/cli_command.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

using Thread::Encoding::BigEndian::HostSwap16;

extern struct gengetopt_args_info args_info;

namespace Thread {

CliServerPosix::CliServerPosix() :
    received_task_(&ReceivedTask, this) {
}

ThreadError CliServerPosix::Add(CliCommand *command) {
  ThreadError error = kThreadError_None;
  CliCommand *cur_command;

  VerifyOrExit(command->next_ == NULL, error = kThreadError_Busy);

  if (commands_ == NULL) {
    commands_ = command;
  } else {
    for (cur_command = commands_; cur_command->next_; cur_command = cur_command->next_) {}
    cur_command->next_ = command;
  }

exit:
  return error;
}

ThreadError CliServerPosix::Start(uint16_t port) {
  struct sockaddr_in sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(8000 + args_info.eui64_arg);
  sockaddr.sin_addr.s_addr = INADDR_ANY;

  sockfd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  bind(sockfd_, (struct sockaddr*)&sockaddr, sizeof(sockaddr));

  pthread_create(&thread_, NULL, ReceiveThread, this);

  return kThreadError_None;
}

void *CliServerPosix::ReceiveThread(void *arg) {
  CliServerPosix *obj = reinterpret_cast<CliServerPosix*>(arg);
  return obj->ReceiveThread();
}

void *CliServerPosix::ReceiveThread() {
  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfd_, &fds);

    int rval = select(sockfd_ + 1, &fds, NULL, NULL, NULL);
    if (rval >= 0 && FD_ISSET(sockfd_, &fds)) {
      pthread_mutex_lock(&mutex_);
      received_task_.Post();
      pthread_cond_wait(&condition_variable_, &mutex_);
      pthread_mutex_unlock(&mutex_);
    }
  }

  return NULL;
}

void CliServerPosix::ReceivedTask(void *context) {
  CliServerPosix *obj = reinterpret_cast<CliServerPosix*>(context);
  obj->ReceivedTask();
}

void CliServerPosix::ReceivedTask() {
  char buf[1024];
  int length;
  char *cmd;
  char *last;

  pthread_mutex_lock(&mutex_);
  socklen_ = sizeof(sockaddr_);
  length = recvfrom(sockfd_, buf, sizeof(buf), 0, &sockaddr_, &socklen_);
  pthread_mutex_unlock(&mutex_);

  if (buf[length-1] == '\n')
    buf[--length] = '\0';
  if (buf[length-1] == '\r')
    buf[--length] = '\0';

  VerifyOrExit((cmd = strtok_r(buf, " ", &last)) != NULL, ;);

  if (strncmp(cmd, "?", 1) == 0) {
    char *cur = buf;
    char *end = cur + sizeof(buf);

    snprintf(cur, end - cur, "%s", "Commands:\r\n");
    cur += strlen(cur);
    for (CliCommand *command = commands_; command; command = command->next_) {
      snprintf(cur, end - cur, "%s\r\n", command->GetName());
      cur += strlen(cur);
    }

    snprintf(cur, end - cur, "Done\r\n");
    cur += strlen(cur);

    sendto(sockfd_, buf, cur - buf, 0, (struct sockaddr*)&sockaddr_, sizeof(sockaddr_));
  } else {
    int argc;
    char *argv[kMaxArgs];

    for (argc = 0; argc < kMaxArgs; argc++) {
      if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL)
        break;
    }

    for (CliCommand *command = commands_; command; command = command->next_) {
      if (strcmp(cmd, command->GetName()) == 0) {
        command->Run(argc, argv, this);
        break;
      }
    }
  }

exit:
  pthread_cond_signal(&condition_variable_);
}

ThreadError CliServerPosix::Output(const char *buf, uint16_t buf_length) {
  pthread_mutex_lock(&mutex_);
  sendto(sockfd_, buf, buf_length, 0, (struct sockaddr*)&sockaddr_, sizeof(sockaddr_));
  pthread_mutex_unlock(&mutex_);
  return kThreadError_None;
}

}  // namespace Thread
