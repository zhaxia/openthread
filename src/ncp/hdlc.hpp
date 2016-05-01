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
 *   This file includes definitions for an HDLC encoder and decoer.
 */

#ifndef HDLC_HPP_
#define HDLC_HPP_

#include <common/message.hpp>
#include <common/thread_error.hpp>

namespace Thread {

/**
 * @namespace Thread::Hdlc
 *
 * @brief
 *   This namespace includes definitions for the HDLC encoder and decoder.
 *
 */
namespace Hdlc {

/**
 * This class implements the HDLC encoder.
 *
 */
class Encoder
{
public:
    /**
     * This method begins an HDLC frame and puts the initial bytes into @p aOutBuf.
     *
     * @param[in]     aOutBuf     A pointer to the output buffer.
     * @param[inout]  aOutLength  On entry, the output buffer size; On exit, the output length.
     *
     * @retval kThreadError_None    Successfully started the HDLC frame.
     * @retval kThreadError_NoBufs  Insufficient buffer space available to start the HDLC frame.
     *
     */
    ThreadError Init(uint8_t *aOutBuf, uint16_t &aOutLength);

    /**
     * This method encodes the frame.
     *
     * @param[in]   aInBuf      A pointer to the input buffer.
     * @param[in]   aInLength   The number of bytes in @p aInBuf to encode.
     * @param[out]  aOutBuf     A pointer to the output buffer.
     * @param[out]  aOutLength  On exit, the number of bytes placed in @p aOutBuf.
     *
     * @retval kThreadError_None    Successfully encoded the HDLC frame.
     * @retval kThreadError_NoBufs  Insufficient buffer space available to encode the HDLC frame.
     *
     */
    ThreadError Encode(const uint8_t *aInBuf, uint16_t aInLength, uint8_t *aOutBuf, uint16_t &aOutLength);

    /**
     * This method ends an HDLC frame and puts the initial bytes into @p aOutBuf.
     *
     * @param[in]     aOutBuf     A pointer to the output buffer.
     * @param[inout]  aOutLength  On entry, the output buffer size; On exit, the output length.
     *
     * @retval kThreadError_None    Successfully ended the HDLC frame.
     * @retval kThreadError_NoBufs  Insufficient buffer space available to end the HDLC frame.
     *
     */
    ThreadError Finalize(uint8_t *aOutBuf, uint16_t &aOutLength);

private:
    ThreadError Encode(uint8_t aInByte, uint8_t *aOutBuf, uint16_t aOutLength);

    uint16_t mOutOffset;
    uint16_t mFcs;
};

/**
 * This class implements the HDLC decoder.
 *
 */
class Decoder
{
public:
    /**
     * This function pointer is called when a complete frame has been formed.
     *
     * @param[in]  aContext      A pointer to arbitrary context information.
     * @param[in]  aFrame        A pointer to the frame.
     * @param[in]  aFrameLength  The frame length in bytes.
     *
     */
    typedef void (*FrameHandler)(void *aContext, uint8_t *aFrame, uint16_t aFrameLength);

    /**
     * This method initializes the decoder.
     *
     * @param[in]  aOutBuf        A pointer to the output buffer.
     * @param[in]  aOutLength     Size of the output buffer in bytes.
     * @param[in]  aFrameHandler  A pointer to a function that is called when a complete frame is received.
     * @param[in]  aContext       A pointer to arbitrary context information.
     *
     */
    void Init(uint8_t *aOutBuf, uint16_t aOutLength, FrameHandler aFrameHandler, void *aContext);

    /**
     * This method streams bytes into the decoder.
     *
     * @param[in]  aInBuf     A pointer to the input buffer.
     * @param[in]  aInLength  The number of bytes in @p aInBuf.
     *
     */
    void Decode(const uint8_t *aInBuf, uint16_t aInLength);

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
