/*
 *  Copyright (c) 2018, The OpenThread Authors.
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
 *   This file includes definitions for the HDLC interface to radio (RCP).
 */

#ifndef POSIX_APP_SPINEL_INTERFACE_HPP_
#define POSIX_APP_SPINEL_INTERFACE_HPP_

#include "ncp/hdlc.hpp"

namespace ot {
namespace PosixApp {

class SpinelInterface
{
public:
    enum
    {
        kMaxFrameSize = 2048, ///< Maximum frame size (number of bytes).
    };

    /**
     * This type defines a receive frame buffer to store received (and decoded) frame(s).
     *
     * @note The receive frame buffer is an `Hdlc::MultiFrameBuffer` and therefore it is capable of storing multiple
     * frames in a FIFO queue manner.
     *
     */
    typedef Hdlc::MultiFrameBuffer<kMaxFrameSize> RxFrameBuffer;

    /**
     * This class defines the callbacks provided by `HdlcInterfac` to its owner/user.
     *
     */
    class Callbacks
    {
    public:
        /**
         * This callback is invoked to notify owner/user of `HdlcInterface` of a received (and decoded) frame.
         *
         * The newly received frame is available in `RxFrameBuffer` from `HdlcInterface::GetRxFrameBuffer()`. The
         * user can read and process the frame. The callback is expected to either discard the new frame using
         * `RxFrameBuffer::DiscardFrame()` or save the frame using `RxFrameBuffer::SaveFrame()` to be read and
         * processed later.
         *
         * @param[in] aInterface    A reference to the `SpinelInterface` object.
         *
         */
        void HandleReceivedFrame(SpinelInterface &aInterface);
    };

    /**
     * This constructor initializes the object.
     *
     * @param[in] aCallback   A reference to a `Callback` object.
     *
     */
    SpinelInterface()
        : mRxFrameBuffer()
    {
    }

    /**
     * This method gets the `RxFrameBuffer`.
     *
     * The receive frame buffer is an `Hdlc::MultiFrameBuffer` and therefore it is capable of storing multiple
     * frames in a FIFO queue manner. The `RxFrameBuffer` contains the decoded received frames.
     *
     * Wen during `Read()` the `Callbacks::HandleReceivedFrame()` is invoked, the newly received decoded frame is
     * available in the receive frame buffer. The callback is expected to either process and then discard the frame
     * (using `RxFrameBuffer::DiscardFrame()` method) or save the frame (using `RxFrameBuffer::SaveFrame()` so that
     * it can be read later.
     *
     * @returns A reference to receive frame buffer containing newly received frame or previously saved frames.
     *
     */
    RxFrameBuffer &GetRxFrameBuffer(void) { return mRxFrameBuffer; }

    virtual otError Init(otPlatformConfig *aConfig)
    {
        OT_UNUSED_VARIABLE(aConfig);
        return OT_ERROR_NOT_IMPLEMENTED;
    }

    virtual void Deinit(void) {}
    virtual bool IsDecoding(void) const { return false; }
    virtual otError SendFrame(const uint8_t *aFrame, uint16_t aLength)
    {
        OT_UNUSED_VARIABLE(aFrame);
        OT_UNUSED_VARIABLE(aLength);
        return OT_ERROR_NOT_IMPLEMENTED;
    }

    /**
     * This method waits for receiving response data within specified interval.
     *
     * @retval OT_ERROR_NONE             Response data is received.
     * @retval OT_ERROR_RESPONSE_TIMEOUT No response data is received within @p aTimeout.
     *
     */
    virtual otError WaitResponse(struct timeval &aTimeout);

    /**
     * This method updates the file descriptor sets with file descriptors used by the radio driver.
     *
     * @param[inout]  aReadFdSet   A reference to the read file descriptors.
     * @param[inout]  aWriteFdSet  A reference to the write file descriptors.
     * @param[inout]  aErrorFdSet  A reference to the error file descriptors.
     * @param[inout]  aMaxFd       A reference to the max file descriptor.
     * @param[inout]  aTimeout     A reference to the timeout.
     *
     */
    virtual void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd,
                             struct timeval &aTimeout);

    /**
     * This method performs radio driver processing.
     *
     * @param[in]   aReadFdSet      A reference to the read file descriptors.
     * @param[in]   aWriteFdSet     A reference to the write file descriptors.
     *
     */
    virtual  void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet);

private:
    RxFrameBuffer mRxFrameBuffer;
};
} // namespace PosixApp
} // namespace ot

#endif // POSIX_APP_SPINEL_INTERFACE_HPP_
