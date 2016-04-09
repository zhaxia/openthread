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
