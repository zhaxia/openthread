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

namespace Thread {
namespace Coap {

void Header::Init(void)
{
    mHeaderLength = kMinHeaderLength;
    mOptionLast = 0;
    mNextOptionOffset = 0;
    mOption = {0, 0, NULL};
    memset(mHeader, 0, sizeof(mHeader));
}

ThreadError Header::FromMessage(const Message &aMessage)
{
    ThreadError error = kThreadError_Parse;
    uint16_t offset = aMessage.GetOffset();
    uint16_t length = aMessage.GetLength() - aMessage.GetOffset();
    uint8_t tokenLength;
    bool firstOption = true;
    uint16_t optionDelta;
    uint16_t optionLength;

    VerifyOrExit(length >= kTokenOffset, error = kThreadError_Parse);
    aMessage.Read(offset, kTokenOffset, mHeader);
    mHeaderLength = kTokenOffset;
    offset += kTokenOffset;
    length -= kTokenOffset;

    VerifyOrExit(GetVersion() == 1, error = kThreadError_Parse);

    tokenLength = GetTokenLength();
    VerifyOrExit(tokenLength <= kMaxTokenLength && tokenLength < length, error = kThreadError_Parse);
    aMessage.Read(offset, tokenLength, mHeader + mHeaderLength);
    mHeaderLength += tokenLength;
    offset += tokenLength;
    length -= tokenLength;

    while (length > 0)
    {
        aMessage.Read(offset, 5, mHeader + mHeaderLength);

        if (mHeader[mHeaderLength] == 0xff)
        {
            mHeaderLength += sizeof(uint8_t);
            ExitNow(error = kThreadError_None);
        }

        optionDelta = mHeader[mHeaderLength] >> 4;
        optionLength = mHeader[mHeaderLength] & 0xf;
        mHeaderLength += sizeof(uint8_t);
        offset += sizeof(uint8_t);
        length -= sizeof(uint8_t);

        if (optionDelta < kOption1ByteExtension)
        {
            // do nothing
        }
        else if (optionDelta == kOption1ByteExtension)
        {
            optionDelta = kOption1ByteExtensionOffset + mHeader[mHeaderLength];
            mHeaderLength += sizeof(uint8_t);
            offset += sizeof(uint8_t);
            length -= sizeof(uint8_t);
        }
        else if (optionDelta == kOption2ByteExtension)
        {
            optionDelta = kOption2ByteExtensionOffset + ((static_cast<uint16_t>(mHeader[mHeaderLength]) << 8) |
                                                         mHeader[mHeaderLength + 1]);
            mHeaderLength += sizeof(uint16_t);
            offset += sizeof(uint16_t);
            length -= sizeof(uint16_t);
        }
        else
        {
            ExitNow(error = kThreadError_Parse);
        }

        if (optionLength < kOption1ByteExtension)
        {
            // do nothing
        }
        else if (optionLength == kOption1ByteExtension)
        {
            optionLength = kOption1ByteExtensionOffset + mHeader[mHeaderLength];
            mHeaderLength += sizeof(uint8_t);
            offset += sizeof(uint8_t);
            length -= sizeof(uint8_t);
        }
        else if (optionLength == kOption2ByteExtension)
        {
            optionLength = kOption2ByteExtensionOffset + ((static_cast<uint16_t>(mHeader[mHeaderLength]) << 8) |
                                                          mHeader[mHeaderLength + 1]);
            mHeaderLength += sizeof(uint16_t);
            offset += sizeof(uint16_t);
            length -= sizeof(uint16_t);
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
        aMessage.Read(offset, optionLength, mHeader + mHeaderLength);
        mHeaderLength += optionLength;
        offset += optionLength;
        length -= optionLength;
    }

exit:
    return error;
}

ThreadError Header::AppendOption(const Option &aOption)
{
    uint8_t *buf = mHeader + mHeaderLength;
    uint8_t *cur = buf + 1;
    uint16_t optionDelta = aOption.mNumber - mOptionLast;
    uint16_t optionLength;

    if (optionDelta < kOption1ByteExtensionOffset)
    {
        *buf = optionDelta << Option::kOptionDeltaOffset;
    }
    else if (optionDelta < kOption2ByteExtensionOffset)
    {
        *buf |= kOption1ByteExtension << Option::kOptionDeltaOffset;
        *cur++ = optionDelta - kOption1ByteExtensionOffset;
    }
    else
    {
        *buf |= kOption2ByteExtension << Option::kOptionDeltaOffset;
        optionDelta -= kOption2ByteExtensionOffset;
        *cur++ = optionDelta >> 8;
        *cur++ = optionDelta;
    }

    if (aOption.mLength < kOption1ByteExtensionOffset)
    {
        *buf |= aOption.mLength;
    }
    else if (aOption.mLength < kOption2ByteExtensionOffset)
    {
        *buf |= kOption1ByteExtension;
        *cur++ = aOption.mLength - kOption1ByteExtensionOffset;
    }
    else
    {
        *buf |= kOption2ByteExtension;
        optionLength = aOption.mLength - kOption2ByteExtensionOffset;
        *cur++ = optionLength >> 8;
        *cur++ = optionLength;
    }

    memcpy(cur, aOption.mValue, aOption.mLength);
    cur += aOption.mLength;

    mHeaderLength += cur - buf;
    mOptionLast = aOption.mNumber;

    return kThreadError_None;
}

ThreadError Header::AppendUriPathOptions(const char *aUriPath)
{
    const char *cur = aUriPath;
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

ThreadError Header::AppendContentFormatOption(MediaType aType)
{
    Option coapOption;
    uint8_t type = aType;

    coapOption.mNumber = Option::kOptionContentFormat;
    coapOption.mLength = 1;
    coapOption.mValue = &type;
    AppendOption(coapOption);

    return kThreadError_None;
}

const Header::Option *Header::GetCurrentOption(void) const
{
    return &mOption;
}

const Header::Option *Header::GetNextOption(void)
{
    Option *rval = NULL;
    uint16_t optionDelta;
    uint16_t optionLength;

    VerifyOrExit(mNextOptionOffset < mHeaderLength, ;);

    optionDelta = mHeader[mNextOptionOffset] >> 4;
    optionLength = mHeader[mNextOptionOffset] & 0xf;
    mNextOptionOffset += sizeof(uint8_t);

    if (optionDelta < kOption1ByteExtension)
    {
        // do nothing
    }
    else if (optionDelta == kOption1ByteExtension)
    {
        optionDelta = kOption1ByteExtensionOffset + mHeader[mNextOptionOffset];
        mNextOptionOffset += sizeof(uint8_t);
    }
    else if (optionDelta == kOption2ByteExtension)
    {
        optionDelta = kOption2ByteExtensionOffset + ((static_cast<uint16_t>(mHeader[mNextOptionOffset]) << 8) |
                                                     mHeader[mNextOptionOffset + 1]);
        mNextOptionOffset += sizeof(uint16_t);
    }
    else
    {
        ExitNow();
    }

    if (optionLength < kOption1ByteExtension)
    {
        // do nothing
    }
    else if (optionLength == kOption1ByteExtension)
    {
        optionLength = kOption1ByteExtensionOffset + mHeader[mNextOptionOffset];
        mNextOptionOffset += sizeof(uint8_t);
    }
    else if (optionLength == kOption2ByteExtension)
    {
        optionLength = kOption2ByteExtensionOffset + ((static_cast<uint16_t>(mHeader[mNextOptionOffset]) << 8) |
                                                      mHeader[mNextOptionOffset + 1]);
        mNextOptionOffset += sizeof(uint16_t);
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

}  // namespace Coap
}  // namespace Thread
