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

#include <common/code_utils.h>
#include <common/message.h>
#include <net/ip6.h>
#include <net/netif.h>
#include <tun/tun_netif.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

namespace Thread {

MessageQueue queue;
static const char name[] = "tun";

int ifconfig_call(const char *cmd) {
  int rval;
  rval = system(cmd);
  fprintf(stdout, "CMD=%s, rval=%d\n", cmd, rval);
  fflush(stdout);
  return rval;
}

TunNetif::TunNetif():
    receive_task_(&ReceiveTask, this) {
}

ThreadError TunNetif::Start(uint8_t tunid) {
  ThreadError error = kThreadError_None;
  char cmd[256];

  snprintf(cmd, sizeof(cmd), "/dev/tun%d", tunid);
  VerifyOrExit((tunfd_ = open(cmd, O_RDWR)) >= 0, error = kThreadError_Error);

  snprintf(cmd, sizeof(cmd),
           "ifconfig tun%d inet6 2001:dead:dead:dead::%d", tunid, tunid);
  ifconfig_call(cmd);

  snprintf(cmd, sizeof(cmd), "thread_tun_semaphore_%d", tunid);
  semaphore_ = sem_open(cmd, O_CREAT, 0644, 0);
  pthread_create(&thread_, NULL, ReceiveThread, this);

  // link-local
  memset(&link_local_, 0, sizeof(link_local_));
  link_local_.address.s6_addr[0] = 0xfe;
  link_local_.address.s6_addr[1] = 0x80;
  link_local_.address.s6_addr[15] = 0x01;
  link_local_.prefix_length = 64;
  link_local_.preferred_lifetime = 0xffffffff;
  link_local_.valid_lifetime = 0xffffffff;
  Netif::AddAddress(&link_local_);
  Netif::AddNetif();

exit:
  return error;
}

const char *TunNetif::GetName() const {
  return name;
}

ThreadError TunNetif::GetLinkAddress(LinkAddress *address) const {
  return kThreadError_Error;
}

ThreadError TunNetif::SendMessage(Message *message) {
  uint8_t buf[1500];

  message->Read(0, message->GetLength(), buf);

  write(tunfd_, buf, message->GetLength());
  Message::Free(message);

  return kThreadError_None;
}

void *TunNetif::ReceiveThread(void *arg) {
  TunNetif *tun = reinterpret_cast<TunNetif*>(arg);

  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(tun->tunfd_, &fds);

    int rval = select(tun->tunfd_ + 1, &fds, NULL, NULL, NULL);
    if (rval >= 0 && FD_ISSET(tun->tunfd_, &fds)) {
      tun->receive_task_.Post();
      sem_wait(tun->semaphore_);
    }
  }

  return NULL;
}

void TunNetif::ReceiveTask(void *context) {
  TunNetif *obj = reinterpret_cast<TunNetif*>(context);
  obj->ReceiveTask();
}

void TunNetif::ReceiveTask() {
  int len;

  len = read(tunfd_, receive_buffer_, sizeof(receive_buffer_));

  Message *message = Message::New(Message::kTypeIp6, 0);
  VerifyOrExit(message != NULL, ;);

  SuccessOrExit(message->SetLength(len));
  message->Write(0, len, receive_buffer_);

  Ip6::HandleDatagram(message, this, GetInterfaceId(), NULL, false);

exit:
  sem_post(semaphore_);
}

}  // namespace Thread
