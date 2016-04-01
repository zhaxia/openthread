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

int ifconfig_call(const char *cmd)
{
    int rval;
    rval = system(cmd);
    fprintf(stdout, "CMD=%s, rval=%d\n", cmd, rval);
    fflush(stdout);
    return rval;
}

TunNetif::TunNetif():
    m_receive_task(&ReceiveTask, this)
{
}

ThreadError TunNetif::Start(uint8_t tunid)
{
    ThreadError error = kThreadError_None;
    char cmd[256];

    snprintf(cmd, sizeof(cmd), "/dev/tun%d", tunid);
    VerifyOrExit((m_tunfd = open(cmd, O_RDWR)) >= 0, error = kThreadError_Error);

    snprintf(cmd, sizeof(cmd),
             "ifconfig tun%d inet6 2001:dead:dead:dead::%d", tunid, tunid);
    ifconfig_call(cmd);

    snprintf(cmd, sizeof(cmd), "thread_tun_semaphore_%d", tunid);
    m_semaphore = sem_open(cmd, O_CREAT, 0644, 0);
    pthread_create(&m_pthread, NULL, ReceiveThread, this);

    // link-local
    memset(&m_link_local, 0, sizeof(m_link_local));
    m_link_local.address.s6_addr[0] = 0xfe;
    m_link_local.address.s6_addr[1] = 0x80;
    m_link_local.address.s6_addr[15] = 0x01;
    m_link_local.prefix_length = 64;
    m_link_local.preferred_lifetime = 0xffffffff;
    m_link_local.valid_lifetime = 0xffffffff;
    Netif::AddUnicastAddress(m_link_local);
    Netif::AddNetif();

exit:
    return error;
}

const char *TunNetif::GetName() const
{
    return name;
}

ThreadError TunNetif::GetLinkAddress(LinkAddress &address) const
{
    return kThreadError_Error;
}

ThreadError TunNetif::SendMessage(Message &message)
{
    uint8_t buf[1500];

    message.Read(0, message.GetLength(), buf);

    write(m_tunfd, buf, message.GetLength());
    Message::Free(message);

    return kThreadError_None;
}

void *TunNetif::ReceiveThread(void *arg)
{
    TunNetif *tun = reinterpret_cast<TunNetif *>(arg);
    fd_set fds;
    int rval;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(tun->m_tunfd, &fds);

        rval = select(tun->m_tunfd + 1, &fds, NULL, NULL, NULL);

        if (rval >= 0 && FD_ISSET(tun->m_tunfd, &fds))
        {
            tun->m_receive_task.Post();
            sem_wait(tun->m_semaphore);
        }
    }

    return NULL;
}

void TunNetif::ReceiveTask(void *context)
{
    TunNetif *obj = reinterpret_cast<TunNetif *>(context);
    obj->ReceiveTask();
}

void TunNetif::ReceiveTask()
{
    uint8_t buf[2048];
    Message *message;
    int len;

    len = read(m_tunfd, buf, sizeof(buf));

    message = Message::New(Message::kTypeIp6, 0);
    VerifyOrExit(message != NULL, ;);

    SuccessOrExit(message->SetLength(len));
    message->Write(0, len, buf);

    Ip6::HandleDatagram(*message, this, GetInterfaceId(), NULL, false);

exit:
    sem_post(m_semaphore);
}

}  // namespace Thread
