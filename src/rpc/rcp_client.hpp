/*
 *  Copyright (c) 2021, The OpenThread Authors.
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
 *   This file includes definitions for the RCP rpc client.
 */

#ifndef RCP_CLIENT_HPP_
#define RCP_CLIENT_HPP_

#include "openthread-core-config.h"

#include <openthread/error.h>
#include <openthread/instance.h>
#include <openthread/platform/stream.h>

#include <pw_rpc/client.h>
#include <pw_rpc/internal/base_client_call.h>
#include <pw_rpc/internal/packet.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "protos/rcp.pb.h"
#include "protos/rcp.rpc.pb.h"

#if OPENTHREAD_RCP_RPC_CLIENT_ENABLE

namespace ot {
namespace Rcp {

void PrintProtoBuf(std::span<const std::byte> aProto);
void PrintProtoBuf(const uint8_t *aBuffer, uint16_t aLength);

// Client response handler for a unary RPC invocation which captures the
// response it receives.
template <typename Response> class RpcUnaryResponseHandler : public pw::rpc::UnaryResponseHandler<Response>
{
public:
    RpcUnaryResponseHandler()
        : mIsCalled(false)
    {
    }

    void ReceivedResponse(pw::Status status, const Response &response) override
    {
        mIsCalled = true;
        mStatus   = status;
        mResponse = response;

        otLogCritMac("ReceivedResponse()");
    }

    bool                      IsCalled() { return mIsCalled; }
    constexpr pw::Status      Status() const { return mStatus; }
    constexpr const Response &GetResponse() const & { return mResponse; }

private:
    bool       mIsCalled;
    pw::Status mStatus;
    Response   mResponse;
};

class RcpOutput : public pw::rpc::ChannelOutput
{
public:
    RcpOutput(const char *aName = "RcpOutput")
        : pw::rpc::ChannelOutput(aName)
    {
    }

    std::span<std::byte> AcquireBuffer() { return mBuffer; }

    pw::Status SendAndReleaseBuffer(std::span<const std::byte> aBuffer);

private:
    enum
    {
        kBufferSize = 128,
    };
    std::array<std::byte, kBufferSize> mBuffer;
};

class RadioReceiveDoneHandler : public pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioRxDoneFrame>
{
    void ReceivedResponse(const ot_rpc_RadioRxDoneFrame &response) override;
    void Complete(pw::Status) override;
    void RpcError(pw::Status) override;
};

class RadioTransmitDoneHandler : public pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioTxDoneFrame>
{
    void ReceivedResponse(const ot_rpc_RadioTxDoneFrame &aResponse) override;
    void Complete(pw::Status) override;
    void RpcError(pw::Status) override;
};

class EnergyScanDoneCallback : public pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioScanResult>
{
    void ReceivedResponse(const ot_rpc_RadioScanResult &response) override;
    void Complete(pw::Status) override;
    void RpcError(pw::Status) override;
};

class RcpStream
{
public:
    RcpStream(pw::rpc::Channel &aChannel)
        : mEnergyScanDoneHandler()
        , mRadioReceiveDoneHandler()
        , mRadioTransmitDoneHandler()
        , mRadioReceiveDoneCall(&aChannel,
                                0x880462e9, // Hash of "ot.rpc.RcpService".
                                0xa5e8b457, // Hash of "ReceiveDoneHandler"
                                mRadioReceiveDoneHandler,
                                ot_rpc_Channel_fields,
                                ot_rpc_RadioRxDoneFrame_fields)
        , mEnergyScanDoneCall(&aChannel,
                              0x880462e9, // Hash of "ot.rpc.RcpService".
                              0xe5d21df2, // Hash of "EnergyScanDoneHandler"
                              mEnergyScanDoneHandler,
                              ot_rpc_Empty_fields,
                              ot_rpc_RadioScanResult_fields)
        , mTransmitDoneCall(&aChannel,
                            0x880462e9, // Hash of "ot.rpc.RcpService".
                            0x6f615abb, // Hash of "TransmitDoneHandler"
                            mRadioTransmitDoneHandler,
                            ot_rpc_Empty_fields,
                            ot_rpc_RadioTxDoneFrame_fields)
    {
    }

    pw::rpc::NanopbClientCall<::pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioScanResult>> &EnergyScanDoneCall(
        void)
    {
        return mEnergyScanDoneCall;
    }

    pw::rpc::NanopbClientCall<::pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioRxDoneFrame>> &ReceiveDoneCall(void)
    {
        return mRadioReceiveDoneCall;
    }

    pw::rpc::NanopbClientCall<::pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioTxDoneFrame>> &TransmitDoneCall(
        void)
    {
        return mTransmitDoneCall;
    }

private:
    EnergyScanDoneCallback   mEnergyScanDoneHandler;
    RadioReceiveDoneHandler  mRadioReceiveDoneHandler;
    RadioTransmitDoneHandler mRadioTransmitDoneHandler;

    pw::rpc::NanopbClientCall<::pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioRxDoneFrame>> mRadioReceiveDoneCall;
    pw::rpc::NanopbClientCall<::pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioScanResult>>  mEnergyScanDoneCall;
    pw::rpc::NanopbClientCall<::pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioTxDoneFrame>> mTransmitDoneCall;
};

class RcpClient
{
public:
    static constexpr uint32_t kChannelId = 0x10;

    RcpClient()
        : mChannel(pw::rpc::internal::Channel::Create<kChannelId>(&mOutput))
        , mClient(std::span(&mChannel, 1))
        , mRcpStream(mChannel)
    {
        otPlatStreamEnable();
    }

    const auto &      output() const { return mOutput; }
    pw::rpc::Channel &channel() { return mChannel; }
    pw::rpc::Client & client() { return mClient; }

    pw::Status ProcessPacket(pw::ConstByteSpan data) { return mClient.ProcessPacket(data); }

    class RadioEnergyScanHandler : public pw::rpc::ServerStreamingResponseHandler<ot_rpc_RadioScanResult>
    {
        void ReceivedResponse(const ot_rpc_RadioScanResult &response) override;
        void Complete(pw::Status) override;
        void RpcError(pw::Status) override;
    };

    RcpStream &Stream(void) { return mRcpStream; }

private:
    RcpOutput        mOutput;
    pw::rpc::Channel mChannel;
    pw::rpc::Client  mClient;

    RcpStream mRcpStream;
};

} // namespace Rcp
} // namespace ot

#endif // OPENTHREAD_RCP_RPC_CLIENT_ENABLE
#endif // RCP_CLIENT_HPP_

