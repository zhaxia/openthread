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

#ifndef HDLC_HPP_
#define HDLC_HPP_

#include <common/message.hpp>
#include <common/thread_error.hpp>

namespace Thread {
namespace Hdlc {

class Encoder
{
public:
    ThreadError Init(uint8_t *aOutBuf, uint16_t &aOutLength);
    ThreadError Encode(const uint8_t *aInBuf, uint16_t aInLength, uint8_t *aOutBuf, uint16_t &aOutLength);
    ThreadError Finalize(uint8_t *aOutBuf, uint16_t &aOutLength);

private:
    ThreadError Encode(uint8_t aInByte, uint8_t *aOutBuf, uint16_t aOutLength);

    uint16_t mOutOffset;
    uint16_t mFcs;
};

class Decoder
{
public:
    typedef void (*FrameHandler)(void *aContext, uint8_t *aFrame, uint16_t aFrameLength);

    void Init(uint8_t *aOutBuf, uint16_t aOutLength, FrameHandler aFrameHandler, void *aContext);
    ThreadError Decode(const uint8_t *aInBuf, uint16_t aInLength);

private:
    enum State
    {
        kStateNoSync = 0,
        kStateSync,
        kStateEscaped,
    };
    State mState;

    FrameHandler mFrameHandler;
    void *mContext;

    uint8_t *mOutBuf;
    uint16_t mOutOffset;
    uint16_t mOutLength;

    uint16_t mFcs;
};

}  // namespace Hdlc
}  // namespace Thread

#endif  // HDLC_HPP_
