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
 *   This file implements the CoAP header generation and parsing.
 */

#include <coap/coap_header.hpp>
#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/thread_error.hpp>

namespace Thread {
namespace Coap {

ThreadError Header::Init()
{
    mHeaderLength = 4;
    mOptionLast = 0;
    mNextOptionOffset = 0;
    mOption = {0, 0, NULL};
    memset(mHeader, 0, sizeof(mHeader));
    return kThreadError_None;
}

ThreadError Header::FromMessage(const Message &message)
{
    ThreadError error = kThreadError_Parse;
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();
    uint8_t tokenLength;
    bool firstOption = true;
    uint16_t optionDelta;
    uint16_t optionLength;

    VerifyOrExit(length >= kTokenOffset, error = kThreadError_Parse);
    message.Read(offset, kTokenOffset, mHeader);
    mHeaderLength = kTokenOffset;
    offset += kTokenOffset;
    length -= kTokenOffset;

    VerifyOrExit(GetVersion() == 1, error = kThreadError_Parse);

    tokenLength = GetTokenLength();
    VerifyOrExit(tokenLength <= kMaxTokenLength && tokenLength < length, error = kThreadError_Parse);
    message.Read(offset, tokenLength, mHeader + mHeaderLength);
    mHeaderLength += tokenLength;
    offset += tokenLength;
    length -= tokenLength;

    while (length > 0)
    {
        message.Read(offset, 5, mHeader + mHeaderLength);

        if (mHeader[mHeaderLength] == 0xff)
        {
            mHeaderLength++;
            ExitNow(error = kThreadError_None);
        }

        optionDelta = mHeader[mHeaderLength] >> 4;
        optionLength = mHeader[mHeaderLength] & 0xf;
        mHeaderLength++;
        offset++;
        length--;

        if (optionDelta < 13)
        {
            // do nothing
        }
        else if (optionDelta == 13)
        {
            optionDelta = 13 + mHeader[mHeaderLength];
            mHeaderLength++;
            offset++;
            length--;
        }
        else if (optionDelta == 14)
        {
            optionDelta = 269 + ((static_cast<uint16_t>(mHeader[mHeaderLength]) << 8) |
                                 mHeader[mHeaderLength + 1]);
            mHeaderLength += 2;
            offset += 2;
            length -= 2;
        }
        else
        {
            ExitNow(error = kThreadError_Parse);
        }

        if (optionLength < 13)
        {
            // do nothing
        }
        else if (optionLength == 13)
        {
            optionLength = 13 + mHeader[mHeaderLength];
            mHeaderLength++;
            offset++;
            length--;
        }
        else if (optionLength == 14)
        {
            optionLength = 269 + ((static_cast<uint16_t>(mHeader[mHeaderLength]) << 8) |
                                  mHeader[mHeaderLength + 1]);
            mHeaderLength += 2;
            offset += 2;
            length -= 2;
        }
        else
        {
            ExitNow(error = kThreadError_Parse);
        }

        if (firstOption)
        {
            mOption.mNumber = optionDelta;
            mOption.mLength = optionLength;
            mOption.mValue = mHeader + mHeaderLength;
            mNextOptionOffset = mHeaderLength + optionLength;
            firstOption = false;
        }

        VerifyOrExit(optionLength <= length, error = kThreadError_Parse);
        message.Read(offset, optionLength, mHeader + mHeaderLength);
        mHeaderLength += optionLength;
        offset += optionLength;
        length -= optionLength;
    }

exit:
    return error;
}

uint8_t Header::GetVersion() const
{
    return (mHeader[0] & kVersionMask) >> kVersionOffset;
}

ThreadError Header::SetVersion(uint8_t version)
{
    mHeader[0] &= ~kVersionMask;
    mHeader[0] |= version << kVersionOffset;
    return kThreadError_None;
}

Header::Type Header::GetType() const
{
    return static_cast<Header::Type>(mHeader[0] & kTypeMask);
}

ThreadError Header::SetType(Header::Type type)
{
    mHeader[0] &= ~kTypeMask;
    mHeader[0] |= type;
    return kThreadError_None;
}

Header::Code Header::GetCode() const
{
    return static_cast<Header::Code>(mHeader[1]);
}

ThreadError Header::SetCode(Header::Code code)
{
    mHeader[1] = code;
    return kThreadError_None;
}

uint16_t Header::GetMessageId() const
{
    return (static_cast<uint16_t>(mHeader[2]) << 8) | mHeader[3];
}

ThreadError Header::SetMessageId(uint16_t message_id)
{
    mHeader[2] = message_id >> 8;
    mHeader[3] = message_id;
    return kThreadError_None;
}

const uint8_t *Header::GetToken() const
{
    return mHeader + kTokenOffset;
}

uint8_t Header::GetTokenLength() const
{
    return (mHeader[0] & kTokenLengthMask) >> kTokenLengthOffset;
}

ThreadError Header::SetToken(const uint8_t *token, uint8_t tokenLength)
{
    mHeader[0] &= ~kTokenLengthMask;
    mHeader[0] |= tokenLength << kTokenLengthOffset;
    memcpy(mHeader + kTokenOffset, token, tokenLength);
    mHeaderLength += tokenLength;
    return kThreadError_None;
}

ThreadError Header::AppendOption(const Option &option)
{
    uint8_t *buf = mHeader + mHeaderLength;
    uint8_t *cur = buf + 1;
    uint16_t optionDelta = option.mNumber - mOptionLast;
    uint16_t optionLength;

    if (optionDelta < 13)
    {
        *buf = optionDelta << Option::kOptionDeltaOffset;
    }
    else if (optionDelta < 269)
    {
        *buf |= 13 << Option::kOptionDeltaOffset;
        *cur++ = optionDelta - 13;
    }
    else
    {
        *buf |= 14 << Option::kOptionDeltaOffset;
        optionDelta -= 269;
        *cur++ = optionDelta >> 8;
        *cur++ = optionDelta;
    }

    if (option.mLength < 13)
    {
        *buf |= option.mLength;
    }
    else if (option.mLength < 269)
    {
        *buf |= 13;
        *cur++ = option.mLength - 13;
    }
    else
    {
        *buf |= 14;
        optionLength = option.mLength - 269;
        *cur++ = optionLength >> 8;
        *cur++ = optionLength;
    }

    memcpy(cur, option.mValue, option.mLength);
    cur += option.mLength;

    mHeaderLength += cur - buf;
    mOptionLast = option.mNumber;

    return kThreadError_None;
}

ThreadError Header::AppendUriPathOptions(const char *uri_path)
{
    const char *cur = uri_path;
    const char *end;
    Header::Option coapOption;

    coapOption.mNumber = Option::kOptionUriPath;

    while ((end = strchr(cur, '/')) != NULL)
    {
        coapOption.mLength = end - cur;
        coapOption.mValue = reinterpret_cast<const uint8_t *>(cur);
        AppendOption(coapOption);
        cur = end + 1;
    }

    coapOption.mLength = strlen(cur);
    coapOption.mValue = reinterpret_cast<const uint8_t *>(cur);
    AppendOption(coapOption);

    return kThreadError_None;
}

ThreadError Header::AppendContentFormatOption(uint8_t type)
{
    Option coapOption;

    coapOption.mNumber = Option::kOptionContentFormat;
    coapOption.mLength = 1;
    coapOption.mValue = &type;
    AppendOption(coapOption);

    return kThreadError_None;
}

const Header::Option *Header::GetCurrentOption() const
{
    return &mOption;
}

const Header::Option *Header::GetNextOption()
{
    Option *rval = NULL;
    uint16_t optionDelta;
    uint16_t optionLength;

    VerifyOrExit(mNextOptionOffset < mHeaderLength, ;);

    optionDelta = mHeader[mNextOptionOffset] >> 4;
    optionLength = mHeader[mNextOptionOffset] & 0xf;
    mNextOptionOffset++;

    if (optionDelta < 13)
    {
        // do nothing
    }
    else if (optionDelta == 13)
    {
        optionDelta = 13 + mHeader[mNextOptionOffset];
        mNextOptionOffset++;
    }
    else if (optionDelta == 14)
    {
        optionDelta = 269 + ((static_cast<uint16_t>(mHeader[mNextOptionOffset]) << 8) |
                             mHeader[mNextOptionOffset + 1]);
        mNextOptionOffset += 2;
    }
    else
    {
        ExitNow();
    }

    if (optionLength < 13)
    {
        // do nothing
    }
    else if (optionLength == 13)
    {
        optionLength = 13 + mHeader[mNextOptionOffset];
        mNextOptionOffset++;
    }
    else if (optionLength == 14)
    {
        optionLength = 269 + ((static_cast<uint16_t>(mHeader[mNextOptionOffset]) << 8) |
                              mHeader[mNextOptionOffset + 1]);
        mNextOptionOffset += 2;
    }
    else
    {
        ExitNow();
    }

    mOption.mNumber += optionDelta;
    mOption.mLength = optionLength;
    mOption.mValue = mHeader + mNextOptionOffset;
    mNextOptionOffset += optionLength;
    rval = &mOption;

exit:
    return rval;
}

ThreadError Header::Finalize()
{
    mHeader[mHeaderLength++] = 0xff;
    return kThreadError_None;
}

const uint8_t *Header::GetBytes() const
{
    return mHeader;
}

uint8_t Header::GetLength() const
{
    return mHeaderLength;
}

}  // namespace Coap
}  // namespace Thread
