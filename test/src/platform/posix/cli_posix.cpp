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

#include <platform/posix/cli_posix.h>
#include <platform/posix/cmdline.h>
#include <cli/cli_command.h>
#include <common/code_utils.h>

extern struct gengetopt_args_info args_info;

namespace Thread {
namespace Cli {

Socket::Socket():
    m_received_task(&ReceivedTask, this)
{
}

ThreadError Socket::Start()
{
    struct sockaddr_in sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(8000 + args_info.nodeid_arg);
    sockaddr.sin_addr.s_addr = INADDR_ANY;

    m_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(m_sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));

    pthread_create(&m_pthread, NULL, ReceiveThread, this);

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
        FD_SET(m_sockfd, &fds);

        rval = select(m_sockfd + 1, &fds, NULL, NULL, NULL);

        if (rval >= 0 && FD_ISSET(m_sockfd, &fds))
        {
            pthread_mutex_lock(&m_mutex);
            m_received_task.Post();
            pthread_cond_wait(&m_condition_variable, &m_mutex);
            pthread_mutex_unlock(&m_mutex);
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
    char *cur = buf;
    char *end = cur + sizeof(buf);
    int length;
    char *cmd;
    char *last;
    int argc;
    char *argv[kMaxArgs];

    pthread_mutex_lock(&m_mutex);
    m_socklen = sizeof(m_sockaddr);
    length = recvfrom(m_sockfd, buf, sizeof(buf), 0, &m_sockaddr, &m_socklen);
    pthread_mutex_unlock(&m_mutex);

    if (buf[length - 1] == '\n')
    {
        buf[--length] = '\0';
    }

    if (buf[length - 1] == '\r')
    {
        buf[--length] = '\0';
    }

    VerifyOrExit((cmd = strtok_r(buf, " ", &last)) != NULL, ;);

    if (strncmp(cmd, "?", 1) == 0)
    {
        snprintf(cur, end - cur, "%s", "Commands:\r\n");
        cur += strlen(cur);

        for (Command *command = m_commands; command; command = command->GetNext())
        {
            snprintf(cur, end - cur, "%s\r\n", command->GetName());
            cur += strlen(cur);
        }

        snprintf(cur, end - cur, "Done\r\n");
        cur += strlen(cur);

        sendto(m_sockfd, buf, cur - buf, 0, (struct sockaddr *)&m_sockaddr, sizeof(m_sockaddr));
    }
    else
    {
        for (argc = 0; argc < kMaxArgs; argc++)
        {
            if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL)
            {
                break;
            }
        }

        for (Command *command = m_commands; command; command = command->GetNext())
        {
            if (strcmp(cmd, command->GetName()) == 0)
            {
                command->Run(argc, argv, *this);
                break;
            }
        }
    }

exit:
    pthread_cond_signal(&m_condition_variable);
}

ThreadError Socket::Output(const char *buf, uint16_t buf_length)
{
    pthread_mutex_lock(&m_mutex);
    sendto(m_sockfd, buf, buf_length, 0, (struct sockaddr *)&m_sockaddr, sizeof(m_sockaddr));
    pthread_mutex_unlock(&m_mutex);
    return kThreadError_None;
}

}  // namespace Cli
}  // namespace Thread
