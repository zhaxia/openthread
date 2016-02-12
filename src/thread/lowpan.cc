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
#include <common/thread_error.h>
#include <net/ip6.h>
#include <net/udp6.h>
#include <thread/lowpan.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

Lowpan::Lowpan(NetworkDataLeader *network_data) {
  network_data_ = network_data;
}

ThreadError CopyContext(const Context &context, Ip6Address *address) {
  memcpy(address, context.prefix, context.prefix_length/8);
  for (int i = (context.prefix_length & ~7); i < context.prefix_length; i++) {
    address->s6_addr[i/8] &= ~(0x80 >> (i % 8));
    address->s6_addr[i/8] |= context.prefix[i/8] & (0x80 >> (i % 8));
  }
  return kThreadError_None;
}

ThreadError ComputeIID(const MacAddress *macaddr, const Context &context, uint8_t *iid) {
  switch (macaddr->length) {
    case 2:
      iid[0] = 0x00;
      iid[1] = 0x00;
      iid[2] = 0x00;
      iid[3] = 0xff;
      iid[4] = 0xfe;
      iid[5] = 0x00;
      iid[6] = macaddr->address16 >> 8;
      iid[7] = macaddr->address16;
      break;
    case 8:
      memcpy(iid, &macaddr->address64, sizeof(macaddr->address64));
      iid[0] ^= 0x02;
      break;
    default:
      assert(false);
  }

  if (context.prefix_length > 64) {
    for (int i = (context.prefix_length & ~7); i < context.prefix_length; i++) {
      iid[i/8] &= ~(0x80 >> (i % 8));
      iid[i/8] |= context.prefix[i/8] & (0x80 >> (i % 8));
    }
  }

  return kThreadError_None;
}

int Lowpan::Compress(Message *message, const MacAddress *macsrc, const MacAddress *macdst, uint8_t *buf) {
  uint8_t *cur = buf;
  uint16_t hc_ctl = 0;
  Ip6Header ip6_header;
  uint8_t *ip6_header_bytes = reinterpret_cast<uint8_t*>(&ip6_header);
  Context src_context, dst_context;
  bool src_context_valid = true, dst_context_valid = true;

  message->Read(0, sizeof(ip6_header), &ip6_header);

  if (network_data_->GetContext(ip6_header.ip6_src, &src_context) != kThreadError_None) {
    network_data_->GetContext(0, &src_context);
    src_context_valid = false;
  }

  if (network_data_->GetContext(ip6_header.ip6_dst, &dst_context) != kThreadError_None) {
    network_data_->GetContext(0, &dst_context);
    dst_context_valid = false;
  }

  hc_ctl = kHcDispatch;

  // Lowpan HC Control Bits
  cur += 2;

  // Context Identifier
  if (src_context.context_id != 0 || dst_context.context_id != 0) {
    hc_ctl |= kHcContextId;
    cur[0] = (src_context.context_id << 4) | dst_context.context_id;
    cur++;
  }

  // Traffic Class
  if (((ip6_header_bytes[0] & 0x0f) == 0) && ((ip6_header_bytes[1] & 0xf0) == 0))
    hc_ctl |= kHcTrafficClass;

  // Flow Label
  if (((ip6_header_bytes[1] & 0x0f) == 0) && ((ip6_header_bytes[2]) == 0) && ((ip6_header_bytes[3]) == 0))
    hc_ctl |= kHcFlowLabel;

  if ((hc_ctl & kHcTrafficFlowMask) != kHcTrafficFlow) {
    cur[0] = (ip6_header_bytes[1] >> 4) << 6;
    if ((hc_ctl & kHcTrafficClass) == 0) {
      cur[0] |= ((ip6_header_bytes[0] & 0x0f) << 2) | (ip6_header_bytes[1] >> 6);
      cur++;
    }
    if ((hc_ctl & kHcFlowLabel) == 0) {
      cur[0] |= ip6_header_bytes[1] & 0x0f;
      cur[1] = ip6_header_bytes[2];
      cur[2] = ip6_header_bytes[3];
      cur += 3;
    }
  }

  // Next Header
  switch (ip6_header.ip6_nxt) {
    case IPPROTO_HOPOPTS:
    case IPPROTO_UDP:
      hc_ctl |= kHcNextHeader;
      break;
    default:
      cur[0] = ip6_header.ip6_nxt;
      cur++;
      break;
  }

  // Hop Limit
  switch (ip6_header.ip6_hlim) {
    case 1: hc_ctl |= kHcHopLimit1; break;
    case 64: hc_ctl |= kHcHopLimit64; break;
    case 255: hc_ctl |= kHcHopLimit255; break;
    default: cur[0] = ip6_header.ip6_hlim; cur++; break;
  }

  // Source Address
  if (ip6_header.ip6_src.IsLinkLocal()) {
    goto src_iid;
  } else if (ip6_header.ip6_src.IsUnspecified()) {
    hc_ctl |= kHcSrcAddrContext;
    goto dst_addr;
  } else if (src_context_valid) {
    hc_ctl |= kHcSrcAddrContext;
    goto src_iid;
  } else {
    memcpy(cur, ip6_header.ip6_src.s6_addr, sizeof(ip6_header.ip6_src));
    cur += 16;
    goto dst_addr;
  }

src_iid:
  uint8_t iid[8];
  ComputeIID(macsrc, src_context, iid);
  if (memcmp(iid, ip6_header.ip6_src.s6_addr + 8, 8) == 0) {
    hc_ctl |= kHcSrcAddrMode3;
    goto dst_addr;
  }

  MacAddress macsrc_tmp;
  macsrc_tmp.length = 2;
  macsrc_tmp.address16 = HostSwap16(ip6_header.ip6_src.s6_addr16[7]);
  ComputeIID(&macsrc_tmp, src_context, iid);
  if (memcmp(iid, ip6_header.ip6_src.s6_addr + 8, 8) == 0) {
    hc_ctl |= kHcSrcAddrMode2;
    cur[0] = ip6_header.ip6_src.s6_addr[14];
    cur[1] = ip6_header.ip6_src.s6_addr[15];
    cur += 2;
    goto dst_addr;
  }

  hc_ctl |= kHcSrcAddrMode1;
  memcpy(cur, ip6_header.ip6_src.s6_addr + 8, 8);
  cur += 8;

dst_addr:
  if (ip6_header.ip6_dst.IsMulticast())
    goto dst_addr_multicast;

  if (ip6_header.ip6_dst.IsLinkLocal()) {
    goto dst_iid;
  } else if (dst_context_valid) {
    hc_ctl |= kHcDstAddrContext;
    goto dst_iid;
  } else {
    memcpy(cur, &ip6_header.ip6_dst, sizeof(ip6_header.ip6_dst));
    cur += 16;
    goto next_header;
  }

dst_iid:
  ComputeIID(macdst, dst_context, iid);
  if (memcmp(iid, ip6_header.ip6_dst.s6_addr + 8, 8) == 0) {
    hc_ctl |= kHcDstAddrMode3;
    goto next_header;
  }

  MacAddress macdst_tmp;
  macdst_tmp.length = 2;
  macdst_tmp.address16 = HostSwap16(ip6_header.ip6_dst.s6_addr16[7]);
  ComputeIID(&macdst_tmp, dst_context, iid);
  if (memcmp(iid, ip6_header.ip6_dst.s6_addr + 8, 8) == 0) {
    hc_ctl |= kHcDstAddrMode2;
    cur[0] = ip6_header.ip6_dst.s6_addr[14];
    cur[1] = ip6_header.ip6_dst.s6_addr[15];
    cur += 2;
    goto next_header;
  }

  hc_ctl |= kHcDstAddrMode1;
  memcpy(cur, ip6_header.ip6_dst.s6_addr + 8, 8);
  cur += 8;

  goto next_header;

dst_addr_multicast:
  hc_ctl |= kHcMulticast;
  for (int i = 2; i < 16; i++) {
    if (ip6_header.ip6_dst.s6_addr[i]) {
      if (ip6_header.ip6_dst.s6_addr[1] == 0x02 && i >= 15) {
        hc_ctl |= kHcDstAddrMode3;
        cur[0] = ip6_header.ip6_dst.s6_addr[15];
        cur++;
      } else if (i >= 13) {
        hc_ctl |= kHcDstAddrMode2;
        cur[0] = ip6_header.ip6_dst.s6_addr[1];
        memcpy(cur + 1, ip6_header.ip6_dst.s6_addr + 13, 3);
        cur += 4;
      } else if (i >= 9) {
        hc_ctl |= kHcDstAddrMode1;
        cur[0] = ip6_header.ip6_dst.s6_addr[1];
        memcpy(cur + 1, ip6_header.ip6_dst.s6_addr + 11, 5);
        cur += 6;
      } else {
        memcpy(cur, ip6_header.ip6_dst.s6_addr, 16);
        cur += 16;
      }
      break;
    }
  }

next_header:
  buf[0] = hc_ctl >> 8;
  buf[1] = hc_ctl;
  message->SetOffset(sizeof(ip6_header));

  uint8_t next_header = ip6_header.ip6_nxt;
  while (1) {
    switch (next_header) {
      case IPPROTO_HOPOPTS:
        cur += CompressExtensionHeader(message, cur, &next_header);
        break;
      case IPPROTO_UDP:
        cur += CompressUdp(message, cur);
        ExitNow();
      default:
        ExitNow();
    }
  }

exit:
  return cur - buf;
}

int Lowpan::CompressExtensionHeader(Message *message, uint8_t *buf, uint8_t *next_header) {
  uint8_t *cur = buf;

  Ip6ExtensionHeader ext_header;
  message->Read(message->GetOffset(), sizeof(ext_header), &ext_header);
  message->MoveOffset(sizeof(ext_header));

  cur[0] = kExtHdrDispatch | kExtHdrEidHbh;
  *next_header = ext_header.ip6e_nxt;

  switch (ext_header.ip6e_nxt) {
    case IPPROTO_UDP:
      cur[0] |= kExtHdrNextHeader;
      break;
    default:
      cur++;
      cur[0] = ext_header.ip6e_nxt;
      break;
  }
  cur++;

  uint8_t len = 8*(ext_header.ip6e_len + 1) - sizeof(ext_header);
  cur[0] = len;
  cur++;

  message->Read(message->GetOffset(), len, cur);
  message->MoveOffset(len);
  cur += len;

  return cur - buf;
}

int Lowpan::CompressUdp(Message *message, uint8_t *buf) {
  uint8_t *cur = buf;

  UdpHeader udp_header;
  message->Read(message->GetOffset(), sizeof(udp_header), &udp_header);

  cur[0] = kUdpDispatch;
  cur++;

  memcpy(cur, &udp_header.source, sizeof(udp_header.source));
  cur += sizeof(udp_header.source);
  memcpy(cur, &udp_header.dest, sizeof(udp_header.dest));
  cur += sizeof(udp_header.dest);
  memcpy(cur, &udp_header.check, sizeof(udp_header.check));
  cur += sizeof(udp_header.check);

  message->MoveOffset(sizeof(udp_header));

  return cur - buf;
}

int Lowpan::DispatchToNextHeader(uint8_t dispatch) {
  if ((dispatch & kExtHdrDispatchMask) == kExtHdrDispatch) {
    switch (dispatch & kExtHdrEidMask) {
      case kExtHdrEidHbh:
        return IPPROTO_HOPOPTS;
      case kExtHdrEidRouting:
        return IPPROTO_ROUTING;
      case kExtHdrEidFragment:
        return IPPROTO_FRAGMENT;
      case kExtHdrEidDst:
        return IPPROTO_DSTOPTS;
      case kExtHdrEidIp6:
        return IPPROTO_IPV6;
    }
  } else if ((dispatch & kUdpDispatchMask) == kUdpDispatch) {
    return IPPROTO_UDP;
  }

  return -1;
}

int Lowpan::DecompressBaseHeader(Ip6Header *ip6_header, const MacAddress *macsrc, const MacAddress *macdst,
                                 const uint8_t* buf) {
  ThreadError error = kThreadError_None;
  const uint8_t *cur = buf;
  uint16_t hc_ctl;
  Context src_context, dst_context;
  bool src_context_valid = true, dst_context_valid = true;

  hc_ctl = (static_cast<uint16_t>(cur[0]) << 8) | cur[1];
  cur += 2;

  // check Dispatch bits
  VerifyOrExit((hc_ctl & kHcDispatchMask) == kHcDispatch, error = kThreadError_Parse);

  // Context Identifier
  src_context.prefix_length = 0;
  dst_context.prefix_length = 0;

  if ((hc_ctl & kHcContextId) != 0) {
    if (network_data_->GetContext(cur[0] >> 4, &src_context) != kThreadError_None)
      src_context_valid = false;
    if (network_data_->GetContext(cur[0] & 0xf, &dst_context) != kThreadError_None)
      dst_context_valid = false;
    cur++;
  } else {
    network_data_->GetContext(0, &src_context);
    network_data_->GetContext(0, &dst_context);
  }

  memset(ip6_header, 0, sizeof(*ip6_header));
  ip6_header->ip6_vfc = IPV6_VERSION;

  // Traffic Class and Flow Label
  if ((hc_ctl & kHcTrafficFlowMask) != kHcTrafficFlow) {
    uint8_t *bytes = reinterpret_cast<uint8_t*>(ip6_header);
    bytes[1] |= (cur[0] & 0xc0) >> 2;
    if ((hc_ctl & kHcTrafficClass) == 0) {
      bytes[0] |= (cur[0] >> 2) & 0x0f;
      bytes[1] |= (cur[0] << 6) & 0xc0;
      cur++;
    }
    if ((hc_ctl & kHcFlowLabel) == 0) {
      bytes[1] |= cur[0] & 0x0f;
      bytes[2] |= cur[1];
      bytes[3] |= cur[2];
      cur += 3;
    }
  }

  // Next Header
  if ((hc_ctl & kHcNextHeader) == 0) {
    ip6_header->ip6_nxt = cur[0];
    cur++;
  }

  // Hop Limit
  switch (hc_ctl & kHcHopLimitMask) {
    case kHcHopLimit1:
      ip6_header->ip6_hlim = 1;
      break;
    case kHcHopLimit64:
      ip6_header->ip6_hlim = 64;
      break;
    case kHcHopLimit255:
      ip6_header->ip6_hlim = 255;
      break;
    default:
      ip6_header->ip6_hlim = cur[0];
      cur++;
      break;
  }

  // Source Address
  switch (hc_ctl & kHcSrcAddrModeMask) {
    case kHcSrcAddrMode0:
      if ((hc_ctl & kHcSrcAddrContext) == 0) {
        memcpy(&ip6_header->ip6_src, cur, sizeof(ip6_header->ip6_src));
        cur += 16;
      }
      break;
    case kHcSrcAddrMode1:
      memcpy(ip6_header->ip6_src.s6_addr + 8, cur, 8);
      cur += 8;
      break;
    case kHcSrcAddrMode2:
      ip6_header->ip6_src.s6_addr[11] = 0xff;
      ip6_header->ip6_src.s6_addr[12] = 0xfe;
      memcpy(ip6_header->ip6_src.s6_addr + 14, cur, 2);
      cur += 2;
      break;
    case kHcSrcAddrMode3:
      ComputeIID(macsrc, src_context, ip6_header->ip6_src.s6_addr + 8);
      break;
  }

  if ((hc_ctl & kHcSrcAddrContext) == 0) {
    if ((hc_ctl & kHcSrcAddrModeMask) != 0)
      ip6_header->ip6_src.s6_addr16[0] = HostSwap16(0xfe80);
  } else {
    VerifyOrExit(src_context_valid, error = kThreadError_Parse);
    CopyContext(src_context, &ip6_header->ip6_src);
  }

  if ((hc_ctl & kHcMulticast) == 0) {
    // Unicast Destination Address

    switch (hc_ctl & kHcDstAddrModeMask) {
      case kHcDstAddrMode0:
        memcpy(&ip6_header->ip6_dst, cur, sizeof(ip6_header->ip6_dst));
        cur += 16;
        break;
      case kHcDstAddrMode1:
        memcpy(ip6_header->ip6_dst.s6_addr + 8, cur, 8);
        cur += 8;
        break;
      case kHcDstAddrMode2:
        ip6_header->ip6_dst.s6_addr[11] = 0xff;
        ip6_header->ip6_dst.s6_addr[12] = 0xfe;
        memcpy(ip6_header->ip6_dst.s6_addr + 14, cur, 2);
        cur += 2;
        break;
      case kHcDstAddrMode3:
        ComputeIID(macdst, dst_context, ip6_header->ip6_dst.s6_addr + 8);
        break;
    }

    if ((hc_ctl & kHcDstAddrContext) == 0) {
      if ((hc_ctl & kHcDstAddrModeMask) != 0)
        ip6_header->ip6_dst.s6_addr16[0] = HostSwap16(0xfe80);
    } else {
      VerifyOrExit(dst_context_valid, error = kThreadError_Parse);
      CopyContext(dst_context, &ip6_header->ip6_dst);
    }
  } else {
    // Multicast Destination Address

    ip6_header->ip6_dst.s6_addr[0] = 0xff;

    if ((hc_ctl & kHcDstAddrContext) == 0) {
      switch (hc_ctl & kHcDstAddrModeMask) {
        case kHcDstAddrMode0:
          memcpy(ip6_header->ip6_dst.s6_addr, cur, 16);
          cur += 16;
          break;
        case kHcDstAddrMode1:
          ip6_header->ip6_dst.s6_addr[1] = cur[0];
          memcpy(ip6_header->ip6_dst.s6_addr + 11, cur + 1, 5);
          cur += 6;
          break;
        case kHcDstAddrMode2:
          ip6_header->ip6_dst.s6_addr[1] = cur[0];
          memcpy(ip6_header->ip6_dst.s6_addr + 13, cur + 1, 3);
          cur += 4;
          break;
        case kHcDstAddrMode3:
          ip6_header->ip6_dst.s6_addr[1] = 0x02;
          ip6_header->ip6_dst.s6_addr[15] = cur[0];
          cur++;
          break;
      }
    } else {
      switch (hc_ctl & kHcDstAddrModeMask) {
        case 0:
          VerifyOrExit(dst_context_valid, error = kThreadError_Parse);
          ip6_header->ip6_dst.s6_addr[1] = cur[0];
          ip6_header->ip6_dst.s6_addr[2] = cur[1];
          memcpy(ip6_header->ip6_dst.s6_addr + 8, dst_context.prefix, 8);
          memcpy(ip6_header->ip6_dst.s6_addr + 12, cur + 2, 4);
          cur += 6;
          break;
        default:
          ExitNow(error = kThreadError_Parse);
      }
    }
  }

  if ((hc_ctl & kHcNextHeader) != 0)
    ip6_header->ip6_nxt = DispatchToNextHeader(cur[0]);

exit:
  if (error != kThreadError_None)
    return -1;

  return cur - buf;
}

int Lowpan::DecompressExtensionHeader(Message *message, const uint8_t *buf, uint16_t buf_length) {
  const uint8_t *cur = buf;
  uint8_t hdr[2];
  uint8_t len;

  uint8_t ctl = cur[0];
  cur++;

  // next header
  if (ctl & kExtHdrNextHeader) {
    len = cur[0];
    cur++;

    hdr[0] = DispatchToNextHeader(cur[len]);
  } else {
    hdr[0] = cur[0];
    cur++;

    len = cur[0];
    cur++;
  }

  // length
  hdr[1] = ((sizeof(hdr) + len + 7) / 8) - 1;

  message->Append(hdr, sizeof(hdr));
  message->MoveOffset(sizeof(hdr));

  // payload
  message->Append(cur, len);
  message->MoveOffset(len);
  cur += len;

  return cur - buf;
}

int Lowpan::DecompressUdpHeader(Message *message, const uint8_t *buf, uint16_t buf_length, uint16_t datagram_length) {
  ThreadError error = kThreadError_None;
  const uint8_t *cur = buf;

  UdpHeader udp_header;
  uint8_t udp_ctl = cur[0];
  cur++;

  memset(&udp_header, 0, sizeof(udp_header));

  // source and dest ports
  switch (udp_ctl & kUdpPortMask) {
    case 0:
      udp_header.source = HostSwap16((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
      udp_header.dest = HostSwap16((static_cast<uint16_t>(cur[2]) << 8) | cur[3]);
      cur += 4;
      break;
    case 1:
      udp_header.source = HostSwap16((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
      udp_header.dest = HostSwap16(0xf000 | cur[2]);
      cur += 3;
      break;
    case 2:
      udp_header.source = HostSwap16(0xf000 | cur[0]);
      udp_header.dest = HostSwap16((static_cast<uint16_t>(cur[2]) << 8) | cur[1]);
      cur += 3;
      break;
    case 3:
      udp_header.source = HostSwap16(0xf000 | cur[0]);
      udp_header.dest = HostSwap16(0xf000 | cur[1]);
      cur += 2;
      break;
  }

  // checksum
  if ((udp_ctl & kUdpChecksum) != 0) {
    ExitNow(error = kThreadError_Parse);
  } else {
    udp_header.check = HostSwap16((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
    cur += 2;
  }

  // length
  if (datagram_length == 0)
    udp_header.len = HostSwap16(sizeof(udp_header) + (buf_length - (cur - buf)));
  else
    udp_header.len = HostSwap16(datagram_length - message->GetOffset());

  message->Append(&udp_header, sizeof(udp_header));
  message->MoveOffset(sizeof(udp_header));

exit:
  if (error != kThreadError_None)
    return -1;
  return cur - buf;
}

int Lowpan::Decompress(Message *message, const MacAddress *macsrc, const MacAddress *macdst,
                       const uint8_t* buf, uint16_t buf_len, uint16_t datagram_length) {
  ThreadError error = kThreadError_None;
  Ip6Header ip6_header;
  const uint8_t *cur = buf;
  bool compressed;
  int rval;

  compressed = (((static_cast<uint16_t>(cur[0]) << 8) | cur[1]) & kHcNextHeader) != 0;

  VerifyOrExit((rval = DecompressBaseHeader(&ip6_header, macsrc, macdst, buf)) >= 0, ;);
  cur += rval;

  SuccessOrExit(error = message->Append(&ip6_header, sizeof(ip6_header)));
  SuccessOrExit(error = message->SetOffset(sizeof(ip6_header)));

  while (compressed) {
    if ((cur[0] & kExtHdrDispatchMask) == kExtHdrDispatch) {
      compressed = (cur[0] & kExtHdrNextHeader) != 0;
      VerifyOrExit((rval = DecompressExtensionHeader(message, cur, buf_len - (cur - buf))) >= 0,
                   error = kThreadError_Parse);
    } else if ((cur[0] & kUdpDispatchMask) == kUdpDispatch) {
      compressed = false;
      VerifyOrExit((rval = DecompressUdpHeader(message, cur, buf_len - (cur - buf), datagram_length)) >= 0,
                   error = kThreadError_Parse);
    } else {
      ExitNow(error = kThreadError_Parse);
    }

    cur += rval;
  }

exit:
  if (error != kThreadError_None)
    return -1;
  return cur - buf;
}

}  // namespace Thread
