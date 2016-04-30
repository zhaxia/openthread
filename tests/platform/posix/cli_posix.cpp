/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#include <platform/posix/cmdline.h>

#include <platform/posix/cli_posix.hpp>
#include <cli/cli.hpp>
#include <common/code_utils.hpp>

extern struct gengetopt_args_info args_info;

namespace Thread {
namespace Cli {

Socket::Socket():
    mReceivedTask(&ReceivedTask, this)
{
    mMutex = PTHREAD_MUTEX_INITIALIZER;
    mConditionVariable = PTHREAD_COND_INITIALIZER;
}

ThreadError Socket::Start()
{
    struct sockaddr_in sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(8000 + args_info.nodeid_arg);
    sockaddr.sin_addr.s_addr = INADDR_ANY;

    mSockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(mSockFd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));

    pthread_create(&mPthread, NULL, ReceiveThread, this);

    return kThreadError_None;
}

void *Socket::ReceiveThread(void *arg)
{
    Socket *obj = reinterpret_cast<Socket *>(arg);
    return obj->ReceiveThread();
}

void *Socket::ReceiveThread()
{
    fd_set fds;
    int rval;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(mSockFd, &fds);

        rval = select(mSockFd + 1, &fds, NULL, NULL, NULL);

        if (rval >= 0 && FD_ISSET(mSockFd, &fds))
        {
            pthread_mutex_lock(&mMutex);
            mReceivedTask.Post();
            pthread_cond_wait(&mConditionVariable, &mMutex);
            pthread_mutex_unlock(&mMutex);
        }
    }

    return NULL;
}

void Socket::ReceivedTask(void *context)
{
    Socket *obj = reinterpret_cast<Socket *>(context);
    obj->ReceivedTask();
}

void Socket::ReceivedTask()
{
    char buf[1024];
    int length;

    pthread_mutex_lock(&mMutex);
    mSockLen = sizeof(mSockAddr);
    length = recvfrom(mSockFd, buf, sizeof(buf), 0, &mSockAddr, &mSockLen);
    pthread_mutex_unlock(&mMutex);

    if (buf[length - 1] == '\n')
    {
        buf[--length] = '\0';
    }

    if (buf[length - 1] == '\r')
    {
        buf[--length] = '\0';
    }

    Interpreter::ProcessLine(buf, length, *this);

    pthread_cond_signal(&mConditionVariable);
}

ThreadError Socket::Output(const char *buf, uint16_t bufLength)
{
    pthread_mutex_lock(&mMutex);
    sendto(mSockFd, buf, bufLength, 0, (struct sockaddr *)&mSockAddr, sizeof(mSockAddr));
    pthread_mutex_unlock(&mMutex);
    return kThreadError_None;
}

}  // namespace Cli
}  // namespace Thread
