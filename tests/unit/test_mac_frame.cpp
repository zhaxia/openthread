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

#include "test_util.h"
#include <common/debug.hpp>
#include <mac/mac_frame.hpp>
#include <string.h>

namespace Thread {

void TestMacHeader()
{
    static const struct
    {
        uint16_t fcf;
        uint8_t sec_ctl;
        uint8_t header_length;
    } tests[] =
    {
        { Mac::Frame::kFcfDstAddrNone | Mac::Frame::kFcfSrcAddrNone, 0, 3 },
        { Mac::Frame::kFcfDstAddrNone | Mac::Frame::kFcfSrcAddrShort, 0, 7 },
        { Mac::Frame::kFcfDstAddrNone | Mac::Frame::kFcfSrcAddrExt, 0, 13 },
        { Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrNone, 0, 7 },
        { Mac::Frame::kFcfDstAddrExt | Mac::Frame::kFcfSrcAddrNone, 0, 13 },
        { Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrShort, 0, 11 },
        { Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrExt, 0, 17 },
        { Mac::Frame::kFcfDstAddrExt | Mac::Frame::kFcfSrcAddrShort, 0, 17 },
        { Mac::Frame::kFcfDstAddrExt | Mac::Frame::kFcfSrcAddrExt, 0, 23 },

        { Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrShort | Mac::Frame::kFcfPanidCompression, 0, 9 },
        { Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrExt | Mac::Frame::kFcfPanidCompression, 0, 15 },
        { Mac::Frame::kFcfDstAddrExt | Mac::Frame::kFcfSrcAddrShort | Mac::Frame::kFcfPanidCompression, 0, 15 },
        { Mac::Frame::kFcfDstAddrExt | Mac::Frame::kFcfSrcAddrExt | Mac::Frame::kFcfPanidCompression, 0, 21 },

        {
            Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrShort | Mac::Frame::kFcfPanidCompression |
            Mac::Frame::kFcfSecurityEnabled, Mac::Frame::kSecMic32 | Mac::Frame::kKeyIdMode1, 15
        },
        {
            Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrShort | Mac::Frame::kFcfPanidCompression |
            Mac::Frame::kFcfSecurityEnabled, Mac::Frame::kSecMic32 | Mac::Frame::kKeyIdMode5, 19
        },
    };

    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        Mac::Frame frame;
        frame.InitMacHeader(tests[i].fcf, tests[i].sec_ctl);
        printf("%d\n", frame.GetHeaderLength());
        VerifyOrQuit(frame.GetHeaderLength() == tests[i].header_length,
                     "MacHeader test failed\n");
    }
}

}  // namespace Thread

int main()
{
    Thread::TestMacHeader();
    printf("All tests passed\n");
    return 0;
}
