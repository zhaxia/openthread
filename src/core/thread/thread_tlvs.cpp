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

#include <common/code_utils.hpp>
#include <common/message.hpp>
#include <thread/thread_tlvs.hpp>

namespace Thread {

ThreadError ThreadTlv::GetTlv(const Message &message, Type type, uint16_t max_length, ThreadTlv &tlv)
{
    ThreadError error = kThreadError_Parse;
    uint16_t offset = message.GetOffset();
    uint16_t end = message.GetLength();

    while (offset < end)
    {
        message.Read(offset, sizeof(ThreadTlv), &tlv);

        if (tlv.GetType() == type && (offset + sizeof(tlv) + tlv.GetLength()) <= end)
        {
            if (max_length > sizeof(tlv) + tlv.GetLength())
            {
                max_length = sizeof(tlv) + tlv.GetLength();
            }

            message.Read(offset, max_length, &tlv);

            ExitNow(error = kThreadError_None);
        }

        offset += sizeof(tlv) + tlv.GetLength();
    }

exit:
    return error;
}

}
