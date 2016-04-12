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

#include <coap/coap_header.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/thread_error.h>

namespace Thread {
namespace Coap {

ThreadError Header::Init()
{
    m_header_length = 4;
    m_option_last = 0;
    m_next_option_offset = 0;
    m_option = {0, 0, NULL};
    memset(m_header, 0, sizeof(m_header));
    return kThreadError_None;
}

ThreadError Header::FromMessage(const Message &message)
{
    ThreadError error = kThreadError_Parse;
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();
    uint8_t token_length;
    bool first_option = true;
    uint16_t option_delta;
    uint16_t option_length;

    VerifyOrExit(length >= kTokenOffset, error = kThreadError_Parse);
    message.Read(offset, kTokenOffset, m_header);
    m_header_length = kTokenOffset;
    offset += kTokenOffset;
    length -= kTokenOffset;

    VerifyOrExit(GetVersion() == 1, error = kThreadError_Parse);

    token_length = GetTokenLength();
    VerifyOrExit(token_length <= kMaxTokenLength && token_length < length, error = kThreadError_Parse);
    message.Read(offset, token_length, m_header + m_header_length);
    m_header_length += token_length;
    offset += token_length;
    length -= token_length;

    while (length > 0)
    {
        message.Read(offset, 5, m_header + m_header_length);

        if (m_header[m_header_length] == 0xff)
        {
            m_header_length++;
            ExitNow(error = kThreadError_None);
        }

        option_delta = m_header[m_header_length] >> 4;
        option_length = m_header[m_header_length] & 0xf;
        m_header_length++;
        offset++;
        length--;

        if (option_delta < 13)
        {
            // do nothing
        }
        else if (option_delta == 13)
        {
            option_delta = 13 + m_header[m_header_length];
            m_header_length++;
            offset++;
            length--;
        }
        else if (option_delta == 14)
        {
            option_delta = 269 + ((static_cast<uint16_t>(m_header[m_header_length]) << 8) |
                                  m_header[m_header_length + 1]);
            m_header_length += 2;
            offset += 2;
            length -= 2;
        }
        else
        {
            ExitNow(error = kThreadError_Parse);
        }

        if (option_length < 13)
        {
            // do nothing
        }
        else if (option_length == 13)
        {
            option_length = 13 + m_header[m_header_length];
            m_header_length++;
            offset++;
            length--;
        }
        else if (option_length == 14)
        {
            option_length = 269 + ((static_cast<uint16_t>(m_header[m_header_length]) << 8) |
                                   m_header[m_header_length + 1]);
            m_header_length += 2;
            offset += 2;
            length -= 2;
        }
        else
        {
            ExitNow(error = kThreadError_Parse);
        }

        if (first_option)
        {
            m_option.number = option_delta;
            m_option.length = option_length;
            m_option.value = m_header + m_header_length;
            m_next_option_offset = m_header_length + option_length;
            first_option = false;
        }

        VerifyOrExit(option_length <= length, error = kThreadError_Parse);
        message.Read(offset, option_length, m_header + m_header_length);
        m_header_length += option_length;
        offset += option_length;
        length -= option_length;
    }

exit:
    return error;
}

uint8_t Header::GetVersion() const
{
    return (m_header[0] & kVersionMask) >> kVersionOffset;
}

ThreadError Header::SetVersion(uint8_t version)
{
    m_header[0] &= ~kVersionMask;
    m_header[0] |= version << kVersionOffset;
    return kThreadError_None;
}

Header::Type Header::GetType() const
{
    return static_cast<Header::Type>(m_header[0] & kTypeMask);
}

ThreadError Header::SetType(Header::Type type)
{
    m_header[0] &= ~kTypeMask;
    m_header[0] |= type;
    return kThreadError_None;
}

Header::Code Header::GetCode() const
{
    return static_cast<Header::Code>(m_header[1]);
}

ThreadError Header::SetCode(Header::Code code)
{
    m_header[1] = code;
    return kThreadError_None;
}

uint16_t Header::GetMessageId() const
{
    return (static_cast<uint16_t>(m_header[2]) << 8) | m_header[3];
}

ThreadError Header::SetMessageId(uint16_t message_id)
{
    m_header[2] = message_id >> 8;
    m_header[3] = message_id;
    return kThreadError_None;
}

const uint8_t *Header::GetToken() const
{
    return m_header + kTokenOffset;
}

uint8_t Header::GetTokenLength() const
{
    return (m_header[0] & kTokenLengthMask) >> kTokenLengthOffset;
}

ThreadError Header::SetToken(const uint8_t *token, uint8_t token_length)
{
    m_header[0] &= ~kTokenLengthMask;
    m_header[0] |= token_length << kTokenLengthOffset;
    memcpy(m_header + kTokenOffset, token, token_length);
    m_header_length += token_length;
    return kThreadError_None;
}

ThreadError Header::AppendOption(const Option &option)
{
    uint8_t *buf = m_header + m_header_length;
    uint8_t *cur = buf + 1;
    uint16_t option_delta = option.number - m_option_last;
    uint16_t option_length;

    if (option_delta < 13)
    {
        *buf = option_delta << Option::kOptionDeltaOffset;
    }
    else if (option_delta < 269)
    {
        *buf |= 13 << Option::kOptionDeltaOffset;
        *cur++ = option_delta - 13;
    }
    else
    {
        *buf |= 14 << Option::kOptionDeltaOffset;
        option_delta -= 269;
        *cur++ = option_delta >> 8;
        *cur++ = option_delta;
    }

    if (option.length < 13)
    {
        *buf |= option.length;
    }
    else if (option.length < 269)
    {
        *buf |= 13;
        *cur++ = option.length - 13;
    }
    else
    {
        *buf |= 14;
        option_length = option.length - 269;
        *cur++ = option_length >> 8;
        *cur++ = option_length;
    }

    memcpy(cur, option.value, option.length);
    cur += option.length;

    m_header_length += cur - buf;
    m_option_last = option.number;

    return kThreadError_None;
}

ThreadError Header::AppendUriPathOptions(const char *uri_path)
{
    const char *cur = uri_path;
    const char *end;
    Header::Option coap_option;

    coap_option.number = Option::kOptionUriPath;

    while ((end = strchr(cur, '/')) != NULL)
    {
        coap_option.length = end - cur;
        coap_option.value = reinterpret_cast<const uint8_t *>(cur);
        AppendOption(coap_option);
        cur = end + 1;
    }

    coap_option.length = strlen(cur);
    coap_option.value = reinterpret_cast<const uint8_t *>(cur);
    AppendOption(coap_option);

    return kThreadError_None;
}

ThreadError Header::AppendContentFormatOption(uint8_t type)
{
    Option coap_option;

    coap_option.number = Option::kOptionContentFormat;
    coap_option.length = 1;
    coap_option.value = &type;
    AppendOption(coap_option);

    return kThreadError_None;
}

const Header::Option *Header::GetCurrentOption() const
{
    return &m_option;
}

const Header::Option *Header::GetNextOption()
{
    Option *rval = NULL;
    uint16_t option_delta;
    uint16_t option_length;

    VerifyOrExit(m_next_option_offset < m_header_length, ;);

    option_delta = m_header[m_next_option_offset] >> 4;
    option_length = m_header[m_next_option_offset] & 0xf;
    m_next_option_offset++;

    if (option_delta < 13)
    {
        // do nothing
    }
    else if (option_delta == 13)
    {
        option_delta = 13 + m_header[m_next_option_offset];
        m_next_option_offset++;
    }
    else if (option_delta == 14)
    {
        option_delta = 269 + ((static_cast<uint16_t>(m_header[m_next_option_offset]) << 8) |
                              m_header[m_next_option_offset + 1]);
        m_next_option_offset += 2;
    }
    else
    {
        ExitNow();
    }

    if (option_length < 13)
    {
        // do nothing
    }
    else if (option_length == 13)
    {
        option_length = 13 + m_header[m_next_option_offset];
        m_next_option_offset++;
    }
    else if (option_length == 14)
    {
        option_length = 269 + ((static_cast<uint16_t>(m_header[m_next_option_offset]) << 8) |
                               m_header[m_next_option_offset + 1]);
        m_next_option_offset += 2;
    }
    else
    {
        ExitNow();
    }

    m_option.number += option_delta;
    m_option.length = option_length;
    m_option.value = m_header + m_next_option_offset;
    m_next_option_offset += option_length;
    rval = &m_option;

exit:
    return rval;
}

ThreadError Header::Finalize()
{
    m_header[m_header_length++] = 0xff;
    return kThreadError_None;
}

const uint8_t *Header::GetBytes() const
{
    return m_header;
}

uint8_t Header::GetLength() const
{
    return m_header_length;
}

}  // namespace Coap
}  // namespace Thread
