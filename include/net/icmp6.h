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

#ifndef NET_ICMP6_H_
#define NET_ICMP6_H_

#include <net/ip6.h>

namespace Thread {

typedef struct icmp6_hdr {
  uint8_t        icmp6_type;     /* type field */
  uint8_t        icmp6_code;     /* code field */
  uint16_t       icmp6_cksum;    /* checksum field */
  union {
    uint32_t       icmp6_un_data32[1]; /* type-specific field */
    uint16_t       icmp6_un_data16[2]; /* type-specific field */
    uint8_t        icmp6_un_data8[4];  /* type-specific field */
  } icmp6_dataun;
} Icmp6Header;

class Icmp6 {
 public:
  class Callbacks {
    friend Icmp6;

   public:
    typedef void (*HandleDstUnreach)(void *context, Message *mesage, const Icmp6Header *icmp6_header,
                                     const Ip6MessageInfo *message_info);
    Callbacks(HandleDstUnreach handle_dst_unreach, void *context) {
      handle_dst_unreach_ = handle_dst_unreach;
      context_ = context;
    }

   private:
    HandleDstUnreach handle_dst_unreach_;
    void *context_;
    Callbacks *next_;
  };

  class EchoClient {
    friend class Icmp6;

   public:
    typedef void (*HandleEchoResponse)(void *context, Message *message, const Ip6MessageInfo *message_info);
    EchoClient(HandleEchoResponse handle_echo_response, void *context);
    ThreadError SendEchoRequest(const struct sockaddr_in6 *address, const void *payload, uint16_t payload_length);

   private:
    HandleEchoResponse handle_echo_response_;
    void *context_;
    uint16_t id_;
    uint16_t seq_;
    EchoClient *next_;
    static uint16_t next_id_;
  };

  static ThreadError RegisterCallbacks(Callbacks *callbacks);
  static ThreadError SendError(const Ip6Address *dst, uint8_t type, uint8_t code, const Ip6Header *ip6_header);
  static ThreadError HandleMessage(Message *message, Ip6MessageInfo *message_info);
  static ThreadError UpdateChecksum(Message *message, uint16_t pseudoheader_checksum);

 private:
  static ThreadError HandleDstUnreach(Message *message, const Icmp6Header *icmp6_header,
                                      const Ip6MessageInfo *message_info);
  static ThreadError HandleEchoRequest(Message *message, const Ip6MessageInfo *message_info);
  static ThreadError HandleEchoReply(Message *message, const Icmp6Header *icmp6_header,
                                     const Ip6MessageInfo *message_info);

  static Callbacks *callbacks_;
  static EchoClient *echo_clients_;
};

#define icmp6_data32    icmp6_dataun.icmp6_un_data32
#define icmp6_data16    icmp6_dataun.icmp6_un_data16
#define icmp6_data8     icmp6_dataun.icmp6_un_data8
#define icmp6_pptr      icmp6_data32[0]         /* parameter prob */
#define icmp6_mtu       icmp6_data32[0]         /* packet too big */
#define icmp6_id        icmp6_data16[0]         /* echo request/reply */
#define icmp6_seq       icmp6_data16[1]         /* echo request/reply */
#define icmp6_maxdelay  icmp6_data16[0]         /* mcast group membership */

#define ICMP6_DST_UNREACH               1       /* dest unreachable, codes: */
#define ICMP6_PACKET_TOO_BIG            2       /* packet too big */
#define ICMP6_TIME_EXCEEDED             3       /* time exceeded, code: */
#define ICMP6_PARAM_PROB                4       /* ip6 header bad */

#define ICMP6_ECHO_REQUEST              128     /* echo service */
#define ICMP6_ECHO_REPLY                129     /* echo reply */

#define ICMP6_DST_UNREACH_NOROUTE       0       /* no route to destination */
#define ICMP6_DST_UNREACH_ADMIN         1       /* administratively prohibited */
#define ICMP6_DST_UNREACH_NOTNEIGHBOR   2       /* not a neighbor(obsolete) */
#define ICMP6_DST_UNREACH_BEYONDSCOPE   2       /* beyond scope of source address */
#define ICMP6_DST_UNREACH_ADDR          3       /* address unreachable */
#define ICMP6_DST_UNREACH_NOPORT        4       /* port unreachable */

#define ICMP6_TIME_EXCEED_TRANSIT       0       /* ttl==0 in transit */
#define ICMP6_TIME_EXCEED_REASSEMBLY    1       /* ttl==0 in reass */

#define ICMP6_PARAMPROB_HEADER          0       /* erroneous header field */
#define ICMP6_PARAMPROB_NEXTHEADER      1       /* unrecognized next header */
#define ICMP6_PARAMPROB_OPTION          2       /* unrecognized option */

#define ICMP6_INFOMSG_MASK              0x80    /* all informational messages */

}  // namespace Thread

#endif  // NET_ICMP6_H_
