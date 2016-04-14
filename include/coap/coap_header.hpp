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

#ifndef COAP_HEADER_HPP_
#define COAP_HEADER_HPP_

#include <common/message.hpp>

namespace Thread {
namespace Coap {

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
    ThreadError SetMessageId(uint16_t message_id);

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
        uint16_t number;
        uint16_t length;
        const uint8_t *value;
    };
    ThreadError AppendOption(const Option &option);
    ThreadError AppendUriPathOptions(const char *uri_path);

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
    uint8_t m_header[kMaxHeaderLength];
    uint8_t m_header_length = 4;
    uint16_t m_option_last = 0;
    uint16_t m_next_option_offset = 0;
    Option m_option = {0, 0, NULL};
};

}  // namespace Coap
}  // namespace Thread

#endif  // COAP_COAP_HEADER_HPP_
