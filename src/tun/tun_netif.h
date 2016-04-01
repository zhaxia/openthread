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

#ifndef TUN_NETIF_H_
#define TUN_NETIF_H_

#include <pthread.h>
#include <semaphore.h>
#include <common/tasklet.h>
#include <net/netif.h>

namespace Thread {

class TunNetif: public Netif
{
public:
    TunNetif();
    ThreadError Start(uint8_t tunid);
    const char *GetName() const final;
    ThreadError GetLinkAddress(LinkAddress &address) const final;
    ThreadError SendMessage(Message &message) final;

private:
    static void *ReceiveThread(void *arg);
    static void ReceiveTask(void *context);
    void ReceiveTask();

    NetifUnicastAddress m_link_local;
    Tasklet m_receive_task;

    pthread_t m_pthread;
    sem_t *m_semaphore;
    int m_tunfd;
};

}  // namespace Thread

#endif  // TUN_NETIF_H_
