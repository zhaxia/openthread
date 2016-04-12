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

#ifndef THREAD_DRIVER_H_
#define THREAD_DRIVER_H_

#include <ncp/hdlc.h>
#include <tun_netif.h>

namespace Thread {

class ThreadDriver
{
public:
    ThreadError Start();

private:
    ThreadError ProcessThreadControl(uint8_t *buf, uint16_t buf_length);
    ThreadError ProcessPrimitive(ThreadPrimitive *primitive);
    ThreadError ProcessAddresses(ThreadIp6Addresses *addresses);

    static void HandleReceive(void *context, uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    void HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    static void HandleSendDone(void *context);
    static void HandleSendMessageDone(void *context);

    TunNetif tun_netif_;
    int ipc_fd_ = -1;
};

}  // namespace Thread

#endif  // THREAD_DRIVER_H_
