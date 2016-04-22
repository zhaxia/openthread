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

/**
 * @file
 *   This file implements IPv6 networking.
 */

#include <common/code_utils.hpp>
#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/icmp6.hpp>
#include <net/ip6.hpp>
#include <net/ip6_address.hpp>
#include <net/ip6_mpl.hpp>
#include <net/ip6_routes.hpp>
#include <net/netif.hpp>
#include <net/udp6.hpp>

namespace Thread {

static Ip6Mpl sIp6Mpl;
static Ip6::NcpReceivedDatagramHandler sNcpReceivedHandler = NULL;
static void *sNcpReceivedHandlerContext = NULL;

static ThreadError ForwardMessage(Message &message, Ip6MessageInfo &messageInfo);

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
    return Ip6::UpdateChecksum(checksum, address.m8, sizeof(address));
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
    sNcpReceivedHandler = handler;
    sNcpReceivedHandlerContext = context;
}

ThreadError AddMplOption(Message &message, Ip6Header &ip6Header, IpProto nextHeader, uint16_t payloadLength)
{
    ThreadError error = kThreadError_None;
    Ip6HopByHopHeader hbhHeader;
    Ip6OptionMpl mplOption;

    hbhHeader.SetNextHeader(nextHeader);
    hbhHeader.SetLength(0);
    sIp6Mpl.InitOption(mplOption, HostSwap16(ip6Header.GetSource()->m16[7]));
    SuccessOrExit(error = message.Prepend(&mplOption, sizeof(mplOption)));
    SuccessOrExit(error = message.Prepend(&hbhHeader, sizeof(hbhHeader)));
    ip6Header.SetPayloadLength(sizeof(hbhHeader) + sizeof(mplOption) + payloadLength);
    ip6Header.SetNextHeader(kProtoHopOpts);
exit:
    return error;
}

ThreadError Ip6::SendDatagram(Message &message, Ip6MessageInfo &messageInfo, IpProto ipproto)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6Header;
    uint16_t payloadLength = message.GetLength();
    uint16_t checksum;
    const NetifUnicastAddress *source;

    ip6Header.Init();
    ip6Header.SetPayloadLength(payloadLength);
    ip6Header.SetNextHeader(ipproto);
    ip6Header.SetHopLimit(messageInfo.mHopLimit ? messageInfo.mHopLimit : kDefaultHopLimit);

    if (messageInfo.GetSockAddr().IsUnspecified())
    {
        VerifyOrExit((source = Netif::SelectSourceAddress(messageInfo)) != NULL, error = kThreadError_Error);
        ip6Header.SetSource(source->GetAddress());
    }
    else
    {
        ip6Header.SetSource(messageInfo.GetSockAddr());
    }

    ip6Header.SetDestination(messageInfo.GetPeerAddr());

    if (ip6Header.GetDestination()->IsLinkLocal() || ip6Header.GetDestination()->IsLinkLocalMulticast())
    {
        VerifyOrExit(messageInfo.mInterfaceId != 0, error = kThreadError_Drop);
    }

    if (messageInfo.GetPeerAddr().IsRealmLocalMulticast())
    {
        SuccessOrExit(error = AddMplOption(message, ip6Header, ipproto, payloadLength));
    }

    SuccessOrExit(error = message.Prepend(&ip6Header, sizeof(ip6Header)));

    // compute checksum
    checksum = ComputePseudoheaderChecksum(*ip6Header.GetSource(), *ip6Header.GetDestination(),
                                           payloadLength, ipproto);

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
        error = HandleDatagram(message, NULL, messageInfo.mInterfaceId, NULL, false);
    }

    return error;
}

ThreadError HandleOptions(Message &message)
{
    ThreadError error = kThreadError_None;
    Ip6HopByHopHeader hbhHeader;
    Ip6OptionHeader optionHeader;
    uint16_t endOffset;

    message.Read(message.GetOffset(), sizeof(hbhHeader), &hbhHeader);
    endOffset = message.GetOffset() + (hbhHeader.GetLength() + 1) * 8;

    message.MoveOffset(sizeof(optionHeader));

    while (message.GetOffset() < endOffset)
    {
        message.Read(message.GetOffset(), sizeof(optionHeader), &optionHeader);

        switch (optionHeader.GetType())
        {
        case Ip6OptionMpl::kType:
            SuccessOrExit(error = sIp6Mpl.ProcessOption(message));
            break;

        default:
            switch (optionHeader.GetAction())
            {
            case Ip6OptionHeader::kActionSkip:
                break;

            case Ip6OptionHeader::kActionDiscard:
                ExitNow(error = kThreadError_Drop);

            case Ip6OptionHeader::kActionForceIcmp:
                // TODO: send icmp error
                ExitNow(error = kThreadError_Drop);

            case Ip6OptionHeader::kActionIcmp:
                // TODO: send icmp error
                ExitNow(error = kThreadError_Drop);

            }

            break;
        }

        message.MoveOffset(sizeof(optionHeader) + optionHeader.GetLength());
    }

exit:
    return error;
}

ThreadError HandleFragment(Message &message)
{
    ThreadError error = kThreadError_None;
    Ip6FragmentHeader fragmentHeader;

    message.Read(message.GetOffset(), sizeof(fragmentHeader), &fragmentHeader);

    VerifyOrExit(fragmentHeader.GetOffset() == 0 && fragmentHeader.IsMoreFlagSet() == false,
                 error = kThreadError_Drop);

    message.MoveOffset(sizeof(fragmentHeader));

exit:
    return error;
}

ThreadError HandleExtensionHeaders(Message &message, uint8_t &nextHeader, bool receive)
{
    ThreadError error = kThreadError_None;
    Ip6ExtensionHeader extensionHeader;

    while (receive == true || nextHeader == kProtoHopOpts)
    {
        VerifyOrExit(message.GetOffset() <= message.GetLength(), error = kThreadError_Drop);

        message.Read(message.GetOffset(), sizeof(extensionHeader), &extensionHeader);

        switch (nextHeader)
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

        nextHeader = extensionHeader.GetNextHeader();
    }

exit:
    return error;
}

ThreadError HandlePayload(Message &message, Ip6MessageInfo &messageInfo, uint8_t ipproto)
{
    ThreadError error = kThreadError_None;

    switch (ipproto)
    {
    case kProtoUdp:
        ExitNow(error = Udp6::HandleMessage(message, messageInfo));

    case kProtoIcmp6:
        ExitNow(error = Icmp6::HandleMessage(message, messageInfo));
    }

exit:
    return error;
}

ThreadError Ip6::HandleDatagram(Message &message, Netif *netif, uint8_t interfaceId, const void *linkMessageInfo,
                                bool fromNcpHost)
{
    ThreadError error = kThreadError_Drop;
    Ip6MessageInfo messageInfo;
    Ip6Header ip6Header;
    uint16_t payloadLength;
    bool receive = false;
    bool forward = false;
    uint8_t nextHeader;
    uint8_t hopLimit;

#if 1
    uint8_t buf[1024];
    message.Read(0, sizeof(buf), buf);
    dump("handle datagram", buf, message.GetLength());
#endif

    // check message length
    VerifyOrExit(message.GetLength() >= sizeof(ip6Header), ;);
    message.Read(0, sizeof(ip6Header), &ip6Header);
    payloadLength = ip6Header.GetPayloadLength();

    // check Version
    VerifyOrExit(ip6Header.IsVersion6(), ;);

    // check Payload Length
    VerifyOrExit(sizeof(ip6Header) + payloadLength == message.GetLength() &&
                 sizeof(ip6Header) + payloadLength <= Ip6::kMaxDatagramLength, ;);

    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.GetPeerAddr() = *ip6Header.GetSource();
    messageInfo.GetSockAddr() = *ip6Header.GetDestination();
    messageInfo.mInterfaceId = interfaceId;
    messageInfo.mHopLimit = ip6Header.GetHopLimit();
    messageInfo.mLinkInfo = linkMessageInfo;

    // determine destination of packet
    if (ip6Header.GetDestination()->IsMulticast())
    {
        if (netif != NULL && netif->IsMulticastSubscribed(*ip6Header.GetDestination()))
        {
            receive = true;
        }

        if (ip6Header.GetDestination()->GetScope() > Ip6Address::kLinkLocalScope)
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
        if (Netif::IsUnicastAddress(*ip6Header.GetDestination()))
        {
            receive = true;
        }
        else if (!ip6Header.GetDestination()->IsLinkLocal())
        {
            forward = true;
        }
        else if (netif == NULL)
        {
            forward = true;
        }
    }

    message.SetOffset(sizeof(ip6Header));

    // process IPv6 Extension Headers
    nextHeader = ip6Header.GetNextHeader();
    SuccessOrExit(HandleExtensionHeaders(message, nextHeader, receive));

    // process IPv6 Payload
    if (receive)
    {
        SuccessOrExit(HandlePayload(message, messageInfo, nextHeader));

        if (sNcpReceivedHandler != NULL && fromNcpHost == false)
        {
            sNcpReceivedHandler(sNcpReceivedHandlerContext, message);
            ExitNow(error = kThreadError_None);
        }
    }

    if (forward)
    {
        if (netif != NULL)
        {
            ip6Header.SetHopLimit(ip6Header.GetHopLimit() - 1);
        }

        if (ip6Header.GetHopLimit() == 0)
        {
            // send time exceeded
        }
        else
        {
            hopLimit = ip6Header.GetHopLimit();
            message.Write(Ip6Header::GetHopLimitOffset(), Ip6Header::GetHopLimitSize(), &hopLimit);
            SuccessOrExit(ForwardMessage(message, messageInfo));
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

ThreadError ForwardMessage(Message &message, Ip6MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    int interfaceId;
    Netif *netif;

    if (messageInfo.GetSockAddr().IsMulticast())
    {
        // multicast
        interfaceId = messageInfo.mInterfaceId;
    }
    else if (messageInfo.GetSockAddr().IsLinkLocal())
    {
        // on-link link-local address
        interfaceId = messageInfo.mInterfaceId;
    }
    else if ((interfaceId = Netif::GetOnLinkNetif(messageInfo.GetSockAddr())) > 0)
    {
        // on-link global address
        ;
    }
    else if ((interfaceId = Ip6Routes::Lookup(messageInfo.GetPeerAddr(), messageInfo.GetSockAddr())) > 0)
    {
        // route
        ;
    }
    else
    {
        dump("no route", &messageInfo.GetSockAddr(), 16);
        ExitNow(error = kThreadError_NoRoute);
    }

    // submit message to interface
    VerifyOrExit((netif = Netif::GetNetifById(interfaceId)) != NULL, error = kThreadError_NoRoute);
    SuccessOrExit(error = netif->SendMessage(message));

exit:
    return error;
}

}  // namespace Thread
