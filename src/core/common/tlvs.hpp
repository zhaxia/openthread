/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes definitions for generating and processing MLE TLVs.
 */

#ifndef TLVS_HPP_
#define TLVS_HPP_

#include "openthread-core-config.h"

#include "utils/wrap_string.h"

#include <openthread/error.h>
#include <openthread/platform/toolchain.h>

#include "common/encoding.hpp"

namespace ot {

using ot::Encoding::BigEndian::HostSwap16;
using ot::Encoding::BigEndian::HostSwap32;

class Message;

/**
 * This class implements TLV generation and parsing.
 *
 */
OT_TOOL_PACKED_BEGIN
class Tlv
{
public:
    /**
     * Length values.
     *
     */
    enum
    {
        kBaseTlvMaxLength = 254, ///< The maximum length of the Base TLV format.
    };

    /**
     * This method returns the Type value.
     *
     * @returns The Type value.
     *
     */
    uint8_t GetType(void) const { return mType; }

    /**
     * This method sets the Type value.
     *
     * @param[in]  aType  The Type value.
     *
     */
    void SetType(uint8_t aType) { mType = aType; }

    /**
     * This method indicates whether the TLV is an Extended TLV.
     *
     * @retval TRUE  If the TLV is an Extended TLV.
     * @retval FALSE If the TLV is not an Extended TLV.
     *
     */
    bool IsExtended(void) const { return (mLength == kExtendedLength); }

    /**
     * This method returns the Length value.
     *
     * @note This method should be used when TLV is not an Extended TLV, otherwise the returned length from this method
     * would not be correct. When TLV is an Extended TLV, the TLV should be down-casted to the `ExtendedTlv` type and
     * the `ExtendedTlv::GetLength()` should be used instead.
     *
     * @returns The Length value.
     *
     */
    uint8_t GetLength(void) const { return mLength; }

    /**
     * This method sets the Length value.
     *
     * @param[in]  aLength  The Length value.
     *
     */
    void SetLength(uint8_t aLength) { mLength = aLength; }

    /**
     * This method returns the TLV's total size (number of bytes) including Type, Length, and Value fields.
     *
     * This method correctly returns the TLV size independent of whether the TLV is an Extended TLV or not.
     *
     * @returns The total size include Type, Length, and Value fields.
     *
     */
    uint16_t GetSize(void) const;

    /**
     * This method returns a pointer to the Value.
     *
     * This method can be used independent of whether the TLV is an Extended TLV or not.
     *
     * @returns A pointer to the value.
     *
     */
    uint8_t *GetValue(void);

    /**
     * This method returns a pointer to the Value.
     *
     * This method can be used independent of whether the TLV is an Extended TLV or not.
     *
     * @returns A pointer to the value.
     *
     */
    const uint8_t *GetValue(void) const;

    /**
     * This method returns a pointer to the next TLV.
     *
     * This method correctly returns the next TLV independent of whether the current TLV is an Extended TLV or not.
     *
     * @returns A pointer to the next TLV.
     *
     */
    Tlv *GetNext(void) { return reinterpret_cast<Tlv *>(reinterpret_cast<uint8_t *>(this) + GetSize()); }

    /**
     * This method returns a pointer to the next TLV.
     *
     * This method correctly returns the next TLV independent of whether the current TLV is an Extended TLV or not.
     *
     * @returns A pointer to the next TLV.
     *
     */
    const Tlv *GetNext(void) const
    {
        return reinterpret_cast<const Tlv *>(reinterpret_cast<const uint8_t *>(this) + GetSize());
    }

    /**
     * This static method reads the requested TLV out of @p aMessage.
     *
     * This method can be used independent of whether the read TLV (from message) is an Extended TLV or not.
     *
     * @param[in]   aMessage    A reference to the message.
     * @param[in]   aType       The Type value to search for.
     * @param[in]   aMaxSize    Maximum number of bytes to read.
     * @param[out]  aTlv        A reference to the TLV that will be copied to.
     *
     * @retval OT_ERROR_NONE       Successfully copied the TLV.
     * @retval OT_ERROR_NOT_FOUND  Could not find the TLV with Type @p aType.
     *
     */
    static otError Get(const Message &aMessage, uint8_t aType, uint16_t aMaxSize, Tlv &aTlv);

    /**
     * This static method obtains the offset of a TLV within @p aMessage.
     *
     * This method can be used independent of whether the read TLV (from message) is an Extended TLV or not.
     *
     * @param[in]   aMessage    A reference to the message.
     * @param[in]   aType       The Type value to search for.
     * @param[out]  aOffset     A reference to the offset of the TLV.
     *
     * @retval OT_ERROR_NONE       Successfully copied the TLV.
     * @retval OT_ERROR_NOT_FOUND  Could not find the TLV with Type @p aType.
     *
     */
    static otError GetOffset(const Message &aMessage, uint8_t aType, uint16_t &aOffset);

    /**
     * This static method finds the offset and length of a given TLV type.
     *
     * This method can be used independent of whether the read TLV (from message) is an Extended TLV or not.
     *
     * @param[in]   aMessage    A reference to the message.
     * @param[in]   aType       The Type value to search for.
     * @param[out]  aOffset     The offset where the value starts.
     * @param[out]  aLength     The length of the value.
     *
     * @retval OT_ERROR_NONE       Successfully found the TLV.
     * @retval OT_ERROR_NOT_FOUND  Could not find the TLV with Type @p aType.
     *
     */
    static otError GetValueOffset(const Message &aMessage, uint8_t aType, uint16_t &aOffset, uint16_t &aLength);

protected:
    enum
    {
        kExtendedLength = 255, ///< Extended Length value
    };

private:
    /**
     * This private static method searches within a given message for TLV type and outputs the TLV offset, size and
     * whether it is an Extended TLV.
     *
     * A NULL pointer can be used for output parameters @p aOffset, @p aSize, or @p aIsExtendedTlv if the parameter
     * is not required.
     *
     * @param[in]   aMessage       A reference to the message to search within.
     * @param[in]   aType          The TLV type to search for.
     * @param[out]  aOffset        A pointer to a variable to output the offset to the start of the TLV.
     * @param[out]  aSize          A pointer to a variable to output the size (total number of bytes) of the TLV.
     * @param[out]  aIsExtendedTlv A pointer to a boolean variable to output whether the found TLV is extended or not.
     *
     * @retval OT_ERROR_NONE       Successfully found the TLV.
     * @retval OT_ERROR_NOT_FOUND  Could not find the TLV with Type @p aType.
     *
     */
    static otError Find(const Message &aMessage,
                        uint8_t        aType,
                        uint16_t *     aOffset,
                        uint16_t *     aSize,
                        bool *         aIsExtendedTlv);

    uint8_t mType;
    uint8_t mLength;
} OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
class ExtendedTlv : public Tlv
{
public:
    /**
     * This method returns the Length value.
     *
     */
    uint16_t GetLength(void) const { return HostSwap16(mLength); }

    /**
     * This method sets the Length value.
     *
     * @param[in]  aLength  The Length value.
     *
     */
    void SetLength(uint16_t aLength)
    {
        Tlv::SetLength(kExtendedLength);
        mLength = HostSwap16(aLength);
    }

private:
    uint16_t mLength;
} OT_TOOL_PACKED_END;

} // namespace ot

#endif // TLVS_HPP_
