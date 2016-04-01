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

#ifndef NETWORK_DATA_LOCAL_H_
#define NETWORK_DATA_LOCAL_H_

#include <net/udp6.h>
#include <thread/mle_router.h>
#include <thread/network_data.h>

namespace Thread {

class ThreadNetif;

namespace NetworkData {

class Local: public NetworkData
{
public:
    explicit Local(ThreadNetif &netif);
    ThreadError AddOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length, int8_t prf, uint8_t flags, bool stable);
    ThreadError RemoveOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length);

    ThreadError AddHasRoutePrefix(const uint8_t *prefix, uint8_t prefix_length, int8_t prf, bool stable);
    ThreadError RemoveHasRoutePrefix(const uint8_t *prefix, uint8_t prefix_length);

    ThreadError Register(const Ip6Address &destination);

private:
    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);

    ThreadError UpdateRloc();
    ThreadError UpdateRloc(PrefixTlv &prefix);
    ThreadError UpdateRloc(HasRouteTlv &has_route);
    ThreadError UpdateRloc(BorderRouterTlv &border_router);

    Udp6Socket m_socket;
    uint8_t m_coap_token[2];
    uint16_t m_coap_message_id;

    Mle::MleRouter *m_mle;
};

}  // namespace NetworkData
}  // namespace Thread

#endif  // NETWORK_DATA_LOCAL_H_
