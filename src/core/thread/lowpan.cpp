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
    mNetworkData = netif.GetNetworkDataLeader();
}

ThreadError CopyContext(const Context &context, Ip6Address &address)
{
    memcpy(&address, context.mPrefix, context.mPrefixLength / 8);

    for (int i = (context.mPrefixLength & ~7); i < context.mPrefixLength; i++)
    {
        address.m8[i / 8] &= ~(0x80 >> (i % 8));
        address.m8[i / 8] |= context.mPrefix[i / 8] & (0x80 >> (i % 8));
    }

    return kThreadError_None;
}

ThreadError ComputeIID(const Mac::Address &macaddr, const Context &context, uint8_t *iid)
{
    switch (macaddr.mLength)
    {
    case 2:
        iid[0] = 0x00;
        iid[1] = 0x00;
        iid[2] = 0x00;
        iid[3] = 0xff;
        iid[4] = 0xfe;
        iid[5] = 0x00;
        iid[6] = macaddr.mAddress16 >> 8;
        iid[7] = macaddr.mAddress16;
        break;

    case 8:
        memcpy(iid, &macaddr.mAddress64, sizeof(macaddr.mAddress64));
        iid[0] ^= 0x02;
        break;

    default:
        assert(false);
    }

    if (context.mPrefixLength > 64)
    {
        for (int i = (context.mPrefixLength & ~7); i < context.mPrefixLength; i++)
        {
            iid[i / 8] &= ~(0x80 >> (i % 8));
            iid[i / 8] |= context.mPrefix[i / 8] & (0x80 >> (i % 8));
        }
    }

    return kThreadError_None;
}

int Lowpan::CompressSourceIid(const Mac::Address &macaddr, const Ip6Address &ipaddr, const Context &context,
                              uint16_t &hcCtl, uint8_t *buf)
{
    uint8_t *cur = buf;
    uint8_t iid[8];
    Mac::Address tmp;

    ComputeIID(macaddr, context, iid);

    if (memcmp(iid, ipaddr.m8 + 8, 8) == 0)
    {
        hcCtl |= kHcSrcAddrMode3;
    }
    else
    {
        tmp.mLength = 2;
        tmp.mAddress16 = HostSwap16(ipaddr.m16[7]);
        ComputeIID(tmp, context, iid);

        if (memcmp(iid, ipaddr.m8 + 8, 8) == 0)
        {
            hcCtl |= kHcSrcAddrMode2;
            cur[0] = ipaddr.m8[14];
            cur[1] = ipaddr.m8[15];
            cur += 2;
        }
        else
        {
            hcCtl |= kHcSrcAddrMode1;
            memcpy(cur, ipaddr.m8 + 8, 8);
            cur += 8;
        }
    }

    return cur - buf;
}

int Lowpan::CompressDestinationIid(const Mac::Address &macaddr, const Ip6Address &ipaddr, const Context &context,
                                   uint16_t &hcCtl, uint8_t *buf)
{
    uint8_t *cur = buf;
    uint8_t iid[8];
    Mac::Address tmp;

    ComputeIID(macaddr, context, iid);

    if (memcmp(iid, ipaddr.m8 + 8, 8) == 0)
    {
        hcCtl |= kHcDstAddrMode3;
    }
    else
    {
        tmp.mLength = 2;
        tmp.mAddress16 = HostSwap16(ipaddr.m16[7]);
        ComputeIID(tmp, context, iid);

        if (memcmp(iid, ipaddr.m8 + 8, 8) == 0)
        {
            hcCtl |= kHcDstAddrMode2;
            cur[0] = ipaddr.m8[14];
            cur[1] = ipaddr.m8[15];
            cur += 2;
        }
        else
        {
            hcCtl |= kHcDstAddrMode1;
            memcpy(cur, ipaddr.m8 + 8, 8);
            cur += 8;
        }
    }

    return cur - buf;
}

int Lowpan::CompressMulticast(const Ip6Address &ipaddr, uint16_t &hcCtl, uint8_t *buf)
{
    uint8_t *cur = buf;

    hcCtl |= kHcMulticast;

    for (int i = 2; i < 16; i++)
    {
        if (ipaddr.m8[i])
        {
            if (ipaddr.m8[1] == 0x02 && i >= 15)
            {
                hcCtl |= kHcDstAddrMode3;
                cur[0] = ipaddr.m8[15];
                cur++;
            }
            else if (i >= 13)
            {
                hcCtl |= kHcDstAddrMode2;
                cur[0] = ipaddr.m8[1];
                memcpy(cur + 1, ipaddr.m8 + 13, 3);
                cur += 4;
            }
            else if (i >= 9)
            {
                hcCtl |= kHcDstAddrMode1;
                cur[0] = ipaddr.m8[1];
                memcpy(cur + 1, ipaddr.m8 + 11, 5);
                cur += 6;
            }
            else
            {
                memcpy(cur, ipaddr.m8, 16);
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
    uint16_t hcCtl = 0;
    Ip6Header ip6Header;
    uint8_t *ip6HeaderBytes = reinterpret_cast<uint8_t *>(&ip6Header);
    Context srcContext, dstContext;
    bool srcContextValid = true, dstContextValid = true;
    uint8_t nextHeader;

    message.Read(0, sizeof(ip6Header), &ip6Header);

    if (mNetworkData->GetContext(*ip6Header.GetSource(), srcContext) != kThreadError_None)
    {
        mNetworkData->GetContext(0, srcContext);
        srcContextValid = false;
    }

    if (mNetworkData->GetContext(*ip6Header.GetDestination(), dstContext) != kThreadError_None)
    {
        mNetworkData->GetContext(0, dstContext);
        dstContextValid = false;
    }

    hcCtl = kHcDispatch;

    // Lowpan HC Control Bits
    cur += 2;

    // Context Identifier
    if (srcContext.mContextId != 0 || dstContext.mContextId != 0)
    {
        hcCtl |= kHcContextId;
        cur[0] = (srcContext.mContextId << 4) | dstContext.mContextId;
        cur++;
    }

    // Traffic Class
    if (((ip6HeaderBytes[0] & 0x0f) == 0) && ((ip6HeaderBytes[1] & 0xf0) == 0))
    {
        hcCtl |= kHcTrafficClass;
    }

    // Flow Label
    if (((ip6HeaderBytes[1] & 0x0f) == 0) && ((ip6HeaderBytes[2]) == 0) && ((ip6HeaderBytes[3]) == 0))
    {
        hcCtl |= kHcFlowLabel;
    }

    if ((hcCtl & kHcTrafficFlowMask) != kHcTrafficFlow)
    {
        cur[0] = (ip6HeaderBytes[1] >> 4) << 6;

        if ((hcCtl & kHcTrafficClass) == 0)
        {
            cur[0] |= ((ip6HeaderBytes[0] & 0x0f) << 2) | (ip6HeaderBytes[1] >> 6);
            cur++;
        }

        if ((hcCtl & kHcFlowLabel) == 0)
        {
            cur[0] |= ip6HeaderBytes[1] & 0x0f;
            cur[1] = ip6HeaderBytes[2];
            cur[2] = ip6HeaderBytes[3];
            cur += 3;
        }
    }

    // Next Header
    switch (ip6Header.GetNextHeader())
    {
    case kProtoHopOpts:
    case kProtoUdp:
        hcCtl |= kHcNextHeader;
        break;

    default:
        cur[0] = ip6Header.GetNextHeader();
        cur++;
        break;
    }

    // Hop Limit
    switch (ip6Header.GetHopLimit())
    {
    case 1:
        hcCtl |= kHcHopLimit1;
        break;

    case 64:
        hcCtl |= kHcHopLimit64;
        break;

    case 255:
        hcCtl |= kHcHopLimit255;
        break;

    default:
        cur[0] = ip6Header.GetHopLimit();
        cur++;
        break;
    }

    // Source Address
    if (ip6Header.GetSource()->IsUnspecified())
    {
        hcCtl |= kHcSrcAddrContext;
    }
    else if (ip6Header.GetSource()->IsLinkLocal())
    {
        cur += CompressSourceIid(macsrc, *ip6Header.GetSource(), srcContext, hcCtl, cur);
    }
    else if (srcContextValid)
    {
        hcCtl |= kHcSrcAddrContext;
        cur += CompressSourceIid(macsrc, *ip6Header.GetSource(), srcContext, hcCtl, cur);
    }
    else
    {
        memcpy(cur, ip6Header.GetSource()->m8, sizeof(*ip6Header.GetSource()));
        cur += 16;
    }

    // Destination Address
    if (ip6Header.GetDestination()->IsMulticast())
    {
        cur += CompressMulticast(*ip6Header.GetDestination(), hcCtl, cur);
    }
    else if (ip6Header.GetDestination()->IsLinkLocal())
    {
        cur += CompressDestinationIid(macdst, *ip6Header.GetDestination(), dstContext, hcCtl, cur);
    }
    else if (dstContextValid)
    {
        hcCtl |= kHcDstAddrContext;
        cur += CompressDestinationIid(macdst, *ip6Header.GetDestination(), dstContext, hcCtl, cur);
    }
    else
    {
        memcpy(cur, ip6Header.GetDestination(), sizeof(*ip6Header.GetDestination()));
        cur += 16;
    }

    buf[0] = hcCtl >> 8;
    buf[1] = hcCtl;
    message.SetOffset(sizeof(ip6Header));

    nextHeader = ip6Header.GetNextHeader();

    while (1)
    {
        switch (nextHeader)
        {
        case kProtoHopOpts:
            cur += CompressExtensionHeader(message, cur, nextHeader);
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

int Lowpan::CompressExtensionHeader(Message &message, uint8_t *buf, uint8_t &nextHeader)
{
    Ip6ExtensionHeader extHeader;
    uint8_t *cur = buf;
    uint8_t len;

    message.Read(message.GetOffset(), sizeof(extHeader), &extHeader);
    message.MoveOffset(sizeof(extHeader));

    cur[0] = kExtHdrDispatch | kExtHdrEidHbh;
    nextHeader = extHeader.GetNextHeader();

    switch (extHeader.GetNextHeader())
    {
    case kProtoUdp:
        cur[0] |= kExtHdrNextHeader;
        break;

    default:
        cur++;
        cur[0] = extHeader.GetNextHeader();
        break;
    }

    cur++;

    len = (extHeader.GetLength() + 1) * 8 - sizeof(extHeader);
    cur[0] = len;
    cur++;

    message.Read(message.GetOffset(), len, cur);
    message.MoveOffset(len);
    cur += len;

    return cur - buf;
}

int Lowpan::CompressUdp(Message &message, uint8_t *buf)
{
    UdpHeader udpHeader;
    uint8_t *cur = buf;

    message.Read(message.GetOffset(), sizeof(udpHeader), &udpHeader);

    cur[0] = kUdpDispatch;
    cur++;

    memcpy(cur, &udpHeader, UdpHeader::GetLengthOffset());
    cur += UdpHeader::GetLengthOffset();
    memcpy(cur, reinterpret_cast<uint8_t *>(&udpHeader) + UdpHeader::GetChecksumOffset(), 2);
    cur += 2;

    message.MoveOffset(sizeof(udpHeader));

    return cur - buf;
}

ThreadError Lowpan::DispatchToNextHeader(uint8_t dispatch, IpProto &nextHeader)
{
    ThreadError error = kThreadError_None;

    if ((dispatch & kExtHdrDispatchMask) == kExtHdrDispatch)
    {
        switch (dispatch & kExtHdrEidMask)
        {
        case kExtHdrEidHbh:
            nextHeader = kProtoHopOpts;
            ExitNow();

        case kExtHdrEidRouting:
            nextHeader = kProtoRouting;
            ExitNow();

        case kExtHdrEidFragment:
            nextHeader = kProtoFragment;
            ExitNow();

        case kExtHdrEidDst:
            nextHeader = kProtoDstOpts;
            ExitNow();

        case kExtHdrEidIp6:
            nextHeader = kProtoIp6;
            ExitNow();
        }
    }
    else if ((dispatch & kUdpDispatchMask) == kUdpDispatch)
    {
        nextHeader = kProtoUdp;
        ExitNow();
    }

    error = kThreadError_Parse;

exit:
    return error;
}

int Lowpan::DecompressBaseHeader(Ip6Header &ip6Header, const Mac::Address &macsrc, const Mac::Address &macdst,
                                 const uint8_t *buf)
{
    ThreadError error = kThreadError_None;
    const uint8_t *cur = buf;
    uint16_t hcCtl;
    Context srcContext, dstContext;
    bool srcContextValid = true, dstContextValid = true;
    IpProto nextHeader;
    uint8_t *bytes;

    hcCtl = (static_cast<uint16_t>(cur[0]) << 8) | cur[1];
    cur += 2;

    // check Dispatch bits
    VerifyOrExit((hcCtl & kHcDispatchMask) == kHcDispatch, error = kThreadError_Parse);

    // Context Identifier
    srcContext.mPrefixLength = 0;
    dstContext.mPrefixLength = 0;

    if ((hcCtl & kHcContextId) != 0)
    {
        if (mNetworkData->GetContext(cur[0] >> 4, srcContext) != kThreadError_None)
        {
            srcContextValid = false;
        }

        if (mNetworkData->GetContext(cur[0] & 0xf, dstContext) != kThreadError_None)
        {
            dstContextValid = false;
        }

        cur++;
    }
    else
    {
        mNetworkData->GetContext(0, srcContext);
        mNetworkData->GetContext(0, dstContext);
    }

    memset(&ip6Header, 0, sizeof(ip6Header));
    ip6Header.Init();

    // Traffic Class and Flow Label
    if ((hcCtl & kHcTrafficFlowMask) != kHcTrafficFlow)
    {
        bytes = reinterpret_cast<uint8_t *>(&ip6Header);
        bytes[1] |= (cur[0] & 0xc0) >> 2;

        if ((hcCtl & kHcTrafficClass) == 0)
        {
            bytes[0] |= (cur[0] >> 2) & 0x0f;
            bytes[1] |= (cur[0] << 6) & 0xc0;
            cur++;
        }

        if ((hcCtl & kHcFlowLabel) == 0)
        {
            bytes[1] |= cur[0] & 0x0f;
            bytes[2] |= cur[1];
            bytes[3] |= cur[2];
            cur += 3;
        }
    }

    // Next Header
    if ((hcCtl & kHcNextHeader) == 0)
    {
        ip6Header.SetNextHeader(static_cast<IpProto>(cur[0]));
        cur++;
    }

    // Hop Limit
    switch (hcCtl & kHcHopLimitMask)
    {
    case kHcHopLimit1:
        ip6Header.SetHopLimit(1);
        break;

    case kHcHopLimit64:
        ip6Header.SetHopLimit(64);
        break;

    case kHcHopLimit255:
        ip6Header.SetHopLimit(255);
        break;

    default:
        ip6Header.SetHopLimit(cur[0]);
        cur++;
        break;
    }

    // Source Address
    switch (hcCtl & kHcSrcAddrModeMask)
    {
    case kHcSrcAddrMode0:
        if ((hcCtl & kHcSrcAddrContext) == 0)
        {
            memcpy(ip6Header.GetSource(), cur, sizeof(*ip6Header.GetSource()));
            cur += 16;
        }

        break;

    case kHcSrcAddrMode1:
        memcpy(ip6Header.GetSource()->m8 + 8, cur, 8);
        cur += 8;
        break;

    case kHcSrcAddrMode2:
        ip6Header.GetSource()->m8[11] = 0xff;
        ip6Header.GetSource()->m8[12] = 0xfe;
        memcpy(ip6Header.GetSource()->m8 + 14, cur, 2);
        cur += 2;
        break;

    case kHcSrcAddrMode3:
        ComputeIID(macsrc, srcContext, ip6Header.GetSource()->m8 + 8);
        break;
    }

    if ((hcCtl & kHcSrcAddrContext) == 0)
    {
        if ((hcCtl & kHcSrcAddrModeMask) != 0)
        {
            ip6Header.GetSource()->m16[0] = HostSwap16(0xfe80);
        }
    }
    else
    {
        VerifyOrExit(srcContextValid, error = kThreadError_Parse);
        CopyContext(srcContext, *ip6Header.GetSource());
    }

    if ((hcCtl & kHcMulticast) == 0)
    {
        // Unicast Destination Address

        switch (hcCtl & kHcDstAddrModeMask)
        {
        case kHcDstAddrMode0:
            memcpy(ip6Header.GetDestination(), cur, sizeof(*ip6Header.GetDestination()));
            cur += 16;
            break;

        case kHcDstAddrMode1:
            memcpy(ip6Header.GetDestination()->m8 + 8, cur, 8);
            cur += 8;
            break;

        case kHcDstAddrMode2:
            ip6Header.GetDestination()->m8[11] = 0xff;
            ip6Header.GetDestination()->m8[12] = 0xfe;
            memcpy(ip6Header.GetDestination()->m8 + 14, cur, 2);
            cur += 2;
            break;

        case kHcDstAddrMode3:
            ComputeIID(macdst, dstContext, ip6Header.GetDestination()->m8 + 8);
            break;
        }

        if ((hcCtl & kHcDstAddrContext) == 0)
        {
            if ((hcCtl & kHcDstAddrModeMask) != 0)
            {
                ip6Header.GetDestination()->m16[0] = HostSwap16(0xfe80);
            }
        }
        else
        {
            VerifyOrExit(dstContextValid, error = kThreadError_Parse);
            CopyContext(dstContext, *ip6Header.GetDestination());
        }
    }
    else
    {
        // Multicast Destination Address

        ip6Header.GetDestination()->m8[0] = 0xff;

        if ((hcCtl & kHcDstAddrContext) == 0)
        {
            switch (hcCtl & kHcDstAddrModeMask)
            {
            case kHcDstAddrMode0:
                memcpy(ip6Header.GetDestination()->m8, cur, 16);
                cur += 16;
                break;

            case kHcDstAddrMode1:
                ip6Header.GetDestination()->m8[1] = cur[0];
                memcpy(ip6Header.GetDestination()->m8 + 11, cur + 1, 5);
                cur += 6;
                break;

            case kHcDstAddrMode2:
                ip6Header.GetDestination()->m8[1] = cur[0];
                memcpy(ip6Header.GetDestination()->m8 + 13, cur + 1, 3);
                cur += 4;
                break;

            case kHcDstAddrMode3:
                ip6Header.GetDestination()->m8[1] = 0x02;
                ip6Header.GetDestination()->m8[15] = cur[0];
                cur++;
                break;
            }
        }
        else
        {
            switch (hcCtl & kHcDstAddrModeMask)
            {
            case 0:
                VerifyOrExit(dstContextValid, error = kThreadError_Parse);
                ip6Header.GetDestination()->m8[1] = cur[0];
                ip6Header.GetDestination()->m8[2] = cur[1];
                memcpy(ip6Header.GetDestination()->m8 + 8, dstContext.mPrefix, 8);
                memcpy(ip6Header.GetDestination()->m8 + 12, cur + 2, 4);
                cur += 6;
                break;

            default:
                ExitNow(error = kThreadError_Parse);
            }
        }
    }

    if ((hcCtl & kHcNextHeader) != 0)
    {
        SuccessOrExit(error = DispatchToNextHeader(cur[0], nextHeader));
        ip6Header.SetNextHeader(nextHeader);
    }

exit:
    return (error == kThreadError_None) ? cur - buf : -1;
}

int Lowpan::DecompressExtensionHeader(Message &message, const uint8_t *buf, uint16_t bufLength)
{
    const uint8_t *cur = buf;
    uint8_t hdr[2];
    uint8_t len;
    IpProto nextHeader;
    int rval = -1;
    uint8_t ctl = cur[0];

    cur++;

    // next header
    if (ctl & kExtHdrNextHeader)
    {
        len = cur[0];
        cur++;

        SuccessOrExit(DispatchToNextHeader(cur[len], nextHeader));
        hdr[0] = static_cast<uint8_t>(nextHeader);
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

int Lowpan::DecompressUdpHeader(Message &message, const uint8_t *buf, uint16_t bufLength, uint16_t datagramLength)
{
    ThreadError error = kThreadError_None;
    const uint8_t *cur = buf;
    UdpHeader udpHeader;
    uint8_t udpCtl = cur[0];

    cur++;

    memset(&udpHeader, 0, sizeof(udpHeader));

    // source and dest ports
    switch (udpCtl & kUdpPortMask)
    {
    case 0:
        udpHeader.SetSourcePort((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
        udpHeader.SetDestinationPort((static_cast<uint16_t>(cur[2]) << 8) | cur[3]);
        cur += 4;
        break;

    case 1:
        udpHeader.SetSourcePort((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
        udpHeader.SetDestinationPort(0xf000 | cur[2]);
        cur += 3;
        break;

    case 2:
        udpHeader.SetSourcePort(0xf000 | cur[0]);
        udpHeader.SetDestinationPort((static_cast<uint16_t>(cur[2]) << 8) | cur[1]);
        cur += 3;
        break;

    case 3:
        udpHeader.SetSourcePort(0xf000 | cur[0]);
        udpHeader.SetDestinationPort(0xf000 | cur[1]);
        cur += 2;
        break;
    }

    // checksum
    if ((udpCtl & kUdpChecksum) != 0)
    {
        ExitNow(error = kThreadError_Parse);
    }
    else
    {
        udpHeader.SetChecksum((static_cast<uint16_t>(cur[0]) << 8) | cur[1]);
        cur += 2;
    }

    // length
    if (datagramLength == 0)
    {
        udpHeader.SetLength(sizeof(udpHeader) + (bufLength - (cur - buf)));
    }
    else
    {
        udpHeader.SetLength(datagramLength - message.GetOffset());
    }

    message.Append(&udpHeader, sizeof(udpHeader));
    message.MoveOffset(sizeof(udpHeader));

exit:

    if (error != kThreadError_None)
    {
        return -1;
    }

    return cur - buf;
}

int Lowpan::Decompress(Message &message, const Mac::Address &macsrc, const Mac::Address &macdst,
                       const uint8_t *buf, uint16_t bufLen, uint16_t datagramLength)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6Header;
    const uint8_t *cur = buf;
    bool compressed;
    int rval;

    compressed = (((static_cast<uint16_t>(cur[0]) << 8) | cur[1]) & kHcNextHeader) != 0;

    VerifyOrExit((rval = DecompressBaseHeader(ip6Header, macsrc, macdst, buf)) >= 0, ;);
    cur += rval;

    SuccessOrExit(error = message.Append(&ip6Header, sizeof(ip6Header)));
    SuccessOrExit(error = message.SetOffset(sizeof(ip6Header)));

    while (compressed)
    {
        if ((cur[0] & kExtHdrDispatchMask) == kExtHdrDispatch)
        {
            compressed = (cur[0] & kExtHdrNextHeader) != 0;
            VerifyOrExit((rval = DecompressExtensionHeader(message, cur, bufLen - (cur - buf))) >= 0,
                         error = kThreadError_Parse);
        }
        else if ((cur[0] & kUdpDispatchMask) == kUdpDispatch)
        {
            compressed = false;
            VerifyOrExit((rval = DecompressUdpHeader(message, cur, bufLen - (cur - buf), datagramLength)) >= 0,
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
