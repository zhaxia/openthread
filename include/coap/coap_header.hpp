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
 *   This file includes definitions for generating and processing CoAP headers.
 */

#ifndef COAP_HEADER_HPP_
#define COAP_HEADER_HPP_

#include <common/message.hpp>

namespace Thread {

/**
 * @namespace Thread::Coap
 * @brief
 *   This namespace includes definitions for CoAP.
 *
 */
namespace Coap {

/**
 * @addtogroup core-coap
 *
 * @brief
 *   This module includes definitions for CoAP.
 *
 * @{
 *
 */

class Header
{
public:
    ThreadError Init();
    ThreadError FromMessage(const Message &message);

    enum
    {
        kVersionMask = 0xc0,
        kVersionOffset = 6,
    };
    uint8_t GetVersion() const;
    ThreadError SetVersion(uint8_t version);

    enum Type
    {
        kTypeConfirmable = 0x00,
        kTypeNonConfirmable = 0x10,
        kTypeAcknowledgment = 0x20,
        kTypeReset = 0x30,
    };
    Type GetType() const;
    ThreadError SetType(Type type);

    enum Code
    {
        kCodeGet = 0x01,
        kCodePost = 0x02,
        kCodePut = 0x03,
        kCodeDelete = 0x04,
        kCodeChanged = 0x44,
        kCodeContent = 0x45,
    };
    Code GetCode() const;
    ThreadError SetCode(Code code);

    uint16_t GetMessageId() const;
    ThreadError SetMessageId(uint16_t messageId);

    enum
    {
        kTokenLengthMask = 0x0f,
        kTokenLengthOffset = 0,
        kTokenOffset = 4,
        kMaxTokenLength = 8,
    };
    uint8_t GetTokenLength() const;
    const uint8_t *GetToken() const;
    ThreadError SetToken(const uint8_t *token, uint8_t token_length);

    struct Option
    {
        enum
        {
            kOptionDeltaOffset = 4,
            kOptionUriPath = 11,
            kOptionContentFormat = 12,
        };
        uint16_t mNumber;
        uint16_t mLength;
        const uint8_t *mValue;
    };
    ThreadError AppendOption(const Option &option);
    ThreadError AppendUriPathOptions(const char *uriPath);

    enum
    {
        kApplicationOctetStream = 42,
    };
    ThreadError AppendContentFormatOption(uint8_t type);
    const Option *GetCurrentOption() const;
    const Option *GetNextOption();

    ThreadError Finalize();

    const uint8_t *GetBytes() const;
    uint8_t GetLength() const;

private:
    enum
    {
        kTypeMask = 0x30,
        kMaxHeaderLength = 128,
    };
    uint8_t mHeader[kMaxHeaderLength];
    uint8_t mHeaderLength = 4;
    uint16_t mOptionLast = 0;
    uint16_t mNextOptionOffset = 0;
    Option mOption = {0, 0, NULL};
};

/**
 * @}
 *
 */

}  // namespace Coap
}  // namespace Thread

#endif  // COAP_HEADER_HPP_
