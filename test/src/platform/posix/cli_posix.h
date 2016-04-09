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

#ifndef CLI_POSIX_H_
#define CLI_POSIX_H_

#include <cli/cli_server.h>
#include <common/tasklet.h>
#include <common/thread_error.h>
#include <sys/socket.h>
#include <pthread.h>

namespace Thread {
namespace Cli {

class Command;

class Socket: public Server
{
public:
    Socket();
    ThreadError Start() final;
    ThreadError Output(const char *buf, uint16_t buf_length) final;

private:
    static void *ReceiveThread(void *arg);
    void *ReceiveThread();

    static void ReceivedTask(void *context);
    void ReceivedTask();

    Tasklet m_received_task;

    pthread_t m_pthread;
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t m_condition_variable = PTHREAD_COND_INITIALIZER;
    int m_sockfd;
    struct sockaddr m_sockaddr;
    socklen_t m_socklen;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_POSIX_H_
