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

#ifndef THREAD_MESHCOP_TLVS_H_
#define THREAD_MESHCOP_TLVS_H_

#include <common/message.h>
#include <mac/mac_frame.h>
#include <net/ip6_address.h>
#include <thread/mle.h>

namespace Thread {

enum {
  kCoapDefaultCommPort = 19779,
  kCoapDefaultFwdPort = 19782,
  kCoapDefaultJoinPort = 19786,
};

const char *kMeshcopUrl_CommPet = "c/cp";
const char *kMeshcopUrl_CommKa  = "c/ca";
const char *kMeshcopUrl_LeadPet = "c/lp";
const char *kMeshcopUrl_LeadKa  = "c/la";
const char *kMeshcopUrl_RelayRx = "c/rx";
const char *kMeshcopUrl_RelayTx = "c/tx";
const char *kMeshcopUrl_UdpRx   = "c/ur";
const char *kMeshcopUrl_UdpTx   = "c/ut";
const char *kMeshcopUrl_MgmtGet = "c/mg";
const char *kMeshcopUrl_MgmtSet = "c/ms";
const char *kMeshcopUrl_JoinEnt = "c/je";
const char *kMeshcopUrl_JoinFin = "c/jf";

struct MeshcopTlv {
  enum Type {
    kTypeChannel                 = 0,
    kTypePanId                   = 1,
    kTypeXPanId                  = 2,
    kTypeNetworkName             = 3,
    kTypePSKc                    = 4,
    kTypeMasterKey               = 5,
    kTypeKeySequence             = 6,
    kTypeMeshPrefix              = 7,
    kTypeSteeringData            = 8,
    kTypeBorderRloc              = 9,
    kTypeCommissionerId          = 10,
    kTypeCommissionerSessionId   = 11,
    kTypeCommissionerDataset     = 14,
    kTypeCommissionerUdpPort     = 15,
    kTypeSecurityPolicy          = 12,

    kTypeGet                     = 13,
    kTypeState                   = 16,
    kTypeJoinerDtls              = 17,
    kTypeJoinerUdpPort           = 18,
    kTypeJoinerIid               = 19,
    kTypeJoinerRloc              = 20,
    kTypeJoinerRouterKek         = 21,

    kTypeProvisioningUrl         = 32,
    kTypeVendorName              = 33,
    kTypeVendorModel             = 34,
    kTypeVendorSwVersion         = 35,
    kTypeVendorData              = 36,
    kTypeVendorStackVersion      = 37,

    kTypeUdpEncapsulation        = 48,
    kTypeIpv6Address             = 49,
    kTypeTmfForwardingPort       = 50,
  };
  uint8_t m_type;     ///< Type of value field in this TLV.
  uint8_t m_length;   ///< Length of value field in this TLV.

public:
    Type GetType() const { return static_cast<Type>(m_type); }
    void SetType(Type type) { m_type = static_cast<uint8_t>(type); }

    uint8_t GetFullLength() const { return m_length + sizeof(MeshcopTlv); }
    void SetFullLength(uint8_t length) { m_length = length - sizeof(MeshcopTlv); }

    void SetLength(uint8_t length) { m_length = length; }
    uint8_t GetLength() const { return m_length; }

    uint8_t *GetValue() { return reinterpret_cast<uint8_t *>(this) + sizeof(MeshcopTlv); }
    MeshcopTlv *GetNext() {
        return reinterpret_cast<MeshcopTlv *>(reinterpret_cast<uint8_t *>(this) + sizeof(*this) + m_length);
    }
    static ThreadError GetTlv(const Message &message, Type type, 
			      uint16_t max_length, MeshcopTlv &tlv);

} __attribute__((packed));

/// Channel TLV (0)
struct ThreadChannelTlv : public MeshcopTlv {
  uint16_t channel;
public:
  void Init() { SetType(kTypeChannel); SetFullLength(sizeof(*this)); }
  bool IsValid() const { return GetFullLength() == sizeof(*this); }
} __attribute__((packed));

/// PANID TLV (1)
struct ThreadPanIdTlv : public MeshcopTlv {
  uint16_t panid;
} __attribute__((packed));

/// XPANID TLV (2)
struct ThreadXPanIdTlv : public MeshcopTlv {
  uint8_t xpanid[8];
} __attribute__((packed));

/// Network Name TLV (3)
struct ThreadNetworkNameTlv : public MeshcopTlv {
  uint8_t name[16];
} __attribute__((packed));

/// PSKc TLV (4)
struct ThreadPSKcTlv : public MeshcopTlv {
  uint8_t pskc[16];
} __attribute__((packed));

/// Network Master Key TLV (5)
struct ThreadMasterKeyTlv : public MeshcopTlv {
  uint8_t master_key[16];
} __attribute__((packed));

/// Network Key Sequence TLV (6)
struct ThreadKeySequenceTlv : public MeshcopTlv {
  uint32_t key_seq;
} __attribute__((packed));

/// Network Mesh-Local Prefix TLV (7)
struct ThreadMeshPrefixTlv : public MeshcopTlv {
  uint8_t mesh_ula[8];
} __attribute__((packed));

/// Steering Data TLV (8)
struct ThreadSteeringDataTlv : public MeshcopTlv {
  uint8_t bloom[8];
} __attribute__((packed));

/// Border Router Locator TLV (9)
struct ThreadBorderRlocTlv : public MeshcopTlv {
  uint16_t address;
} __attribute__((packed));

/// Commissioner ID TLV (10)
struct ThreadCommissionerIdTlv : public MeshcopTlv {
  uint8_t id[64];
} __attribute__((packed));

/// Commissioner Session ID TLV (11)
struct ThreadCommissionerSessionIdTlv : public MeshcopTlv {
  uint16_t session;
} __attribute__((packed));

/// Commissioning Dataset Timestamp TLV (14)
struct ThreadCommissioningDatasetTimestampTlv : public MeshcopTlv {
  uint64_t timestamp;
} __attribute__((packed));

/// Commissioner UDP Port TLV (15)
struct ThreadCommissionerUdpPortTlv : public MeshcopTlv {
  uint16_t port;
} __attribute__((packed));

/// Security Policy TLV (12)
struct ThreadSecurityPolicyTlv : public MeshcopTlv {
  struct {
    uint8_t rsvd:6;
    uint8_t n:1;
    uint8_t o:1;
  } bits;
  uint16_t rotation_time;
} __attribute__((packed));


/// Get TLV (13)

/// State TLV (16)
struct ThreadStateTlv : public MeshcopTlv {
  enum {
    kAccept = 1,
    kPending = 0,
    kReject = -1,
  };
  uint8_t status;
} __attribute__((packed));


/// Joiner DTLS Encapsulation TLV (17)

/// Joiner UDP Port TLV (18)
struct ThreadJoinerUdpPortTlv : public MeshcopTlv {
  uint16_t port;
public:
  void Init() { SetType(kTypeJoinerUdpPort); SetFullLength(sizeof(*this)); }
  bool IsValid() const { return GetFullLength() == sizeof(*this); }
} __attribute__((packed));

/// Joiner IID TLV (19)
struct ThreadJoinerIidTlv : public MeshcopTlv {
  uint8_t m_iid[8];
public:
  void Init() { SetType(kTypeJoinerIid); SetFullLength(sizeof(*this)); }
  bool IsValid() const { return GetFullLength() == sizeof(*this); }
  
  const uint8_t *GetIid() const { return m_iid; }
  void SetIid(const uint8_t *iid) { memcpy(m_iid, iid, sizeof(m_iid)); }
} __attribute__((packed));


/// Joiner Router Locator TLV (20)
struct ThreadJoinerRlocTlv : public MeshcopTlv {
  uint16_t address;
public:
  void Init() { SetType(kTypeJoinerRloc); SetFullLength(sizeof(*this)); }
  bool IsValid() const { return GetFullLength() == sizeof(*this); }
} __attribute__((packed));

/// Joiner Router KEK TLV (21)
struct ThreadJoinerRouterKekTlv : public MeshcopTlv {
  uint8_t kek[16];
public:
  void Init() { SetType(kTypeJoinerRouterKek); SetFullLength(sizeof(*this)); }
  bool IsValid() const { return GetFullLength() == sizeof(*this); }
} __attribute__((packed));

/// Provisioning URL TLV (32)
struct ThreadProvisioningUrlTlv : public MeshcopTlv {
  uint8_t url[64];
} __attribute__((packed));

/// Vendor Name TLV (33)
struct ThreadVendorNameTlv : public MeshcopTlv {
  uint8_t url[32];
} __attribute__((packed));

/// Vendor Model TLV (34)
struct ThreadVendorModelTlv : public MeshcopTlv {
  uint8_t url[32];
} __attribute__((packed));

/// Vendor SW Version TLV (35)
struct ThreadVendorSwVersion : public MeshcopTlv {
  uint8_t version[16];
} __attribute__((packed));

/// Vendor Data TLV (36)
struct ThreadVendorDataTlv : public MeshcopTlv {
  uint8_t url[64];
} __attribute__((packed));

/// Vendor Stack Version TLV (37)
struct ThreadVendorStackVersionTlv : public MeshcopTlv {
  uint8_t oui[3];
  uint16_t build;
  uint8_t version;
} __attribute__((packed));

/// UDP Encapsulation TLV (48)
struct ThreadUdpEncapsulationTlv : public MeshcopTlv {
  uint16_t src_port;
  uint16_t dst_port;
  uint8_t payload[128];
} __attribute__((packed));

/// IPv6 Address TLV (49)
struct ThreadIpv6AddressTlv : public MeshcopTlv {
  uint8_t address[16];
} __attribute__((packed));

/// TMF Forwarding Port TLV (50)
struct ThreadForwardingUdpPortTlv : public MeshcopTlv {
  uint16_t port;
} __attribute__((packed));

}  // namespace Thread

#endif  // THREAD_THREAD_TLVS_H_
