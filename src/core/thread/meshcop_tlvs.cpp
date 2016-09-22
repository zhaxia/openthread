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
 *   This file implements common MeshCoP TLV processing.
 */

#include <common/code_utils.hpp>
#include <thread/meshcop_tlvs.hpp>

namespace Thread {
namespace MeshCoP {

int Timestamp::Compare(const Timestamp &aCompare) const
{
    uint64_t thisSeconds = GetSeconds();
    uint64_t compareSeconds = aCompare.GetSeconds();
    uint16_t thisTicks = GetTicks();
    uint16_t compareTicks = aCompare.GetTicks();
    int rval;

    if (compareSeconds > thisSeconds)
    {
        rval = 1;
    }
    else if (compareSeconds < thisSeconds)
    {
        rval = -1;
    }
    else if (compareTicks > thisTicks)
    {
        rval = 1;
    }
    else if (compareTicks < thisTicks)
    {
        rval = -1;
    }
    else
    {
        rval = 0;
    }

    return rval;
}

ThreadError Tlv::GetTlv(const Message &message, Type type, uint16_t maxLength, Tlv &tlv)
{
    ThreadError error = kThreadError_Parse;
    uint16_t offset = message.GetOffset();
    uint16_t end = message.GetLength();

    while (offset < end)
    {
        message.Read(offset, sizeof(Tlv), &tlv);

        if (tlv.GetType() == type && (offset + sizeof(tlv) + tlv.GetLength()) <= end)
        {
            if (maxLength > sizeof(tlv) + tlv.GetLength())
            {
                maxLength = sizeof(tlv) + tlv.GetLength();
            }

            message.Read(offset, maxLength, &tlv);

            ExitNow(error = kThreadError_None);
        }

        offset += sizeof(tlv) + tlv.GetLength();
    }

exit:
    return error;
}

ThreadError Tlv::GetValueOffset(const Message &aMessage, Type aType, uint16_t &aOffset, uint16_t &aLength)
{
    ThreadError error = kThreadError_Parse;
    uint16_t offset = aMessage.GetOffset();
    uint16_t end = aMessage.GetLength();

    while (offset < end)
    {
        Tlv tlv;
        uint16_t length;

        aMessage.Read(offset, sizeof(tlv), &tlv);
        offset += sizeof(tlv);

        length = tlv.GetLength();

        if (length == kExtendedLength)
        {
            aMessage.Read(offset, sizeof(length), &length);
            offset += sizeof(length);
            length = HostSwap16(length);
        }

        if (tlv.GetType() == aType)
        {
            aOffset = offset;
            aLength = length;
            ExitNow(error = kThreadError_None);
        }

        offset += length;
    }

exit:
    return error;
}

}  // namespace MeshCoP
}  // namespace Thread
