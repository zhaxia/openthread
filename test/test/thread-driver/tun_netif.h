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

#include <common/code_utils.h>
#include <common/thread_error.h>
#include <ncp/ncp.pb-c.h>
#include <netinet/in.h>

namespace Thread {

class TunNetif
{
public:
    class Callbacks
    {
        void HandleReceive(uint8_t *buf, size_t buf_length);
    };

    ThreadError Open();
    ThreadError GetName(char *name, int name_length);
    int GetIndex();

    ThreadError Down();
    ThreadError Up();

    int GetFileDescriptor();
    size_t Read(uint8_t *buf, size_t buf_length);
    size_t Write(uint8_t *buf, size_t buf_length);

    ThreadError AddIp6Address(const struct in6_addr *address, uint8_t prefix_length);
    ThreadError RemoveIp6Address(const struct in6_addr *address);
    ThreadError SetIp6Addresses(ThreadIp6Addresses *addresses);

    ThreadError AddRoute(const struct in6_addr *prefix, uint8_t prefix_length);
    ThreadError ClearRoutes();

private:
    int tunfd_;
};

}  // namespace Thread

#endif  // TUN_NETIF_H_
