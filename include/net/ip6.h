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

#ifndef NET_IP6_H_
#define NET_IP6_H_

#include <common/encoding.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6_address.h>
#include <net/netif.h>
#include <net/socket.h>

namespace Thread {

class Ncp;

class Ip6 {
 public:
  enum {
    kDefaultHopLimit = 64,
    kMaxDatagramLength = 1500,
  };

  static Message *NewMessage(uint16_t reserved);
  static ThreadError SendDatagram(Message *message, Ip6MessageInfo *message_info, uint8_t ipproto);
  static ThreadError HandleDatagram(Message *message, Netif *netif, uint8_t interface_id, void *link_message_info,
                                    bool from_ncp_host);

  static uint16_t UpdateChecksum(uint16_t checksum, uint16_t val);
  static uint16_t UpdateChecksum(uint16_t checksum, const void *buf, uint16_t length);
  static uint16_t UpdateChecksum(uint16_t checksum, const Ip6Address *address);
  static uint16_t ComputePseudoheaderChecksum(const Ip6Address *src, const Ip6Address *dst,
                                              uint16_t length, uint8_t proto);

  static ThreadError SetNcp(Ncp* ncp);
};

typedef struct {
  union {
    struct ip6_hdrctl {
      uint32_t ip6_un1_flow;
      uint16_t ip6_un1_plen;
      uint8_t ip6_un1_nxt;
      uint8_t ip6_un1_hlim;
    } ip6_un1;
    uint8_t ip6_un2_vfc;
  } ip6_ctlun;
  Ip6Address ip6_src;
  Ip6Address ip6_dst;
} Ip6Header;

#define ip6_vfc         ip6_ctlun.ip6_un2_vfc
#define ip6_flow        ip6_ctlun.ip6_un1.ip6_un1_flow
#define ip6_plen        ip6_ctlun.ip6_un1.ip6_un1_plen
#define ip6_nxt         ip6_ctlun.ip6_un1.ip6_un1_nxt
#define ip6_hlim        ip6_ctlun.ip6_un1.ip6_un1_hlim
#define ip6_hops        ip6_ctlun.ip6_un1.ip6_un1_hlim

#define IPV6_VERSION            0x60
#define IPV6_VERSION_MASK       0xf0

#if BYTE_ORDER == BIG_ENDIAN
#define IPV6_FLOWINFO_MASK      0x0fffffff      /* flow info (28 bits) */
#define IPV6_FLOWLABEL_MASK     0x000fffff      /* flow label (20 bits) */
#else /* BYTE_ORDER == LITTLE_ENDIAN */
#define IPV6_FLOWINFO_MASK      0xffffff0f      /* flow info (28 bits) */
#define IPV6_FLOWLABEL_MASK     0xffff0f00      /* flow label (20 bits) */
#endif

#define IPPROTO_HOPOPTS         0               /* IP6 hop-by-hop options */
#define IPPROTO_TCP             6               /* tcp */
#define IPPROTO_UDP             17              /* user datagram protocol */
#define IPPROTO_IPV6            41              /* IP6 header */
#define IPPROTO_ROUTING         43              /* IP6 routing header */
#define IPPROTO_FRAGMENT        44              /* IP6 fragmentation header */
#define IPPROTO_ICMPV6          58              /* ICMP6 */
#define IPPROTO_NONE            59              /* IP6 no next header */
#define IPPROTO_DSTOPTS         60              /* IP6 destination option */

/*
 * Extension Headers
 */

typedef struct ip6_ext {
  uint8_t ip6e_nxt;
  uint8_t ip6e_len;
} Ip6ExtensionHeader;

/* Hop-by-Hop options header */
typedef struct ip6_hbh {
  uint8_t ip6h_nxt;      /* next header */
  uint8_t ip6h_len;      /* length in units of 8 octets */
  /* followed by options */
} Ip6HopByHopHeader;

/* Destination options header */
typedef struct ip6_dest {
  uint8_t ip6d_nxt;      /* next header */
  uint8_t ip6d_len;      /* length in units of 8 octets */
  /* followed by options */
} Ip6DestinationHeader;

/* Option types and related macros */
#define IP6OPT_PAD1             0x00    /* 00 0 00000 */
#define IP6OPT_PADN             0x01    /* 00 0 00001 */
#define IP6OPT_JUMBO            0xC2    /* 11 0 00010 = 194 */
#define IP6OPT_NSAP_ADDR        0xC3    /* 11 0 00011 */
#define IP6OPT_TUNNEL_LIMIT     0x04    /* 00 0 00100 */
#define IP6OPT_RTALERT          0x05    /* 00 0 00101 (KAME definition) */
#define IP6OPT_ROUTER_ALERT     0x05    /* 00 0 00101 (RFC3542, recommended) */

#define IP6OPT_RTALERT_LEN      4
#define IP6OPT_RTALERT_MLD      0       /* Datagram contains an MLD message */
#define IP6OPT_RTALERT_RSVP     1       /* Datagram contains an RSVP message */
#define IP6OPT_RTALERT_ACTNET   2       /* contains an Active Networks msg */
#define IP6OPT_MINLEN           2

#define IP6OPT_EID              0x8a    /* 10 0 01010 */

#define IP6OPT_TYPE(o)          ((o) & 0xC0)
#define IP6OPT_TYPE_SKIP        0x00
#define IP6OPT_TYPE_DISCARD     0x40
#define IP6OPT_TYPE_FORCEICMP   0x80
#define IP6OPT_TYPE_ICMP        0xC0

#define IP6OPT_MUTABLE          0x20

/* IPv6 options: common part */
typedef struct ip6_opt {
  uint8_t ip6o_type;
  uint8_t ip6o_len;
} Ip6OptionHeader;

typedef struct ip6_frag {
  uint8_t  ip6f_nxt;             /* next header */
  uint8_t  ip6f_reserved;        /* reserved field */
  uint16_t ip6f_offlg;           /* offset, reserved, and flag */
  uint32_t ip6f_ident;           /* identification */
} Ip6FragmentHeader;

#if BYTE_ORDER_BIG_ENDIAN
#define IP6F_OFF_MASK           0xfff8  /* mask out offset from _offlg */
#define IP6F_RESERVED_MASK      0x0006  /* reserved bits in ip6f_offlg */
#define IP6F_MORE_FRAG          0x0001  /* more-fragments flag */
#else /* BYTE_ORDER_LITTLE_ENDIAN */
#define IP6F_OFF_MASK           0xf8ff  /* mask out offset from _offlg */
#define IP6F_RESERVED_MASK      0x0600  /* reserved bits in ip6f_offlg */
#define IP6F_MORE_FRAG          0x0100  /* more-fragments flag */
#endif

}  // namespace Thread

#endif  // NET_IP6_H_
