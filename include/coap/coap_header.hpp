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

/**
 * This class implements CoAP header generation and parsing.
 *
 */
class Header
{
public:
    /**
     * This method initializes the CoAP header.
     *
     */
    void Init(void);

    /**
     * This method parses the CoAP header from a message.
     *
     * @param[in]  aMessage  A reference to the message.
     *
     * @retval kThreadError_None   Successfully parsed the message.
     * @retval kThreadError_Parse  Failed to parse the message.
     *
     */
    ThreadError FromMessage(const Message &aMessage);

    /**
     * This method returns the Version value.
     *
     * @returns The Version value.
     *
     */
    uint8_t GetVersion(void) const { return (mHeader[0] & kVersionMask) >> kVersionOffset; }

    /**
     * This method sets the Version value.
     *
     * @param[in]  aVersion  The Version value.
     *
     */
    void SetVersion(uint8_t aVersion) { mHeader[0] &= ~kVersionMask; mHeader[0] |= aVersion << kVersionOffset; }

    /**
     * CoAP Type values.
     *
     */
    enum Type
    {
        kTypeConfirmable    = 0x00,  ///< Confirmable
        kTypeNonConfirmable = 0x10,  ///< Non-confirmable
        kTypeAcknowledgment = 0x20,  ///< Acknowledgment
        kTypeReset          = 0x30,  ///< Reset
    };

    /**
     * This method returns the Type value.
     *
     * @returns The Type value.
     *
     */
    Type GetType(void) const { return static_cast<Header::Type>(mHeader[0] & kTypeMask); }

    /**
     * This method sets the Type value.
     *
     * @param[in]  aType  The Type value.
     *
     */
    void SetType(Type aType) { mHeader[0] &= ~kTypeMask; mHeader[0] |= aType; }

    /**
     * CoAP Code values.
     *
     */
    enum Code
    {
        kCodeGet     = 0x01,  ///< Get
        kCodePost    = 0x02,  ///< Post
        kCodePut     = 0x03,  ///< Put
        kCodeDelete  = 0x04,  ///< Delete
        kCodeChanged = 0x44,  ///< Changed
        kCodeContent = 0x45,  ///< Content
    };

    /**
     * This method returns the Code value.
     *
     * @returns The Code value.
     *
     */
    Code GetCode(void) const { return static_cast<Header::Code>(mHeader[1]); }

    /**
     * This method sets the Code value.
     *
     * @param[in]  aCode  The Code value.
     *
     */
    void SetCode(Code aCode) { mHeader[1] = aCode; }

    /**
     * This method returns the Message ID value.
     *
     * @returns The Message ID value.
     *
     */
    uint16_t GetMessageId(void) const { return (static_cast<uint16_t>(mHeader[2]) << 8) | mHeader[3]; }

    /**
     * This method sets the Message ID value.
     *
     * @param[in]  aMessageId  The Message ID value.
     *
     */
    void SetMessageId(uint16_t aMessageId) { mHeader[2] = aMessageId >> 8; mHeader[3] = aMessageId; }

    enum
    {
        kTokenLengthMask = 0x0f,
        kTokenLengthOffset = 0,
        kTokenOffset = 4,
        kMaxTokenLength = 8,
    };

    /**
     * This method returns the Token length.
     *
     * @returns The Token length.
     *
     */
    uint8_t GetTokenLength(void) const { return (mHeader[0] & kTokenLengthMask) >> kTokenLengthOffset; }

    /**
     * This method returns a pointer to the Token value.
     *
     * @returns A pointer to the Token value.
     *
     */
    const uint8_t *GetToken(void) const { return mHeader + kTokenOffset; }

    /**
     * This method sets the Token value and length.
     *
     * @param[in]  aToken        A pointer to the Token value.
     * @param[in]  aTokenLength  The Length of @p aToken.
     *
     */
    void SetToken(const uint8_t *aToken, uint8_t aTokenLength)
    {
	mHeader[0] = (mHeader[0] & ~kTokenLengthMask) | (aTokenLength << kTokenLengthOffset);
	memcpy(mHeader + kTokenOffset, aToken, aTokenLength);
	mHeaderLength += aTokenLength;
    }

    /**
     * This structure represents a CoAP option.
     *
     */
    struct Option
    {
	/**
	 * Protocol Constants
	 *
	 */
	enum
	{
            kOptionDeltaOffset   = 4,    ///< Delta 
	};

	/**
	 * Option Numbers
	 */
        enum Type
        {
            kOptionUriPath       = 11,   ///< Uri-Path
            kOptionContentFormat = 12,   ///< Content-Format
        };

        uint16_t       mNumber;  ///< Option Number
        uint16_t       mLength;  ///< Option Length
        const uint8_t *mValue;   ///< A pointer to the Option Value
    };

    /**
     * This method appends a CoAP option.
     *
     * @param[in]  aOption  The CoAP Option.
     *
     * @retval kThreadError_None         Successfully appended the option.
     * @retval kThreadError_InvalidArgs  The option type is not equal or gerater than the last option type.
     *
     */
    ThreadError AppendOption(const Option &aOption);

    /**
     * This method appends a Uri-Path option.
     *
     * @param[in]  aUriPath  A pointer to a NULL-terminated string.
     *
     * @retval kThreadError_None         Successfully appended the option.
     * @retval kThreadError_InvalidArgs  The option type is not equal or gerater than the last option type.
     *
     */
    ThreadError AppendUriPathOptions(const char *aUriPath);

    /**
     * Media Types
     *
     */
    enum MediaType
    {
        kApplicationOctetStream = 42,  ///< application/octet-stream
    };

    /**
     * This method appends a Content-Format option.
     *
     * @param[in]  aType  The Media Type value.
     *
     * @retval kThreadError_None         Successfully appended the option.
     * @retval kThreadError_InvalidArgs  The option type is not equal or gerater than the last option type.
     *
     */
    ThreadError AppendContentFormatOption(MediaType aType);

    /**
     * This method returns a pointer to the current option.
     *
     * @returns A pointer to the current option.
     *
     */
    const Option *GetCurrentOption(void) const;

    /**
     * This method returns a pointer to the next option.
     *
     * @returns A pointer to the next option.
     *
     */
    const Option *GetNextOption(void);

    /**
     * This method terminates the CoAP header.
     *
     */
    void Finalize(void) { mHeader[mHeaderLength++] = 0xff; }

    /**
     * This method returns a pointer to the first byte of the header.
     *
     * @returns A pointer to the first byte of the header.
     *
     */
    const uint8_t *GetBytes(void) const { return mHeader; }

    /**
     * This method returns the header length in bytes.
     *
     * @returns The header length in bytes.
     *
     */
    uint8_t GetLength(void) const { return mHeaderLength; }

private:
    enum
    {
        kVersionMask = 0xc0,
        kVersionOffset = 6,
    };

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
