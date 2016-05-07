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

#include "test_util.h"
#include <openthread.h>
#include <common/debug.hpp>
#include <mac/mac_frame.hpp>
#include <string.h>

namespace Thread {

extern"C" void otSignalTaskletPending(void)
{
}

void TestMacHeader(void)
{
    static const struct
    {
        uint16_t fcf;
        uint8_t secCtl;
        uint8_t headerLength;
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
            Mac::Frame::kFcfSecurityEnabled, Mac::Frame::kSecMic32 | Mac::Frame::kKeyIdMode2, 19
        },
    };

    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        Mac::Frame frame;
        frame.InitMacHeader(tests[i].fcf, tests[i].secCtl);
        printf("%d\n", frame.GetHeaderLength());
        VerifyOrQuit(frame.GetHeaderLength() == tests[i].headerLength,
                     "MacHeader test failed\n");
    }
}

}  // namespace Thread

int main(void)
{
    Thread::TestMacHeader();
    printf("All tests passed\n");
    return 0;
}
