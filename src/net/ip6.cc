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

#include <common/code_utils.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/icmp6.h>
#include <net/ip6.h>
#include <net/ip6_address.h>
#include <net/ip6_mpl.h>
#include <net/ip6_routes.h>
#include <net/netif.h>
#include <net/udp6.h>

namespace Thread {

static Ip6Mpl s_ip6_mpl;
static Ip6::NcpReceivedDatagramHandler s_ncp_received_handler = NULL;
static void *s_ncp_received_handler_context = NULL;

static ThreadError ForwardMessage(Message &message, Ip6MessageInfo &message_info);

Message *Ip6::NewMessage(uint16_t reserved)
{
    return Message::New(Message::kTypeIp6,
                        sizeof(Ip6Header) + sizeof(Ip6HopByHopHeader) + sizeof(Ip6OptionMpl) + reserved);
}

uint16_t Ip6::UpdateChecksum(uint16_t checksum, uint16_t val)
{
    uint16_t result = checksum + val;
    return result + (result < checksum);
}

uint16_t Ip6::UpdateChecksum(uint16_t checksum, const void *buf, uint16_t len)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(buf);

    for (int i = 0; i < len; i++)
    {
        checksum = Ip6::UpdateChecksum(checksum, (i & 1) ? bytes[i] : (static_cast<uint16_t>(bytes[i])) << 8);
    }

    return checksum;
}

uint16_t Ip6::UpdateChecksum(uint16_t checksum, const Ip6Address &address)
{
    return Ip6::UpdateChecksum(checksum, address.s6_addr, sizeof(address));
}

uint16_t Ip6::ComputePseudoheaderChecksum(const Ip6Address &src, const Ip6Address &dst, uint16_t length, IpProto proto)
{
    uint16_t checksum;

    checksum = Ip6::UpdateChecksum(0, length);
    checksum = Ip6::UpdateChecksum(checksum, proto);
    checksum = UpdateChecksum(checksum, src);
    checksum = UpdateChecksum(checksum, dst);

    return checksum;
}

void Ip6::SetNcpReceivedHandler(NcpReceivedDatagramHandler handler, void *context)
{
    s_ncp_received_handler = handler;
    s_ncp_received_handler_context = context;
}

ThreadError AddMplOption(Message &message, Ip6Header &ip6_header, IpProto next_header, uint16_t payload_length)
{
    ThreadError error = kThreadError_None;
    Ip6HopByHopHeader hbh_header;
    Ip6OptionMpl mpl_option;

    hbh_header.SetNextHeader(next_header);
    hbh_header.SetLength(0);
    s_ip6_mpl.InitOption(mpl_option, HostSwap16(ip6_header.GetSource()->s6_addr16[7]));
    SuccessOrExit(error = message.Prepend(&mpl_option, sizeof(mpl_option)));
    SuccessOrExit(error = message.Prepend(&hbh_header, sizeof(hbh_header)));
    ip6_header.SetPayloadLength(sizeof(hbh_header) + sizeof(mpl_option) + payload_length);
    ip6_header.SetNextHeader(kProtoHopOpts);
exit:
    return error;
}

ThreadError Ip6::SendDatagram(Message &message, Ip6MessageInfo &message_info, IpProto ipproto)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6_header;
    uint16_t payload_length = message.GetLength();
    uint16_t checksum;
    const NetifUnicastAddress *source;

    ip6_header.Init();
    ip6_header.SetPayloadLength(payload_length);
    ip6_header.SetNextHeader(ipproto);
    ip6_header.SetHopLimit(message_info.hop_limit ? message_info.hop_limit : kDefaultHopLimit);

    if (message_info.sock_addr.IsUnspecified())
    {
        VerifyOrExit((source = Netif::SelectSourceAddress(message_info)) != NULL, error = kThreadError_Error);
        ip6_header.SetSource(source->address);
    }
    else
    {
        ip6_header.SetSource(message_info.sock_addr);
    }

    ip6_header.SetDestination(message_info.peer_addr);

    if (ip6_header.GetDestination()->IsLinkLocal() || ip6_header.GetDestination()->IsLinkLocalMulticast())
    {
        VerifyOrExit(message_info.interface_id != 0, error = kThreadError_Drop);
    }

    if (message_info.peer_addr.IsRealmLocalMulticast())
    {
        SuccessOrExit(error = AddMplOption(message, ip6_header, ipproto, payload_length));
    }

    SuccessOrExit(error = message.Prepend(&ip6_header, sizeof(ip6_header)));

    // compute checksum
    checksum = ComputePseudoheaderChecksum(*ip6_header.GetSource(), *ip6_header.GetDestination(),
                                           payload_length, ipproto);

    switch (ipproto)
    {
    case kProtoUdp:
        SuccessOrExit(error = Udp6::UpdateChecksum(message, checksum));
        break;

    case kProtoIcmp6:
        SuccessOrExit(error = Icmp6::UpdateChecksum(message, checksum));
        break;

    default:
        break;
    }

exit:

    if (error != kThreadError_None)
    {
        Message::Free(message);
    }
    else
    {
        error = HandleDatagram(message, NULL, message_info.interface_id, NULL, false);
    }

    return error;
}

ThreadError HandleOptions(Message &message)
{
    ThreadError error = kThreadError_None;
    Ip6HopByHopHeader hbh_header;
    Ip6OptionHeader option_header;
    uint16_t end_offset;

    message.Read(message.GetOffset(), sizeof(hbh_header), &hbh_header);
    end_offset = message.GetOffset() + (hbh_header.GetLength() + 1) * 8;

    message.MoveOffset(sizeof(option_header));

    while (message.GetOffset() < end_offset)
    {
        message.Read(message.GetOffset(), sizeof(option_header), &option_header);

        switch (option_header.GetType())
        {
        case Ip6OptionMpl::kType:
            SuccessOrExit(error = s_ip6_mpl.ProcessOption(message));
            break;

        default:
            switch (option_header.GetAction())
            {
            case Ip6OptionHeader::kActionSkip:
                break;

            case Ip6OptionHeader::kActionDiscard:
                ExitNow(error = kThreadError_Drop);

            case Ip6OptionHeader::kActionForceIcmp:
                // XXX: send icmp error
                ExitNow(error = kThreadError_Drop);

            case Ip6OptionHeader::kActionIcmp:
                // XXX: send icmp error
                ExitNow(error = kThreadError_Drop);

            }

            break;
        }

        message.MoveOffset(sizeof(option_header) + option_header.GetLength());
    }

exit:
    return error;
}

ThreadError HandleFragment(Message &message)
{
    ThreadError error = kThreadError_None;
    Ip6FragmentHeader fragment_header;

    message.Read(message.GetOffset(), sizeof(fragment_header), &fragment_header);

    VerifyOrExit(fragment_header.GetOffset() == 0 && fragment_header.IsMoreFlagSet() == false,
                 error = kThreadError_Drop);

    message.MoveOffset(sizeof(fragment_header));

exit:
    return error;
}

ThreadError HandleExtensionHeaders(Message &message, uint8_t &next_header, bool receive)
{
    ThreadError error = kThreadError_None;
    Ip6ExtensionHeader extension_header;

    while (receive == true || next_header == kProtoHopOpts)
    {
        VerifyOrExit(message.GetOffset() <= message.GetLength(), error = kThreadError_Drop);

        message.Read(message.GetOffset(), sizeof(extension_header), &extension_header);

        switch (next_header)
        {
        case kProtoHopOpts:
            SuccessOrExit(error = HandleOptions(message));
            break;

        case kProtoFragment:
            SuccessOrExit(error = HandleFragment(message));
            break;

        case kProtoDstOpts:
            SuccessOrExit(error = HandleOptions(message));
            break;

        case kProtoIp6:
        case kProtoRouting:
        case kProtoNone:
            ExitNow(error = kThreadError_Drop);

        default:
            ExitNow();
        }

        next_header = extension_header.GetNextHeader();
    }

exit:
    return error;
}

ThreadError HandlePayload(Message &message, Ip6MessageInfo &message_info, uint8_t ipproto)
{
    ThreadError error = kThreadError_None;

    switch (ipproto)
    {
    case kProtoUdp:
        ExitNow(error = Udp6::HandleMessage(message, message_info));

    case kProtoIcmp6:
        ExitNow(error = Icmp6::HandleMessage(message, message_info));
    }

exit:
    return error;
}

ThreadError Ip6::HandleDatagram(Message &message, Netif *netif, uint8_t interface_id, const void *link_message_info,
                                bool from_ncp_host)
{
    ThreadError error = kThreadError_Drop;
    Ip6MessageInfo message_info;
    Ip6Header ip6_header;
    uint16_t payload_len;
    bool receive = false;
    bool forward = false;
    uint8_t next_header;
    uint8_t hop_limit;

#if 0
    uint8_t buf[1024];
    message.Read(0, sizeof(buf), buf);
    dump("handle datagram", buf, message.GetLength());
#endif

    // check message length
    VerifyOrExit(message.GetLength() >= sizeof(ip6_header), ;);
    message.Read(0, sizeof(ip6_header), &ip6_header);
    payload_len = ip6_header.GetPayloadLength();

    // check Version
    VerifyOrExit(ip6_header.IsVersion6(), ;);

    // check Payload Length
    VerifyOrExit(sizeof(ip6_header) + payload_len == message.GetLength() &&
                 sizeof(ip6_header) + payload_len <= Ip6::kMaxDatagramLength, ;);

    memset(&message_info, 0, sizeof(message_info));
    message_info.peer_addr = *ip6_header.GetSource();
    message_info.sock_addr = *ip6_header.GetDestination();
    message_info.interface_id = interface_id;
    message_info.hop_limit = ip6_header.GetHopLimit();
    message_info.link_info = link_message_info;

    // determine destination of packet
    if (ip6_header.GetDestination()->IsMulticast())
    {
        if (netif != NULL && netif->IsMulticastSubscribed(*ip6_header.GetDestination()))
        {
            receive = true;
        }

        if (ip6_header.GetDestination()->GetScope() > Ip6Address::kLinkLocalScope)
        {
            forward = true;
        }
        else if (netif == NULL)
        {
            forward = true;
        }
    }
    else
    {
        if (Netif::IsUnicastAddress(*ip6_header.GetDestination()))
        {
            receive = true;
        }
        else if (!ip6_header.GetDestination()->IsLinkLocal())
        {
            forward = true;
        }
        else if (netif == NULL)
        {
            forward = true;
        }
    }

    message.SetOffset(sizeof(ip6_header));

    // process IPv6 Extension Headers
    next_header = ip6_header.GetNextHeader();
    SuccessOrExit(HandleExtensionHeaders(message, next_header, receive));

    // process IPv6 Payload
    if (receive)
    {
        SuccessOrExit(HandlePayload(message, message_info, next_header));

        if (s_ncp_received_handler != NULL && from_ncp_host == false)
        {
            s_ncp_received_handler(s_ncp_received_handler_context, message);
            ExitNow(error = kThreadError_None);
        }
    }

    if (forward)
    {
        if (netif != NULL)
        {
            ip6_header.SetHopLimit(ip6_header.GetHopLimit() - 1);
        }

        if (ip6_header.GetHopLimit() == 0)
        {
            // send time exceeded
        }
        else
        {
            hop_limit = ip6_header.GetHopLimit();
            message.Write(Ip6Header::GetHopLimitOffset(), Ip6Header::GetHopLimitSize(), &hop_limit);
            SuccessOrExit(ForwardMessage(message, message_info));
            ExitNow(error = kThreadError_None);
        }
    }

exit:

    if (error == kThreadError_Drop)
    {
        Message::Free(message);
    }

    return kThreadError_None;
}

ThreadError ForwardMessage(Message &message, Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    int interface_id;
    Netif *netif;

    if (message_info.sock_addr.IsMulticast())
    {
        // multicast
        interface_id = message_info.interface_id;
    }
    else if (message_info.sock_addr.IsLinkLocal())
    {
        // on-link link-local address
        interface_id = message_info.interface_id;
    }
    else if ((interface_id = Netif::GetOnLinkNetif(message_info.sock_addr)) > 0)
    {
        // on-link global address
        ;
    }
    else if ((interface_id = Ip6Routes::Lookup(message_info.peer_addr, message_info.sock_addr)) > 0)
    {
        // route
        ;
    }
    else
    {
        dump("no route", &message_info.sock_addr, 16);
        ExitNow(error = kThreadError_NoRoute);
    }

    // submit message to interface
    VerifyOrExit((netif = Netif::GetNetifById(interface_id)) != NULL, error = kThreadError_NoRoute);
    SuccessOrExit(error = netif->SendMessage(message));

exit:
    return error;
}

}  // namespace Thread
