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

#ifndef THREAD_NETWORK_DATA_H_
#define THREAD_NETWORK_DATA_H_

#include <common/encoding.h>
#include <common/thread_error.h>
#include <net/ip6_address.h>
#include <stdint.h>
#include <string.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

class NetworkDataTlv {
 public:
  void Init() { type_ = 0; length_ = 0; }

  enum {
    kTypeHasRoute = 0,
    kTypePrefix = 1,
    kTypeBorderRouter = 2,
    kTypeContext = 3,
    kTypeCommissioningData = 4,
  };
  uint8_t GetType() { return type_ >> kTypeOffset; }
  void SetType(uint8_t type) { type_ = (type_ & ~kTypeMask) | (type << kTypeOffset); }

  uint8_t GetLength() { return length_; }
  void SetLength(uint8_t length) { length_ = length; }
  void AdjustLength(int diff) { length_ += diff; }

  uint8_t *GetValue() { return reinterpret_cast<uint8_t*>(this) + sizeof(NetworkDataTlv); }
  NetworkDataTlv *GetNext() {
    return reinterpret_cast<NetworkDataTlv*>(reinterpret_cast<uint8_t*>(this) + sizeof(*this) + length_);
  }

  void ClearStable() { type_ &= ~kStableMask; }
  bool IsStable() { return (type_ & kStableMask); }
  void SetStable() { type_ |= kStableMask; }

 private:
  enum {
    kTypeOffset = 1,
    kTypeMask = 0x7f << kTypeOffset,
    kStableMask = 1 << 0,
  };
  uint8_t type_;
  uint8_t length_;
} __attribute__((packed));

class PrefixTlv: public NetworkDataTlv {
 public:
  void Init(uint8_t domain_id, uint8_t prefix_length, const uint8_t *prefix) {
    NetworkDataTlv::Init();
    SetType(kTypePrefix);
    domain_id_ = domain_id;
    prefix_length_ = prefix_length;
    memcpy(GetPrefix(), prefix, (prefix_length+7)/8);
    SetSubTlvsLength(0);
  }

  uint8_t GetDomainId() { return domain_id_; }
  uint8_t GetPrefixLength() const { return prefix_length_; }
  uint8_t *GetPrefix() { return reinterpret_cast<uint8_t*>(this) + sizeof(*this); }

  uint8_t *GetSubTlvs() { return GetPrefix() + (prefix_length_+7)/8; }
  uint8_t GetSubTlvsLength() {
    return GetLength() - (sizeof(*this) - sizeof(NetworkDataTlv) + (prefix_length_+7)/8);
  }
  void SetSubTlvsLength(int length) {
    SetLength(sizeof(*this) - sizeof(NetworkDataTlv) + (prefix_length_+7)/8 + length);
  }

 private:
  uint8_t domain_id_;
  uint8_t prefix_length_;
} __attribute__((packed));

class BorderRouterEntry {
 public:
  enum {
    kPreferenceOffset = 6,
    kPreferenceMask = 3 << kPreferenceOffset,
    kPreferredFlag = 1 << 5,
    kValidFlag = 1 << 4,
    kDhcpFlag = 1 << 3,
    kConfigureFlag = 1 << 2,
    kDefaultRouteFlag = 1 << 1,
  };

  void Init() { SetRloc(0xfffe); flags_ = 0; reserved_ = 0; }

  uint16_t GetRloc() { return HostSwap16(rloc_); }
  void SetRloc(uint16_t rloc) { rloc_ = HostSwap16(rloc); }

  uint8_t GetFlags() { return flags_; }
  void SetFlags(uint8_t flags) { flags_ = flags; }

  uint8_t GetPreference() { return flags_ >> kPreferenceOffset; }
  void SetPreference(uint8_t prf) { flags_ = (flags_ & ~kPreferenceMask) | (prf << kPreferenceOffset); }

  bool IsPreferred() { return (flags_ & kPreferredFlag) != 0; }
  void ClearPreferred() { flags_ &= ~kPreferredFlag; }
  void SetPreferred() { flags_ |= kPreferredFlag; }

  bool IsValid() { return (flags_ & kValidFlag) != 0; }
  void ClearValid() { flags_ &= ~kValidFlag; }
  void SetValid() { flags_ |= kValidFlag; }

  bool IsDhcp() { return (flags_ & kDhcpFlag) != 0; }
  void ClearDhcp() { flags_ &= ~kDhcpFlag; }
  void SetDhcp() { flags_ |= kDhcpFlag; }

  bool IsConfigure() { return (flags_ & kConfigureFlag) != 0; }
  void ClearConfigure() { flags_ &= ~kConfigureFlag; }
  void SetConfigure() { flags_ |= kConfigureFlag; }

  bool IsDefaultRoute() { return (flags_ & kDefaultRouteFlag) != 0; }
  void ClearDefaultRoute() { flags_ &= ~kDefaultRouteFlag; }
  void SetDefaultRoute() { flags_ |= kDefaultRouteFlag; }

 private:
  uint16_t rloc_;
  uint8_t flags_;
  uint8_t reserved_;
} __attribute__((packed));

class BorderRouterTlv: public NetworkDataTlv {
 public:
  void Init() { NetworkDataTlv::Init(); SetType(kTypeBorderRouter); SetLength(0); }

  uint8_t GetNumEntries() { return GetLength() / sizeof(BorderRouterEntry); }

  BorderRouterEntry *GetEntry(int i) {
    return reinterpret_cast<BorderRouterEntry*>(GetValue() + (i * sizeof(BorderRouterEntry)));
  }
} __attribute__((packed));

class ContextTlv: public NetworkDataTlv {
 public:
  void Init() { NetworkDataTlv::Init(); SetType(kTypeContext); SetLength(2); flags_ = 0; context_length_ = 0; }

  bool IsCompress() { return (flags_ & kCompressFlag) != 0; }
  void ClearCompress() { flags_ &= ~kCompressFlag; }
  void SetCompress() { flags_ |= kCompressFlag; }

  uint8_t GetContextId() { return flags_ & kContextIdMask; }
  void SetContextId(uint8_t cid) { flags_ = (flags_ & ~kContextIdMask) | (cid << kContextIdOffset); }

  uint8_t GetContextLength() { return context_length_; }
  void SetContextLength(uint8_t length) { context_length_ = length; }

 private:
  enum {
    kCompressFlag = 1 << 4,
    kContextIdOffset = 0,
    kContextIdMask = 0xf << kContextIdOffset,
  };
  uint8_t flags_;
  uint8_t context_length_;
} __attribute__((packed));

struct Context {
  const uint8_t *prefix;
  uint8_t prefix_length;
  uint8_t context_id;
};

class NetworkData {
 public:
  ThreadError GetNetworkData(bool stable, uint8_t *data, uint8_t *data_length);

 protected:
  BorderRouterTlv *FindBorderRouter(PrefixTlv *prefix);
  BorderRouterTlv *FindBorderRouter(PrefixTlv *prefix, bool stable);
  PrefixTlv *FindPrefix(const uint8_t *prefix, uint8_t prefix_length);

  ThreadError Insert(uint8_t *start, uint8_t length);
  ThreadError Remove(uint8_t *start, uint8_t length);
  ThreadError RemoveTemporaryData(uint8_t *data, uint8_t *data_length);
  ThreadError RemoveTemporaryData(uint8_t *data, uint8_t *data_length, PrefixTlv *prefix);
  bool PrefixMatch(const uint8_t *a, const uint8_t *b, uint8_t prefix_length);

  uint8_t tlvs_[256];
  uint8_t length_ = 0;
};

}  // namespace Thread

#endif  // THREAD_NETWORK_DATA_H_
