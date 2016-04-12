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

#ifndef NET_DHCP6_SERVER_H_
#define NET_DHCP6_SERVER_H_

#include <mac/mac_frame.h>
#include <net/dhcp6.h>
#include <net/udp6.h>

namespace Thread {
namespace Dhcp6 {

class Dhcp6ServerDelegate
{
public:
    virtual ThreadError HandleGetAddress(const ClientIdentifier *cilent_identifier, IaAddress *ia_address) = 0;
    virtual ThreadError HandleGetVendorSpecificInformation(void *buf, uint16_t *buf_length) = 0;
    virtual ThreadError HandleReleaseAddress(const Ip6Address *address) = 0;
    virtual ThreadError HandleLeaseQuery(Ip6Address *eid, Ip6Address *rloc, uint32_t *last_transaction_time) = 0;
};

class Dhcp6Server
{
public:
    explicit Dhcp6Server(Netif &netif);
    ThreadError Start(const Ip6Address *address, Dhcp6ServerDelegate *delegate);

private:
    enum
    {
        kUdpPort = 547,
    };

    ThreadError AppendHeader(Message &message, uint8_t type, const uint8_t *transaction_id);
    ThreadError AppendClientIdentifier(Message &message, const ClientIdentifier *client_identifier);
    ThreadError AppendServerIdentifier(Message &message);
    ThreadError AppendIaNa(Message &message, const ClientIdentifier *client_identifier);
    ThreadError AppendIaAddr(Message &message, const ClientIdentifier *client_identifier);
    ThreadError AppendStatusCode(Message &message, uint16_t status_code);
    ThreadError AppendRapidCommit(Message &message);
    ThreadError AppendVendorSpecificInformation(Message &message);

    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);

    void ProcessSolicit(Message &message, const Ip6Address *address, const uint8_t *transaction_id);
    void ProcessRelease(Message &message, const Ip6Address *address, const uint8_t *transaction_id);
    void ProcessLeaseQuery(Message &message, const Ip6Address *address, const uint8_t *transaction_id);
    uint16_t FindOption(Message &message, uint16_t offset, uint16_t length, uint16_t type);
    ThreadError ProcessClientIdentifier(Message &message, uint16_t offset, ClientIdentifier *option);
    ThreadError ProcessServerIdentifier(Message &message, uint16_t offset);
    ThreadError ProcessIaNa(Message &message, uint16_t offset);
    ThreadError ProcessIaAddr(Message &message, uint16_t offset);
    ThreadError ProcessRequestOption(Message &message, uint16_t offset);
    ThreadError ProcessStatusCode(Message &message, uint16_t offset);

    ThreadError SendReply(const Ip6Address *address, uint8_t type, const uint8_t *transaction_id,
                          const ClientIdentifier *client_identifier);
    ThreadError SendLeaseQueryReply(const Ip6Address *dst, const uint8_t *transaction_id,
                                    const ClientIdentifier *client_identifier, const Ip6Address *target,
                                    const Ip6Address *address, uint32_t last_transaction_time);

    Udp6Socket m_socket;
    Dhcp6ServerDelegate *m_delegate = NULL;
    Netif *m_netif;
};

}  // namespace Dhcp6
}  // namespace Thread

#endif  // NET_DHCP6_SERVER_H_
