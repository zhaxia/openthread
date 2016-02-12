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

#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <ncp/ncp.h>
#include <net/icmp6.h>
#include <net/ip6.h>
#include <net/ip6_address.h>
#include <net/ip6_mpl.h>
#include <net/ip6_routes.h>
#include <net/netif.h>
#include <net/udp6.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static Ip6Mpl ip6_mpl_;
static uint8_t mpl_sequence_;
static Ncp *ncp_ = NULL;

static ThreadError ForwardMessage(Message *message, Ip6MessageInfo *message_info);

Message *Ip6::NewMessage(uint16_t reserved) {
  return Message::New(Message::kTypeIp6,
                      sizeof(Ip6Header) + sizeof(Ip6HopByHopHeader) + sizeof(Ip6OptMpl) +
                      reserved);
}

uint16_t Ip6::UpdateChecksum(uint16_t checksum, uint16_t val) {
  uint16_t result = checksum + val;
  return result + (result < checksum);
}

uint16_t Ip6::UpdateChecksum(uint16_t checksum, const void *buf, uint16_t len) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t*>(buf);

  for (int i = 0; i < len; i++)
    checksum = Ip6::UpdateChecksum(checksum, (i & 1) ? bytes[i] : (static_cast<uint16_t>(bytes[i])) << 8);

  return checksum;
}

uint16_t Ip6::UpdateChecksum(uint16_t checksum, const Ip6Address *address) {
  return Ip6::UpdateChecksum(checksum, address->s6_addr, sizeof(*address));
}

uint16_t Ip6::ComputePseudoheaderChecksum(const Ip6Address *src, const Ip6Address *dst,
                                          uint16_t length, uint8_t proto) {
  uint16_t checksum;

  checksum = Ip6::UpdateChecksum(0, length);
  checksum = Ip6::UpdateChecksum(checksum, proto);
  checksum = UpdateChecksum(checksum, src);
  checksum = UpdateChecksum(checksum, dst);

  return checksum;
}

ThreadError Ip6::SetNcp(Ncp *ncp) {
  ncp_ = ncp;
  return kThreadError_None;
}

ThreadError Ip6::SendDatagram(Message *message, Ip6MessageInfo *message_info, uint8_t ipproto) {
  ThreadError error = kThreadError_None;
  Ip6Header ip6_header;
  uint16_t payload_length = message->GetLength();
  uint16_t checksum;

  ip6_header.ip6_flow = 0;
  ip6_header.ip6_vfc |= IPV6_VERSION;
  ip6_header.ip6_plen = HostSwap16(payload_length);
  ip6_header.ip6_nxt = ipproto;
  ip6_header.ip6_hlim = message_info->hop_limit ? message_info->hop_limit : kDefaultHopLimit;

  if (message_info->sock_addr.IsUnspecified()) {
    const NetifAddress *source = Netif::SelectSourceAddress(message_info);
    VerifyOrExit(source != NULL, error = kThreadError_Error);
    ip6_header.ip6_src = source->address;
  } else {
    ip6_header.ip6_src = message_info->sock_addr;
  }

  ip6_header.ip6_dst = message_info->peer_addr;
  if (ip6_header.ip6_dst.IsLinkLocal() || ip6_header.ip6_dst.IsLinkLocalMulticast())
    VerifyOrExit(message_info->interface_id != 0, error = kThreadError_Drop);

  if (message_info->peer_addr.IsRealmLocalMulticast()) {
    Ip6HopByHopHeader hbh_header;
    Ip6OptMpl mpl_option;

    hbh_header.ip6h_nxt = ipproto;
    hbh_header.ip6h_len = 0;
    mpl_option.ip6o_type = IP6OPT_MPL;
    mpl_option.ip6o_len = sizeof(Ip6OptMpl) - offsetof(Ip6OptMpl, mpl_control);
    mpl_option.mpl_control = IP6_MPL_SEED_2;
    mpl_option.mpl_sequence = mpl_sequence_++;
    mpl_option.mpl_seed = ip6_header.ip6_src.s6_addr16[7];
    SuccessOrExit(error = message->Prepend(&mpl_option, sizeof(mpl_option)));
    SuccessOrExit(error = message->Prepend(&hbh_header, sizeof(hbh_header)));
    ip6_header.ip6_plen = HostSwap16(sizeof(hbh_header) + sizeof(mpl_option) + payload_length);
    ip6_header.ip6_nxt = IPPROTO_HOPOPTS;
  }

  SuccessOrExit(error = message->Prepend(&ip6_header, sizeof(ip6_header)));

  // compute checksum
  checksum = ComputePseudoheaderChecksum(&ip6_header.ip6_src, &ip6_header.ip6_dst, payload_length, ipproto);
  switch (ipproto) {
    case IPPROTO_UDP:
      SuccessOrExit(error = Udp6::UpdateChecksum(message, checksum));
      break;
    case IPPROTO_ICMPV6:
      SuccessOrExit(error = Icmp6::UpdateChecksum(message, checksum));
      break;
  }

exit:
  if (error != kThreadError_None) {
    Message::Free(message);
    return error;
  }

  return HandleDatagram(message, NULL, message_info->interface_id, NULL, false);
}

ThreadError HandleOptions(Message *message) {
  Ip6HopByHopHeader hbh_header;
  Ip6OptionHeader option_header;
  uint16_t end_offset;
  ThreadError error = kThreadError_None;

  message->Read(message->GetOffset(), sizeof(hbh_header), &hbh_header);
  end_offset = message->GetOffset() + 8*(hbh_header.ip6h_len + 1);

  message->MoveOffset(sizeof(option_header));

  while (message->GetOffset() < end_offset) {
    message->Read(message->GetOffset(), sizeof(option_header), &option_header);

    switch (option_header.ip6o_type) {
      case IP6OPT_MPL:
        SuccessOrExit(error = ip6_mpl_.ProcessOption(message));
        break;
      default:
        switch (IP6OPT_TYPE(option_header.ip6o_type)) {
          case IP6OPT_TYPE_SKIP:
            break;
          case IP6OPT_TYPE_DISCARD:
            ExitNow(error = kThreadError_Drop);
          case IP6OPT_TYPE_ICMP:
            // XXX: send icmp error
            ExitNow(error = kThreadError_Drop);
          case IP6OPT_TYPE_FORCEICMP:
            // XXX: send icmp error
            ExitNow(error = kThreadError_Drop);
        }
        break;
    }
    message->MoveOffset(sizeof(option_header) + option_header.ip6o_len);
  }

exit:
  return error;
}

ThreadError HandleFragment(Message *message) {
  Ip6FragmentHeader fragment_header;
  ThreadError error = kThreadError_None;

  message->Read(message->GetOffset(), sizeof(fragment_header), &fragment_header);

  VerifyOrExit((fragment_header.ip6f_offlg & (IP6F_OFF_MASK | IP6F_MORE_FRAG)) == 0,
               error = kThreadError_Drop);

  message->MoveOffset(sizeof(fragment_header));

exit:
  return error;
}

ThreadError HandleExtensionHeaders(Message *message, uint8_t *next_header, bool receive) {
  Ip6ExtensionHeader extension_header;
  ThreadError error = kThreadError_None;

  while (receive == true || *next_header == IPPROTO_HOPOPTS) {
    VerifyOrExit(message->GetOffset() <= message->GetLength(), error = kThreadError_Drop);

    message->Read(message->GetOffset(), sizeof(extension_header), &extension_header);

    switch (*next_header) {
      case IPPROTO_HOPOPTS:
        SuccessOrExit(error = HandleOptions(message));
        break;
      case IPPROTO_IPV6:
        ExitNow(error = kThreadError_Drop);
      case IPPROTO_ROUTING:
        ExitNow(error = kThreadError_Drop);
      case IPPROTO_FRAGMENT:
        SuccessOrExit(error = HandleFragment(message));
        break;
      case IPPROTO_DSTOPTS:
        SuccessOrExit(error = HandleOptions(message));
        break;
      case IPPROTO_NONE:
        ExitNow(error = kThreadError_Drop);
      default:
        ExitNow();
    }
    *next_header = extension_header.ip6e_nxt;
  }

exit:
  return error;
}

ThreadError HandlePayload(Message *message, Ip6MessageInfo *message_info, uint8_t ipproto) {
  switch (ipproto) {
    case IPPROTO_UDP:
      return Udp6::HandleMessage(message, message_info);
    case IPPROTO_ICMPV6:
      return Icmp6::HandleMessage(message, message_info);
  }
  return kThreadError_None;
}

ThreadError Ip6::HandleDatagram(Message *message, Netif *netif, uint8_t interface_id, void *link_message_info,
                                bool from_ncp_host) {
  Ip6MessageInfo message_info;
  Ip6Header ip6_header;
  uint16_t payload_len;
  bool receive = false;
  bool forward = false;

#if 0
  uint8_t buf[1024];
  message->Read(0, sizeof(buf), buf);
  dump("handle datagram", buf, message->GetLength());
#endif

  // check message length
  VerifyOrExit(message->GetLength() >= sizeof(ip6_header), ;);
  message->Read(0, sizeof(ip6_header), &ip6_header);
  payload_len = HostSwap16(ip6_header.ip6_plen);

  // check Version
  VerifyOrExit((ip6_header.ip6_vfc & IPV6_VERSION_MASK) == IPV6_VERSION, ;);

  // check Payload Length
  VerifyOrExit(sizeof(ip6_header) + payload_len == message->GetLength() &&
               sizeof(ip6_header) + payload_len <= Ip6::kMaxDatagramLength, ;);

  memset(&message_info, 0, sizeof(message_info));
  message_info.peer_addr = ip6_header.ip6_src;
  message_info.sock_addr = ip6_header.ip6_dst;
  message_info.interface_id = interface_id;
  message_info.hop_limit = ip6_header.ip6_hlim;
  message_info.link_info = link_message_info;

  // determine destination of packet
  if (ip6_header.ip6_dst.IsMulticast()) {
    if (netif != NULL && netif->IsMulticastSubscribed(&ip6_header.ip6_dst))
      receive = true;
    if (ip6_header.ip6_dst.GetScope() > Ip6Address::kLinkLocalScope)
      forward = true;
    else if (netif == NULL)
      forward = true;
  } else {
    if (Netif::IsAddress(&ip6_header.ip6_dst))
      receive = true;
    else if (!ip6_header.ip6_dst.IsLinkLocal())
      forward = true;
    else if (netif == NULL)
      forward = true;
  }

  message->SetOffset(sizeof(ip6_header));

  // process IPv6 Extension Headers
  SuccessOrExit(HandleExtensionHeaders(message, &ip6_header.ip6_nxt, receive));

  // process IPv6 Payload
  if (receive) {
    SuccessOrExit(HandlePayload(message, &message_info, ip6_header.ip6_nxt));
    if (ncp_ != NULL && from_ncp_host == false) {
      ncp_->SendMessage(message);
      return kThreadError_None;
    }
  }

  if (forward) {
    if (netif != NULL)
      ip6_header.ip6_hlim--;

    if (ip6_header.ip6_hlim == 0) {
      // send time exceeded
    } else {
      message->Write(offsetof(Ip6Header, ip6_hlim), sizeof(ip6_header.ip6_hlim), &ip6_header.ip6_hlim);
      SuccessOrExit(ForwardMessage(message, &message_info));
      return kThreadError_None;
    }
  }

exit:
  Message::Free(message);
  return kThreadError_None;
}

ThreadError ForwardMessage(Message *message, Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  int interface_id;
  Netif *netif;

  // multicast
  if (message_info->sock_addr.IsMulticast()) {
    interface_id = message_info->interface_id;
    goto found_route;
  }

  // on-link link-local address
  if (message_info->sock_addr.IsLinkLocal()) {
    interface_id = message_info->interface_id;
    goto found_route;
  }

  // on-link global address
  if ((interface_id = Netif::GetOnLinkNetif(&message_info->sock_addr)) > 0)
    goto found_route;

  // route
  const Ip6Route *route;
  if ((route = Ip6Routes::Lookup(&message_info->peer_addr, &message_info->sock_addr)) != NULL) {
    interface_id = route->interface_id;
    goto found_route;
  }

  dump("no route", &message_info->sock_addr, 16);
  ExitNow(error = kThreadError_NoRoute);

found_route:
  // submit message to interface
  VerifyOrExit((netif = Netif::GetNetifById(interface_id)) != NULL, error = kThreadError_NoRoute);
  SuccessOrExit(error = netif->SendMessage(message));

exit:
  return error;
}

}  // namespace Thread
