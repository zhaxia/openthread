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
 *   This file implements common methods for manipulating MLE TLVs.
 */

#include <common/code_utils.hpp>
#include <common/message.hpp>
#include <thread/mle_tlvs.hpp>

namespace Thread {
namespace Mle {

ThreadError Tlv::GetTlv(const Message &aMessage, Type aType, uint16_t aMaxLength, Tlv &aTlv)
{
    ThreadError error = kThreadError_Parse;
    uint16_t offset = aMessage.GetOffset();
    uint16_t end = aMessage.GetLength();

    while (offset < end)
    {
        aMessage.Read(offset, sizeof(Tlv), &aTlv);

        if (aTlv.GetType() == aType && (offset + sizeof(aTlv) + aTlv.GetLength()) <= end)
        {
            if (aMaxLength > sizeof(aTlv) + aTlv.GetLength())
            {
                aMaxLength = sizeof(aTlv) + aTlv.GetLength();
            }

            aMessage.Read(offset, aMaxLength, &aTlv);

            ExitNow(error = kThreadError_None);
        }

        offset += sizeof(aTlv) + aTlv.GetLength();
    }

exit:
    return error;
}

}  // namespace Mle
}  // namespace Thread
