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

#ifndef CLI_POSIX_HPP_
#define CLI_POSIX_HPP_

#include <sys/socket.h>
#include <pthread.h>

#include <openthread-types.h>
#include <cli/cli_server.hpp>
#include <common/tasklet.hpp>

namespace Thread {
namespace Cli {

class Socket: public Server
{
public:
    Socket(void);

    /**
     * This method starts the CLI server.
     *
     * @retval kThreadError_None  Successfully started the server.
     *
     */
    ThreadError Start(void);

    /**
     * This method delivers output to the client.
     *
     * @param[in]  aBuf        A pointer to a buffer.
     * @param[in]  aBufLength  Number of bytes in the buffer.
     *
     * @retval kThreadError_None  Successfully delivered output the client.
     *
     */
    ThreadError Output(const char *aBuf, uint16_t aBufLength);

private:
    static void *ReceiveThread(void *aContext);
    void *ReceiveThread(void);

    static void ReceivedTask(void *aContext);
    void ReceivedTask(void);

    Tasklet mReceivedTask;

    pthread_t mPthread;
    pthread_mutex_t mMutex;
    pthread_cond_t mConditionVariable;
    int mSockFd;
    struct sockaddr mSockAddr;
    socklen_t mSockLen;
};

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_POSIX_HPP_
