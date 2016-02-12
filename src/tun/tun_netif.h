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

#ifndef TUN_TUN_NETIF_H_
#define TUN_TUN_NETIF_H_

#include <common/tasklet.h>
#include <net/netif.h>
#include <pthread.h>
#include <semaphore.h>

namespace Thread {

class TunNetif: public Netif {
 public:
  TunNetif();
  ThreadError Start(uint8_t tunid);
  const char *GetName() const final;
  ThreadError GetLinkAddress(LinkAddress *address) const final;
  ThreadError SendMessage(Message *message) final;

 private:
  NetifAddress link_local_;

  int tunfd_;
  pthread_t thread_;
  sem_t *semaphore_;

  uint8_t receive_buffer_[2048];

  static void ReceiveTask(void *context);
  void ReceiveTask();
  Tasklet receive_task_;

  static void *ReceiveThread(void *arg);
};

}  // namespace Thread

#endif  // TUN_TUN_NETIF_H_
