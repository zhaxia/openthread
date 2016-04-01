/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#include <thread/thread_tlvs.h>
#include <common/code_utils.h>
#include <common/message.h>

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
