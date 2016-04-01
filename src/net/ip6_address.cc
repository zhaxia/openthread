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

#include <stdio.h>
#include <string.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/thread_error.h>
#include <net/ip6_address.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {

bool Ip6Address::IsUnspecified() const
{
    return (s6_addr32[0] == 0 && s6_addr32[1] == 0 && s6_addr32[2] == 0 && s6_addr32[3] == 0);
}

bool Ip6Address::IsLoopback() const
{
    return (s6_addr32[0] == 0 && s6_addr32[1] == 0 && s6_addr32[2] == 0 && s6_addr32[3] == HostSwap32(1));
}

bool Ip6Address::IsLinkLocal() const
{
    return (s6_addr[0] == 0xfe) && ((s6_addr[1] & 0xc0) == 0x80);
}

bool Ip6Address::IsMulticast() const
{
    return s6_addr[0] == 0xff;
}

bool Ip6Address::IsLinkLocalMulticast() const
{
    return IsMulticast() && (GetScope() == kLinkLocalScope);
}

bool Ip6Address::IsLinkLocalAllNodesMulticast() const
{
    return (s6_addr32[0] == HostSwap32(0xff020000) && s6_addr32[1] == 0 &&
            s6_addr32[2] == 0 && s6_addr32[3] == HostSwap32(0x01));
}

bool Ip6Address::IsLinkLocalAllRoutersMulticast() const
{
    return (s6_addr32[0] == HostSwap32(0xff020000) && s6_addr32[1] == 0 &&
            s6_addr32[2] == 0 && s6_addr32[3] == HostSwap32(0x02));
}

bool Ip6Address::IsRealmLocalMulticast() const
{
    return IsMulticast() && (GetScope() == kRealmLocalScope);
}

bool Ip6Address::IsRealmLocalAllNodesMulticast() const
{
    return (s6_addr32[0] == HostSwap32(0xff030000) && s6_addr32[1] == 0 &&
            s6_addr32[2] == 0 && s6_addr32[3] == HostSwap32(0x01));
}

bool Ip6Address::IsRealmLocalAllRoutersMulticast() const
{
    return (s6_addr32[0] == HostSwap32(0xff030000) && s6_addr32[1] == 0 &&
            s6_addr32[2] == 0 && s6_addr32[3] == HostSwap32(0x02));
}

uint8_t Ip6Address::GetScope() const
{
    if (IsMulticast())
    {
        return s6_addr[1] & 0xf;
    }
    else if (IsLinkLocal())
    {
        return kLinkLocalScope;
    }
    else if (IsLoopback())
    {
        return kNodeLocalScope;
    }

    return kGlobalScope;
}

uint8_t Ip6Address::PrefixMatch(const Ip6Address &other) const
{
    uint8_t rval = 0;
    uint8_t diff;

    for (uint8_t i = 0; i < sizeof(Ip6Address); i++)
    {
        diff = s6_addr[i] ^ other.s6_addr[i];

        if (diff == 0)
        {
            rval += 8;
        }
        else
        {
            while ((diff & 0x80) == 0)
            {
                rval++;
                diff <<= 1;
            }

            break;
        }
    }

    return rval;
}

bool Ip6Address::operator==(const Ip6Address &other) const
{
    return memcmp(s6_addr, other.s6_addr, sizeof(s6_addr)) == 0;
}

bool Ip6Address::operator!=(const Ip6Address &other) const
{
    return memcmp(s6_addr, other.s6_addr, sizeof(s6_addr)) != 0;
}

ThreadError Ip6Address::FromString(const char *buf)
{
    ThreadError error = kThreadError_None;
    uint8_t *dst = reinterpret_cast<uint8_t *>(s6_addr);
    uint8_t *endp = reinterpret_cast<uint8_t *>(s6_addr + 15);
    uint8_t *colonp = NULL;
    uint16_t val = 0;
    uint8_t count = 0;
    bool first = true;
    uint8_t ch;
    uint8_t d;

    memset(s6_addr, 0, 16);

    dst--;

    for (;;)
    {
        ch = *buf++;
        d = ch & 0xf;

        if (('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F'))
        {
            d += 9;
        }
        else if (ch == ':' || ch == '\0' || ch == ' ')
        {
            if (count)
            {
                VerifyOrExit(dst + 2 <= endp, error = kThreadError_Parse);
                *(dst + 1) = static_cast<uint8_t>(val >> 8);
                *(dst + 2) = static_cast<uint8_t>(val);
                dst += 2;
                count = 0;
                val = 0;
            }
            else if (ch == ':')
            {
                VerifyOrExit(colonp == NULL || first, error = kThreadError_Parse);
                colonp = dst;
            }

            if (ch == '\0' || ch == ' ')
            {
                break;
            }

            continue;
        }
        else
        {
            VerifyOrExit('0' <= ch && ch <= '9', error = kThreadError_Parse);
        }

        first = false;
        val = (val << 4) | d;
        VerifyOrExit(++count <= 4, error = kThreadError_Parse);
    }

    while (colonp && dst > colonp)
    {
        *endp-- = *dst--;
    }

    while (endp > dst)
    {
        *endp-- = 0;
    }

exit:
    return error;
}

const char *Ip6Address::ToString(char *buf, uint16_t size) const
{
    snprintf(buf, size, "%x:%x:%x:%x:%x:%x:%x:%x",
             HostSwap16(s6_addr16[0]), HostSwap16(s6_addr16[1]),
             HostSwap16(s6_addr16[2]), HostSwap16(s6_addr16[3]),
             HostSwap16(s6_addr16[4]), HostSwap16(s6_addr16[5]),
             HostSwap16(s6_addr16[6]), HostSwap16(s6_addr16[7]));
    return buf;
}

}  // namespace Thread

