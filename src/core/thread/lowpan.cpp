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

#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/thread_error.hpp>
#include <net/ip6.hpp>
#include <net/udp6.hpp>
#include <thread/lowpan.hpp>
#include <thread/thread_netif.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

Lowpan::Lowpan(ThreadNetif &netif)
{
    m_network_data = netif.GetNetworkDataLeader();
}

ThreadError CopyContext(const Context &context, Ip6Address &address)
{
    memcpy(&address, context.prefix, context.prefix_length / 8);

    for (int i = (context.prefix_length & ~7); i < context.prefix_length; i++)
    {
        address.addr8[i / 8] &= ~(0x80 >> (i % 8));
        address.addr8[i / 8] |= context.prefix[i / 8] & (0x80 >> (i % 8));
    }

    return kThreadError_None;
}

ThreadError ComputeIID(const Mac::Address &macaddr, const Context &context, uint8_t *iid)
{
    switch (macaddr.length)
    {
    case 2:
        iid[0] = 0x00;
        iid[1] = 0x00;
        iid[2] = 0x00;
        iid[3] = 0xff;
        iid[4] = 0xfe;
        iid[5] = 0x00;
        iid[6] = macaddr.address16 >> 8;
        iid[7] = macaddr.address16;
        break;

    case 8:
        memcpy(iid, &macaddr.address64, sizeof(macaddr.address64));
        iid[0] ^= 0x02;
        break;

    default:
        assert(false);
    }

    if (context.prefix_length > 64)
    {
        for (int i = (context.prefix_length & ~7); i < context.prefix_length; i++)
        {
            iid[i / 8] &= ~(0x80 >> (i % 8));
            iid[i / 8] |= context.prefix[i / 8] & (0x80 >> (i % 8));
        }
    }

    return kThreadError_None;
}

int Lowpan::CompressSourceIid(const Mac::Address &macaddr, const Ip6Address &ipaddr, const Context &context,
                              uint16_t &hc_ctl, uint8_t *buf)
{
    uint8_t *cur = buf;
    uint8_t iid[8];
    Mac::Address tmp;

    ComputeIID(macaddr, context, iid);

    if (memcmp(iid, ipaddr.addr8 + 8, 8) == 0)
    {
        hc_ctl |= kHcSrcAddrMode3;
    }
    else
    {
        tmp.length = 2;
        tmp.address16 = HostSwap16(ipaddr.addr16[7]);
        ComputeIID(tmp, context, iid);

        if (memcmp(iid, ipaddr.addr8 + 8, 8) == 0)
        {
            hc_ctl |= kHcSrcAddrMode2;
            cur[0] = ipaddr.addr8[14];
            cur[1] = ipaddr.addr8[15];
            cur += 2;
        }
        else
        {
            hc_ctl |= kHcSrcAddrMode1;
            memcpy(cur, ipaddr.addr8 + 8, 8);
            cur += 8;
        }
    }

    return cur - buf;
}

int Lowpan::CompressDestinationIid(const Mac::Address &macaddr, const Ip6Address &ipaddr, const Context &context,
                                   uint16_t &hc_ctl, uint8_t *buf)
{
    uint8_t *cur = buf;
    uint8_t iid[8];
    Mac::Address tmp;

    ComputeIID(macaddr, context, iid);

    if (memcmp(iid, ipaddr.addr8 + 8, 8) == 0)
    {
        hc_ctl |= kHcDstAddrMode3;
    }
    else
    {
        tmp.length = 2;
        tmp.address16 = HostSwap16(ipaddr.addr16[7]);
        ComputeIID(tmp, context, iid);

        if (memcmp(iid, ipaddr.addr8 + 8, 8) == 0)
        {
            hc_ctl |= kHcDstAddrMode2;
            cur[0] = ipaddr.addr8[14];
            cur[1] = ipaddr.addr8[15];
            cur += 2;
        }
        else
        {
            hc_ctl |= kHcDstAddrMode1;
            memcpy(cur, ipaddr.addr8 + 8, 8);
            cur += 8;
        }
    }

    return cur - buf;
}

int Lowpan::CompressMulticast(const Ip6Address &ipaddr, uint16_t &hc_ctl, uint8_t *buf)
{
    uint8_t *cur = buf;

    hc_ctl |= kHcMulticast;

    for (int i = 2; i < 16; i++)
    {
        if (ipaddr.addr8[i])
        {
            if (ipaddr.addr8[1] == 0x02 && i >= 15)
            {
                hc_ctl |= kHcDstAddrMode3;
                cur[0] = ipaddr.addr8[15];
                cur++;
            }
            else if (i >= 13)
            {
                hc_ctl |= kHcDstAddrMode2;
                cur[0] = ipaddr.addr8[1];
                memcpy(cur + 1, ipaddr.addr8 + 13, 3);
                cur += 4;
            }
            else if (i >= 9)
            {
                hc_ctl |= kHcDstAddrMode1;
                cur[0] = ipaddr.addr8[1];
                memcpy(cur + 1, ipaddr.addr8 + 11, 5);
                cur += 6;
            }
            else
            {
                memcpy(cur, ipaddr.addr8, 16);
                cur += 16;
            }

            break;
        }
    }

    return cur - buf;
}

int Lowpan::Compress(Message &message, const Mac::Address &macsrc, const Mac::Address &macdst, uint8_t *buf)
{
    uint8_t *cur = buf;
    uint16_t hc_ctl = 0;
    Ip6Header ip6_header;
    uint8_t *ip6_header_bytes = reinterpret_cast<uint8_t *>(&ip6_header);
    Context src_context, dst_context;
    bool src_context_valid = true, dst_context_valid = true;
    uint8_t next_header;

    message.Read(0, sizeof(ip6_header), &ip6_header);

    if (m_network_data->GetContext(*ip6_header.GetSource(), src_context) != kThreadError_None)
    {
        m_network_data->GetContext(0, src_context);
        src_context_valid = false;
    }

    if (m_network_data->GetContext(*ip6_header.GetDestination(), dst_context) != kThreadError_None)
    {
        m_network_data->GetContext(0, dst_context);
        dst_context_valid = false;
    }

    hc_ctl = kHcDispatch;

    // Lowpan HC Control Bits
    cur += 2;

    // Context Identifier
    if (src_context.context_id != 0 || dst_context.context_id != 0)
    {
        hc_ctl |= kHcContextId;
        cur[0] = (src_context.context_id << 4) | dst_context.context_id;
        cur++;
    }

    // Traffic Class
    if (((ip6_header_bytes[0] & 0x0f) == 0) && ((ip6_header_bytes[1] & 0xf0) == 0))
    {
        hc_ctl |= kHcTrafficClass;
    }

    // Flow Label
    if (((ip6_header_bytes[1] & 0x0f) == 0) && ((ip6_header_bytes[2]) == 0) && ((ip6_header_bytes[3]) == 0))
    {
        hc_ctl |= kHcFlowLabel;
    }

    if ((hc_ctl & kHcTrafficFlowMask) != kHcTrafficFlow)
    {
        cur[0] = (ip6_header_bytes[1] >> 4) << 6;

        if ((hc_ctl & kHcTrafficClass) == 0)
        {
            cur[0] |= ((ip6_header_bytes[0] & 0x0f) << 2) | (ip6_header_bytes[1] >> 6);
            cur++;
        }

        if ((hc_ctl & kHcFlowLabel) == 0)
        {
            cur[0] |= ip6_header_bytes[1] & 0x0f;
            cur[1] = ip6_header_bytes[2];
            cur[2] = ip6_header_bytes[3];
            cur += 3;
        }
    }

    // Next Header
    switch (ip6_header.GetNextHeader())
    {
    case kProtoHopOpts:
    case kProtoUdp:
        hc_ctl |= kHcNextHeader;
        break;

    default:
        cur[0] = ip6_header.GetNextHeader();
        cur++;
        break;
    }

    // Hop Limit
    switch (ip6_header.GetHopLimit())
    {
    case 1:
        hc_ctl |= kHcHopLimit1;
        break;

    case 64:
        hc_ctl |= kHcHopLimit64;
        break;

    case 255:
        hc_ctl |= kHcHopLimit255;
        break;

    default:
        cur[0] = ip6_header.GetHopLimit();
        cur++;
        break;
    }

    // Source Address
    if (ip6_header.GetSource()->IsUnspecified())
    {
        hc_ctl |= kHcSrcAddrContext;
    }
    else if (ip6_header.GetSource()->IsLinkLocal())
    {
        cur += CompressSourceIid(macsrc, *ip6_header.GetSource(), src_context, hc_ctl, cur);
    }
    else if (src_context_valid)
    {
        hc_ctl |= kHcSrcAddrContext;
        cur += CompressSourceIid(macsrc, *ip6_header.GetSource(), src_context, hc_ctl, cur);
    }
    else
    {
        memcpy(cur, ip6_header.GetSource()->addr8, sizeof(*ip6_header.GetSource()));
        cur += 16;
    }

    // Destination Address
    if (ip6_header.GetDestination()->IsMulticast())
    {
        cur += CompressMulticast(*ip6_header.GetDestination(), hc_ctl, cur);
    }
    else if (ip6_header.GetDestination()->IsLinkLocal())
    {
        cur += CompressDestinationIid(macdst, *ip6_header.GetDestination(), dst_context, hc_ctl, cur);
    }
    else if (dst_context_valid)
    {
        hc_ctl |= kHcDstAddrContext;
        cur += CompressDestinationIid(macdst, *ip6_header.GetDestination(), dst_context, hc_ctl, cur);
    }
    else
    {
        memcpy(cur, ip6_header.GetDestination(), sizeof(*ip6_header.GetDestination()));
        cur += 16;
    }

    buf[0] = hc_ctl >> 8;
    buf[1] = hc_ctl;
    message.SetOffset(sizeof(ip6_header));

    next_header = ip6_header.GetNextHeader();

    while (1)
    {
        switch (next_header)
        {
        case kProtoHopOpts:
            cur += CompressExtensionHeader(message, cur, next_header);
            break;

        case kProtoUdp:
            cur += CompressUdp(message, cur);
            ExitNow();

        default:
            ExitNow();
        }
    }

exit:
    return cur - buf;
}

int Lowpan::CompressExtensionHeader(Message &message, uint8_t *buf, uint8_t &next_header)
{
    Ip6ExtensionHeader ext_header;
    uint8_t *cur = buf;
    uint8_t len;

    message.Read(message.GetOffset(), sizeof(ext_header), &ext_header);
    message.MoveOffset(sizeof(ext_header));

    cur[0] = kExtHdrDispatch | kExtHdrEidHbh;
    next_header = ext_header.GetNextHeader();

    switch (ext_header.GetNextHeader())
    {
    case kProtoUdp:
        cur[0] |= kExtHdrNextHeader;
        break;

    default:
        cur++;
        cur[0] = ext_header.GetNextHeader();
        break;
    }

    cur++;

    len = (ext_header.GetLength() + 1) * 8 - sizeof(ext_header);
    cur[0] = len;
    cur++;

    message.Read(message.GetOffset(), len, cur);
    message.MoveOffset(len);
    cur += len;

    return cur - buf;
}

int Lowpan::CompressUdp(Message &message, uint8_t *buf)
{
    UdpHeader udp_header;
    uint8_t *cur = buf;

    message.Read(message.GetOffset(), sizeof(udp_header), &udp_header);

    cur[0] = kUdpDispatch;
    cur++;

    memcpy(cur, &udp_header, UdpHeader::GetLengthOffset());
    cur += UdpHeader::GetLengthOffset();
    memcpy(cur, reinterpret_cast<uint8_t *>(&udp_header) + UdpHeader::GetChecksumOffset(), 2);
    cur += 2;

    message.MoveOffset(sizeof(udp_header));

    return cur - buf;
}

ThreadError Lowpan::DispatchToNextHeader(uint8_t dispatch, IpProto &next_header)
{
    ThreadError error = kThreadError_None;

    if ((dispatch & kExtHdrDispatchMask) == kExtHdrDispatch)
    {
        switch (dispatch & kExtHdrEidMask)
        {
        case kExtHdrEidHbh:
            next_header = kProtoHopOpts;
            ExitNow();

        case kExtHdrEidRouting:
            next_header = kProtoRouting;
            ExitNow();

        case kExtHdrEidFragment:
            next_header = kProtoFragment;
            ExitNow();

        case kExtHdrEidDst:
            next_header = kProtoDstOpts;
            ExitNow();

        case kExtHdrEidIp6:
            next_header = kProtoIp6;
            ExitNow();
        }
    }
    else if ((dispatch & kUdpDispatchMask) == kUdpDispatch)
    {
        next_header = kProtoUdp;
        ExitNow();
    }

    error = kThreadError_Parse;

exit:
    return error;
}

int Lowpan::DecompressBaseHeader(Ip6Header &ip6_header, const Mac::Address &macsrc, const Mac::Address &macdst,
                                 const uint8_t *buf)
{
    ThreadError error = kThreadError_None;
    const uint8_t *cur = buf;
    uint16_t hc_ctl;
    Context src_context, dst_context;
    bool src_context_valid = true, dst_context_valid = true;
    IpProto next_header;
    uint8_t *bytes;

    hc_ctl = (static_cast<uint16_t>(cur[0]) << 8) | cur[1];
    cur += 2;

    // check Dispatch bits
    VerifyOrExit((hc_ctl & kHcDispatchMask) == kHcDispatch, error = kThreadError_Parse);

    // Context Identifier
    src_context.prefix_length = 0;
    dst_context.prefix_length = 0;

    if ((hc_ctl & kHcContextId) != 0)
    {
        if (m_network_data->GetContext(cur[0] >> 4, src_context) != kThreadError_None)
        {
            src_context_valid = false;
        }

        if (m_network_data->GetContext(cur[0] & 0xf, dst_context) != kThreadError_None)
        {
            dst_context_valid = false;
        }

        cur++;
    }
    else
    {
        m_network_data->GetContext(0, src_context);
        m_network_data->GetContext(0, dst_context);
    }

    memset(&ip6_header, 0, sizeof(ip6_header));
    ip6_header.Init();

    // Traffic Class and Flow Label
    if ((hc_ctl & kHcTrafficFlowMask) != kHcTrafficFlow)
    {
        bytes = reinterpret_cast<uint8_t *>(&ip6_header);
        bytes[1] |= (cur[0] & 0xc0) >> 2;

        if ((hc_ctl & kHcTrafficClass) == 0)
        {
            bytes[0] |= (cur[0] >> 2) & 0x0f;
            bytes[1] |= (cur[0] << 6) & 0xc0;
            cur++;
        }

        if ((hc_ctl & kHcFlowLabel) == 0)
        {
            bytes[1] |= cur[0] & 0x0f;
            bytes[2] |= cur[1];
            bytes[3] |= cur[2];
            cur += 3;
        }
    }

    // Next Header
    if ((hc_ctl & kHcNextHeader) == 0)
    {
        ip6_header.SetNextHeader(static_cast<IpProto>(cur[0]));
        cur++;
    }

    // Hop Limit
    switch (hc_ctl & kHcHopLimitMask)
    {
    case kHcHopLimit1:
        ip6_header.SetHopLimit(1);
        break;

    case kHcHopLimit64:
        ip6_header.SetHopLimit(64);
        break;

    case kHcHopLimit255:
        ip6_header.SetHopLimit(255);
        break;

    default:
        ip6_header.SetHopLimit(cur[0]);
        cur++;
        break;
    }

    // Source Address
    switch (hc_ctl & kHcSrcAddrModeMask)
    {
    case kHcSrcAddrMode0:
        if ((hc_ctl & kHcSrcAddrContext) == 0)
        {
            memcpy(ip6_header.GetSource(), cur, sizeof(*ip6_header.GetSource()));
            cur += 16;
        }

        break;

    case kHcSrcAddrMode1:
        memcpy(ip6_header.GetSource()->addr8 + 8, cur, 8);
        cur += 8;
        break;

    case kHcSrcAddrMode2:
        ip6_header.GetSource()->addr8[11] = 0xff;
        ip6_header.GetSource()->addr8[12] = 0xfe;
        memcpy(ip6_header.GetSource()->addr8 + 14, cur, 2);
        cur += 2;
        break;

    case kHcSrcAddrMode3:
        ComputeIID(macsrc, src_context, ip6_header.GetSource()->addr8 + 8);
        break;
    }

    if ((hc_ctl & kHcSrcAddrContext) == 0)
    {
        if ((hc_ctl & kHcSrcAddrModeMask) != 0)
        {
            ip6_header.GetSource()->addr16[0] = HostSwap16(0xfe80);
        }
    }
    else
    {
        VerifyOrExit(src_context_valid, error = kThreadError_Parse);
        CopyContext(src_context, *ip6_header.GetSource());
    }

    if ((hc_ctl & kHcMulticast) == 0)
    {
        // Unicast Destination Address

        switch (hc_ctl & kHcDstAddrModeMask)
        {
        case kHcDstAddrMode0:
            memcpy(ip6_header.GetDestination(), cur, sizeof(*ip6_header.GetDestination()));
            cur += 16;
            break;

        case kHcDstAddrMode1:
            memcpy(ip6_header.GetDestination()->addr8 + 8, cur, 8);
            cur += 8;
            break;

        case kHcDstAddrMode2:
            ip6_header.GetDestination()->addr8[11] = 0xff;
            ip6_header.GetDestination()->addr8[12] = 0xfe;
            memcpy(ip6_header.GetDestination()->addr8 + 14, cur, 2);
            cur += 2;
            break;

        case kHcDstAddrMode3:
            ComputeIID(macdst, dst_context, ip6_header.GetDestination()->addr8 + 8);
            break;
        }

        if ((hc_ctl & kHcDstAddrContext) == 0)
        {
            if ((hc_ctl & kHcDstAddrModeMask) != 0)
            {
                ip6_header.GetDestination()->addr16[0] = HostSwap16(0xfe80);
            }
        }
        else
        {
            VerifyOrExit(dst_context_valid, error = kThreadError_Parse);
            CopyContext(dst_context, *ip6_header.GetDestination());
        }
    }
    else
    {
        // Multicast Destination Address

        ip6_header.GetDestination()->addr8[0] = 0xff;

        if ((hc_ctl & kHcDstAddrContext) == 0)
        {
            switch (hc_ctl & kHcDstAddrModeMask)
            {
            case kHcDstAddrMode0:
                memcpy(ip6_header.GetDestination()->addr8, cur, 16);
                cur += 16;
                break;

            case kHcDstAddrMode1:
                ip6_header.GetDestination()->addr8[1] = cur[0];
                memcpy(ip6_header.GetDestination()->addr8 + 11, cur + 1, 5);
                cur += 6;
                break;

            case kHcDstAddrMode2:
                ip6_header.GetDestination()->addr8[1] = cur[0];
                memcpy(ip6_header.GetDestination()->addr8 + 13, cur + 1, 3);
                cur += 4;
                break;

            case kHcDstAddrMode3:
                ip6_header.GetDestination()->addr8[1] = 0x02;
                ip6_header.GetDestination()->addr8[15] = cur[0];
                cur++;
                break;
            }
        }
        else
        {
            switch (hc_ctl & kHcDstAddrModeMask)
            {
            case 0:
                VerifyOrExit(dst_context_valid, error = kThreadError_Parse);
                ip6_header.GetDestination()->addr8[1] = cur[0];
                ip6_header.GetDestination()->addr8[2] = cur[1];
                memcpy(ip6_header.GetDestination()->addr8 + 8, dst_context.prefix, 8);
                memcpy(ip6_header.GetDestination()->addr8 + 12, cur + 2, 4);
                cur += 6;
                break;

            default:
                ExitNow(error = kThreadError_Parse);
            }
        }
    }

    if ((hc_ctl & kHcNextHeader) != 0)
    {
        SuccessOrExit(error = DispatchToNextHeader(cur[0], next_header));
        ip6_header.SetNextHeader(next_header);
    }

exit:
    return (error == kThreadError_None) ? cur - buf : -1;
}

int Lowpan::DecompressExtensionHeader(Message &message, const uint8_t *buf, uint16_t buf_length)
{
    const uint8_t *cur = buf;
    uint8_t hdr[2];
    uint8_t len;
    IpProto next_header;
    int rval = -1;
    uint8_t ctl = cur[0];

    cur++;

    // next header
    if (ctl & kExtHdrNextHeader)
    {
        len = cur[0];
        cur++;

        SuccessOrExit(DispatchToNextHeader(cur[len], next_header));
        hdr[0] = static_cast<uint8_t>(next_header);
    }
    else
    {
        hdr[0] = cur[0];
        cur++;

        len = cur[0];
        cur++;
    }

    // length
    hdr[1] = ((sizeof(hdr) + len + 7) / 8) - 1;

    SuccessOrExit(message.Append(hdr, sizeof(hdr)));
    message.MoveOffset(sizeof(hdr));

    // payload
    SuccessOrExit(message.Append(cur, len));
    message.MoveOffset(len);
    cur += len;

    rval = cur - buf;

exit:
    return rval;
}

int Lowpan::DecompressUdpHeader(Message &message, const uint8_t *buf, uint16_t buf_length, uint16_t datagram_length)
{
    ThreadError error = kThreadError_None;
    const uint8_t *cur = buf;
    UdpHeader udp_header;
    uint8_t udp_ctl = cur[0];

    cur++;

    memset(&udp_header, 0, sizeof(udp_header));

    // source and dest ports
    switch (udp_ctl & kUdpPortMask)
    {
    case 0:
        udp_header.SetSourcePort((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
        udp_header.SetDestinationPort((static_cast<uint16_t>(cur[2]) << 8) | cur[3]);
        cur += 4;
        break;

    case 1:
        udp_header.SetSourcePort((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
        udp_header.SetDestinationPort(0xf000 | cur[2]);
        cur += 3;
        break;

    case 2:
        udp_header.SetSourcePort(0xf000 | cur[0]);
        udp_header.SetDestinationPort((static_cast<uint16_t>(cur[2]) << 8) | cur[1]);
        cur += 3;
        break;

    case 3:
        udp_header.SetSourcePort(0xf000 | cur[0]);
        udp_header.SetDestinationPort(0xf000 | cur[1]);
        cur += 2;
        break;
    }

    // checksum
    if ((udp_ctl & kUdpChecksum) != 0)
    {
        ExitNow(error = kThreadError_Parse);
    }
    else
    {
        udp_header.SetChecksum((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
        cur += 2;
    }

    // length
    if (datagram_length == 0)
    {
        udp_header.SetLength(sizeof(udp_header) + (buf_length - (cur - buf)));
    }
    else
    {
        udp_header.SetLength(datagram_length - message.GetOffset());
    }

    message.Append(&udp_header, sizeof(udp_header));
    message.MoveOffset(sizeof(udp_header));

exit:

    if (error != kThreadError_None)
    {
        return -1;
    }

    return cur - buf;
}

int Lowpan::Decompress(Message &message, const Mac::Address &macsrc, const Mac::Address &macdst,
                       const uint8_t *buf, uint16_t buf_len, uint16_t datagram_length)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6_header;
    const uint8_t *cur = buf;
    bool compressed;
    int rval;

    compressed = (((static_cast<uint16_t>(cur[0]) << 8) | cur[1]) & kHcNextHeader) != 0;

    VerifyOrExit((rval = DecompressBaseHeader(ip6_header, macsrc, macdst, buf)) >= 0, ;);
    cur += rval;

    SuccessOrExit(error = message.Append(&ip6_header, sizeof(ip6_header)));
    SuccessOrExit(error = message.SetOffset(sizeof(ip6_header)));

    while (compressed)
    {
        if ((cur[0] & kExtHdrDispatchMask) == kExtHdrDispatch)
        {
            compressed = (cur[0] & kExtHdrNextHeader) != 0;
            VerifyOrExit((rval = DecompressExtensionHeader(message, cur, buf_len - (cur - buf))) >= 0,
                         error = kThreadError_Parse);
        }
        else if ((cur[0] & kUdpDispatchMask) == kUdpDispatch)
        {
            compressed = false;
            VerifyOrExit((rval = DecompressUdpHeader(message, cur, buf_len - (cur - buf), datagram_length)) >= 0,
                         error = kThreadError_Parse);
        }
        else
        {
            ExitNow(error = kThreadError_Parse);
        }

        cur += rval;
    }

exit:

    if (error != kThreadError_None)
    {
        return -1;
    }

    return cur - buf;
}

}  // namespace Thread
