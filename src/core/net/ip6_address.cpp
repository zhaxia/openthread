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

/**
 * @file
 *   This file implements IPv6 addresses.
 */

#include <stdio.h>
#include <string.h>

#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/thread_error.hpp>
#include <net/ip6_address.hpp>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {

bool Ip6Address::IsUnspecified() const
{
    return (m32[0] == 0 && m32[1] == 0 && m32[2] == 0 && m32[3] == 0);
}

bool Ip6Address::IsLoopback() const
{
    return (m32[0] == 0 && m32[1] == 0 && m32[2] == 0 && m32[3] == HostSwap32(1));
}

bool Ip6Address::IsLinkLocal() const
{
    return (m8[0] == 0xfe) && ((m8[1] & 0xc0) == 0x80);
}

bool Ip6Address::IsMulticast() const
{
    return m8[0] == 0xff;
}

bool Ip6Address::IsLinkLocalMulticast() const
{
    return IsMulticast() && (GetScope() == kLinkLocalScope);
}

bool Ip6Address::IsLinkLocalAllNodesMulticast() const
{
    return (m32[0] == HostSwap32(0xff020000) && m32[1] == 0 &&
            m32[2] == 0 && m32[3] == HostSwap32(0x01));
}

bool Ip6Address::IsLinkLocalAllRoutersMulticast() const
{
    return (m32[0] == HostSwap32(0xff020000) && m32[1] == 0 &&
            m32[2] == 0 && m32[3] == HostSwap32(0x02));
}

bool Ip6Address::IsRealmLocalMulticast() const
{
    return IsMulticast() && (GetScope() == kRealmLocalScope);
}

bool Ip6Address::IsRealmLocalAllNodesMulticast() const
{
    return (m32[0] == HostSwap32(0xff030000) && m32[1] == 0 &&
            m32[2] == 0 && m32[3] == HostSwap32(0x01));
}

bool Ip6Address::IsRealmLocalAllRoutersMulticast() const
{
    return (m32[0] == HostSwap32(0xff030000) && m32[1] == 0 &&
            m32[2] == 0 && m32[3] == HostSwap32(0x02));
}

uint8_t Ip6Address::GetScope() const
{
    if (IsMulticast())
    {
        return m8[1] & 0xf;
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
        diff = m8[i] ^ other.m8[i];

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
    return memcmp(m8, other.m8, sizeof(m8)) == 0;
}

bool Ip6Address::operator!=(const Ip6Address &other) const
{
    return memcmp(m8, other.m8, sizeof(m8)) != 0;
}

ThreadError Ip6Address::FromString(const char *buf)
{
    ThreadError error = kThreadError_None;
    uint8_t *dst = reinterpret_cast<uint8_t *>(m8);
    uint8_t *endp = reinterpret_cast<uint8_t *>(m8 + 15);
    uint8_t *colonp = NULL;
    uint16_t val = 0;
    uint8_t count = 0;
    bool first = true;
    uint8_t ch;
    uint8_t d;

    memset(m8, 0, 16);

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
             HostSwap16(m16[0]), HostSwap16(m16[1]),
             HostSwap16(m16[2]), HostSwap16(m16[3]),
             HostSwap16(m16[4]), HostSwap16(m16[5]),
             HostSwap16(m16[6]), HostSwap16(m16[7]));
    return buf;
}

}  // namespace Thread
