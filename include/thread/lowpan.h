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

#ifndef THREAD_LOWPAN_H_
#define THREAD_LOWPAN_H_

#include <common/message.h>
#include <mac/mac_frame.h>
#include <net/ip6.h>
#include <net/ip6_address.h>
#include <thread/network_data_leader.h>

namespace Thread {

class Lowpan {
 public:
  enum {
    kHopsLeft = 15,
  };

  enum {
    kHcDispatch           = 3 << 13,
    kHcDispatchMask       = 7 << 13,
  };

  explicit Lowpan(NetworkDataLeader *network_data);
  int Compress(Message *message, const MacAddress *macsrc, const MacAddress *macdst, uint8_t *buf);
  int CompressExtensionHeader(Message *message, uint8_t *buf, uint8_t *next_header);
  int CompressUdp(Message *message, uint8_t *buf);
  int Decompress(Message *message, const MacAddress *macsrc, const MacAddress *macdst,
                 const uint8_t* buf, uint16_t buf_len, uint16_t datagram_len);
  int DecompressBaseHeader(Ip6Header *header, const MacAddress *macsrc, const MacAddress *macdst, const uint8_t* buf);
  int DecompressExtensionHeader(Message *message, const uint8_t *buf, uint16_t buf_length);
  int DecompressUdpHeader(Message *message, const uint8_t *buf, uint16_t buf_length, uint16_t datagram_length);
  int DispatchToNextHeader(uint8_t dispatch);

 private:
  enum {
    kHcTrafficClass    = 1 << 11,
    kHcFlowLabel       = 2 << 11,
    kHcTrafficFlow     = 3 << 11,
    kHcTrafficFlowMask = 3 << 11,
    kHcNextHeader      = 1 << 10,
    kHcHopLimit1       = 1 << 8,
    kHcHopLimit64      = 2 << 8,
    kHcHopLimit255     = 3 << 8,
    kHcHopLimitMask    = 3 << 8,
    kHcContextId       = 1 << 7,
    kHcSrcAddrContext  = 1 << 6,
    kHcSrcAddrMode0    = 0 << 4,
    kHcSrcAddrMode1    = 1 << 4,
    kHcSrcAddrMode2    = 2 << 4,
    kHcSrcAddrMode3    = 3 << 4,
    kHcSrcAddrModeMask = 3 << 4,
    kHcMulticast       = 1 << 3,
    kHcDstAddrContext  = 1 << 2,
    kHcDstAddrMode0    = 0 << 0,
    kHcDstAddrMode1    = 1 << 0,
    kHcDstAddrMode2    = 2 << 0,
    kHcDstAddrMode3    = 3 << 0,
    kHcDstAddrModeMask = 3 << 0,

    kExtHdrDispatch     = 0xe0,
    kExtHdrDispatchMask = 0xf0,

    kExtHdrEidHbh       = 0x00,
    kExtHdrEidRouting   = 0x02,
    kExtHdrEidFragment  = 0x04,
    kExtHdrEidDst       = 0x06,
    kExtHdrEidMobility  = 0x08,
    kExtHdrEidIp6       = 0x0e,
    kExtHdrEidMask      = 0x0e,

    kExtHdrNextHeader   = 0x01,

    kUdpDispatch       = 0xf0,
    kUdpDispatchMask   = 0xf8,
    kUdpChecksum       = 1 << 2,
    kUdpPortMask       = 3 << 0,
  };

  NetworkDataLeader *network_data_;
};

class MeshHeader {
 public:
  enum {
    kDispatch         = 2 << 6,
    kDispatchMask     = 3 << 6,
    kHopsLeftMask     = 0xf << 0,
    kSourceShort      = 1 << 5,
    kDestinationShort = 1 << 4,
  };

  void Init() { m_dispatch_hops_left = kDispatch | kSourceShort | kDestinationShort; }
  bool IsValid() { return (m_dispatch_hops_left & kSourceShort) && (m_dispatch_hops_left & kDestinationShort); }

  uint8_t GetHeaderLength() { return sizeof(*this); }

  uint8_t GetHopsLeft() { return m_dispatch_hops_left & kHopsLeftMask; }
  void SetHopsLeft(uint8_t hops) { m_dispatch_hops_left = (m_dispatch_hops_left & ~kHopsLeftMask) | hops; }

  uint16_t GetSource() { return HostSwap16(m_source); }
  void SetSource(uint16_t source) { m_source = HostSwap16(source); }

  uint16_t GetDestination() { return HostSwap16(m_destination); }
  void SetDestination(uint16_t destination) { m_destination = HostSwap16(destination); }

 private:
  uint8_t m_dispatch_hops_left;
  uint16_t m_source;
  uint16_t m_destination;
} __attribute__((packed));

class FragmentHeader {
 public:
  enum {
    kDispatch     = 3 << 6,
    kDispatchMask = 3 << 6,
    kOffset       = 1 << 5,
    kSizeMask     = 7 << 0,
  };

  void Init() {
    m_dispatch_offset_size = kDispatch;
  }

  uint8_t GetHeaderLength() {
    return (m_dispatch_offset_size & kOffset) ? sizeof(*this) : sizeof(*this) - sizeof(m_offset);
  }

  uint16_t GetSize() {
    return (static_cast<uint16_t>(m_dispatch_offset_size & kSizeMask) << 8) | m_size;
  }

  void SetSize(uint16_t size) {
    m_dispatch_offset_size = (m_dispatch_offset_size & ~kSizeMask) | ((size >> 8) & kSizeMask);
    m_size = size;
  }

  uint16_t GetTag() {
    return HostSwap16(m_tag);
  }

  void SetTag(uint16_t tag) {
    m_tag = HostSwap16(tag);
  }

  uint16_t GetOffset() {
    return (m_dispatch_offset_size & kOffset) ? static_cast<uint16_t>(m_offset) * 8 : 0;
  }

  void SetOffset(uint16_t offset) {
    if (offset == 0) {
      m_dispatch_offset_size &= ~kOffset;
    } else {
      m_dispatch_offset_size |= kOffset;
      m_offset = offset / 8;
    }
  }

 private:
  uint8_t m_dispatch_offset_size;
  uint8_t m_size;
  uint16_t m_tag;
  uint8_t m_offset;
} __attribute__((packed));

}  // namespace Thread

#endif  // THREAD_LOWPAN_H_
