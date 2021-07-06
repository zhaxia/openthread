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
 *   This file includes definitions for the RCP rpc server.
 */

#ifndef RCP_SERVER_HPP_
#define RCP_SERVER_HPP_

#include "openthread-core-config.h"

#include <openthread/error.h>
#include <openthread/instance.h>
#include <openthread/link.h>
#include <openthread/link_raw.h>
#include <openthread/platform/stream.h>

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "common/logging.hpp"
#include "protos/rcp.pb.h"
#include "protos/rcp.rpc.pb.h"
#include "rpc/rpc_decoder.hpp"

#include <pw_assert/assert.h>
#include <pw_protobuf/decoder.h>
#include <pw_protobuf/encoder.h>
#include <pw_rpc/internal/base_server_writer.h>
#include <pw_rpc/internal/channel.h>
#include <pw_rpc/internal/method.h>
#include <pw_rpc/internal/packet.h>
#include <pw_rpc/internal/test_method.h>
#include <pw_rpc/server.h>
#include <pw_rpc/server_context.h>
#include <pw_rpc/service.h>

#if OPENTHREAD_RCP_RPC_SERVER_ENABLE
class RcpService : public ot::rpc::generated::RcpService<RcpService>
{
public:
    RcpService(otInstance *aInstance)
        : mInstance(aInstance)
    {
        OT_ASSERT(mInstance != nullptr);
    }

    pw::Status ResetRcp(ServerContext &, const ot_rpc_Empty &, ot_rpc_Empty &response)
    {
        static_cast<void>(response);
        return pw::Status::Unimplemented();
    }

    pw::Status GetRadioCaps(ServerContext &, const ot_rpc_Empty &, ot_rpc_RadioCaps &response)
    {
        response.mCaps = otLinkRawGetCaps(mInstance);
        return pw::Status();
    }

    pw::Status GetReceiveSensitivity(ServerContext &, const ot_rpc_Empty &, ot_rpc_ReceiveSensitivity &response)
    {
        response.mRssi = otPlatRadioGetReceiveSensitivity(mInstance);
        return pw::Status();
    }

    pw::Status GetEui64(ServerContext &, const ot_rpc_Empty &, ot_rpc_Eui64 &response)
    {
        otLinkGetFactoryAssignedIeeeEui64(mInstance, reinterpret_cast<otExtAddress *>(response.mEui64.bytes));
        response.mEui64.size = OT_EXT_ADDRESS_SIZE;

        otLogCritMac("RcpService::GetEui64()");

        return pw::Status();
    }

    pw::Status SetPanId(ServerContext &, const ot_rpc_PanId &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::SetPanId(0x%04x)", request.mPanId);
        response.mError = otLinkSetPanId(mInstance, static_cast<otPanId>(request.mPanId));
        return pw::Status();
    }

    pw::Status SetExtendedAddress(ServerContext &, const ot_rpc_ExtAddress &request, ot_rpc_Error &response)
    {
        otExtAddress addr;

        otLogCritMac("RcpService::SetExtendedAddress()");
        OT_ASSERT(request.mAddr.size == OT_EXT_ADDRESS_SIZE);
        memcpy(addr.m8, request.mAddr.bytes, OT_EXT_ADDRESS_SIZE);
        response.mError = otLinkSetExtendedAddress(mInstance, &addr);
        return pw::Status();
    }

    pw::Status SetShortAddress(ServerContext &, const ot_rpc_ShortAddress &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::SetShortAddress(0x%04x)", request.mAddr);
        response.mError = otLinkRawSetShortAddress(mInstance, request.mAddr);
        return pw::Status();
    }

    pw::Status SetTransmitPower(ServerContext &, const ot_rpc_TxPower &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::SetTxPower(%d)", request.mTxPower);
        response.mError = otPlatRadioSetTransmitPower(mInstance, request.mTxPower);
        return pw::Status();
    }

    pw::Status GetTransmitPower(ServerContext &, const ot_rpc_Empty &, ot_rpc_TxPower &response)
    {
        int8_t power;

        otPlatRadioGetTransmitPower(mInstance, &power);
        response.mTxPower = power;

        otLogCritMac("RcpService::GetTxPower(%d)", power);

        return pw::Status();
    }

    pw::Status SetCcaEnergyDetectThreshold(ServerContext &,
                                           const ot_rpc_CcaEnergyDetectThreshold &request,
                                           ot_rpc_Error &                         response)
    {
        otLogCritMac("RcpService::SetCcaEnergyDetectThreshold(%d)", request.mThreshold);
        response.mError = otPlatRadioSetCcaEnergyDetectThreshold(mInstance, static_cast<int8_t>(request.mThreshold));
        return pw::Status();
    }

    pw::Status GetCcaEnergyDetectThreshold(ServerContext &,
                                           const ot_rpc_Empty &                     request,
                                           ot_rpc_CcaEnergyDetectThresholdResponse &response)
    {
        OT_UNUSED_VARIABLE(request);

        int8_t threshold;

        response.mError     = otPlatRadioGetCcaEnergyDetectThreshold(mInstance, &threshold);
        response.mThreshold = threshold;
        otLogCritMac("RcpService::GetCcaEnergyDetectThreshold(%d)", threshold);

        return pw::Status();
    }

    pw::Status SetFemLnaGain(ServerContext &, const ot_rpc_FemLnaGain &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::SetFemLnaGain(%d)", request.mGain);
        response.mError = otPlatRadioSetFemLnaGain(mInstance, static_cast<int8_t>(request.mGain));
        return pw::Status();
    }

    pw::Status GetFemLnaGain(ServerContext &, const ot_rpc_Empty &request, ot_rpc_FemLnaGainResponse &response)
    {
        OT_UNUSED_VARIABLE(request);

        int8_t gain;

        response.mError = otPlatRadioGetCcaEnergyDetectThreshold(mInstance, &gain);
        response.mGain  = gain;

        otLogCritMac("RcpService::GetFemLnaGain(%u)", gain);

        return pw::Status();
    }

    pw::Status SetPromiscuous(ServerContext &, const ot_rpc_EnableVal &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::SetPromiscuous(%u)", request.mEnable);
        response.mError = otLinkRawSetPromiscuous(mInstance, request.mEnable);
        return pw::Status();
    }

    pw::Status GetPromiscuous(ServerContext &, const ot_rpc_Empty &, ot_rpc_EnableVal &response)
    {
        response.mEnable = otLinkRawGetPromiscuous(mInstance);
        otLogCritMac("RcpService::GetPromiscuous(%u)", response.mEnable);

        return pw::Status();
    }

    pw::Status SetMacKey(ServerContext &, const ot_rpc_MacKey &request, ot_rpc_Error &response)
    {
        OT_ASSERT(request.mPrevKey.size == OT_MAC_KEY_SIZE);
        OT_ASSERT(request.mCurrKey.size == OT_MAC_KEY_SIZE);
        OT_ASSERT(request.mNextKey.size == OT_MAC_KEY_SIZE);

        response.mError = otLinkRawSetMacKey(mInstance, static_cast<uint8_t>(request.mKeyIdMode),
                                             static_cast<uint8_t>(request.mKeyId),
                                             reinterpret_cast<const otMacKey *>(request.mPrevKey.bytes),
                                             reinterpret_cast<const otMacKey *>(request.mCurrKey.bytes),
                                             reinterpret_cast<const otMacKey *>(request.mNextKey.bytes));
        otLogCritMac("RcpService::SetMacKey() Error=0x%02x", response.mError);
        return pw::Status();
    }

    pw::Status SetMacFrameCounter(ServerContext &, const ot_rpc_MacFrameCounter &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::SetMacFrameCounter(%u)", request.mCounter);
        response.mError = otLinkRawSetMacFrameCounter(mInstance, request.mCounter);
        return pw::Status();
    }

    pw::Status Enable(ServerContext &, const ot_rpc_Empty &, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::Enable()");
        response.mError = otLinkRawSetReceiveDone(mInstance, LinkRawReceiveDone);
        return pw::Status();
    }

    pw::Status Disable(ServerContext &, const ot_rpc_Empty &, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::Disable()");
        response.mError = otLinkRawSetReceiveDone(mInstance, nullptr);
        return pw::Status();
    }

    pw::Status Sleep(ServerContext &, const ot_rpc_Empty &, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::Sleep()");
        response.mError = otLinkRawSleep(mInstance);
        return pw::Status();
    }

    pw::Status Receive(ServerContext &, const ot_rpc_Channel &request, ot_rpc_Error &response)
    {
        otError error = OT_ERROR_NONE;

        otLogCritMac("RcpService::Receive(%u)", request.mChannel);
        VerifyOrExit(otLinkRawIsEnabled(mInstance), error = OT_ERROR_INVALID_STATE);
        SuccessOrExit(error = otLinkSetChannel(mInstance, static_cast<uint8_t>(request.mChannel)));
        error = otLinkRawReceive(mInstance);

    exit:
        response.mError = error;
        return pw::Status();
    }

    pw::Status Transmit(ServerContext &, const ot_rpc_RadioTxFrame &request, ot_rpc_Error &response)
    {
        otError       error = OT_ERROR_NONE;
        otRadioFrame *frame;

        otLogCritMac("RcpService::Transmit(Length:%u, Channel:%u)", request.mFrame.mPsdu.size, request.mFrame.mChannel);
        VerifyOrExit(otLinkRawIsEnabled(mInstance), error = OT_ERROR_INVALID_STATE);

        frame = otLinkRawGetTransmitBuffer(mInstance);
        VerifyOrExit(frame != nullptr, error = OT_ERROR_NO_BUFS);
        VerifyOrExit(request.has_mFrame && request.has_mTxInfo, error = OT_ERROR_INVALID_ARGS);

        memcpy(frame->mPsdu, request.mFrame.mPsdu.bytes, request.mFrame.mPsdu.size);
        frame->mLength                            = static_cast<uint16_t>(request.mFrame.mPsdu.size);
        frame->mChannel                           = static_cast<uint8_t>(request.mFrame.mChannel);
        frame->mInfo.mTxInfo.mTxDelay             = request.mTxInfo.mTxDelay;
        frame->mInfo.mTxInfo.mTxDelayBaseTime     = request.mTxInfo.mTxDelayBaseTime;
        frame->mInfo.mTxInfo.mMaxCsmaBackoffs     = static_cast<uint8_t>(request.mTxInfo.mMaxCsmaBackoffs);
        frame->mInfo.mTxInfo.mMaxFrameRetries     = static_cast<uint8_t>(request.mTxInfo.mMaxFrameRetries);
        frame->mInfo.mTxInfo.mIsARetx             = request.mTxInfo.mIsARetx;
        frame->mInfo.mTxInfo.mCsmaCaEnabled       = request.mTxInfo.mCsmaCaEnabled;
        frame->mInfo.mTxInfo.mCslPresent          = request.mTxInfo.mCslPresent;
        frame->mInfo.mTxInfo.mIsSecurityProcessed = request.mTxInfo.mIsSecurityProcessed;

        error = otLinkRawTransmit(mInstance, LinkRawTransmitDone);

    exit:
        response.mError = error;
        return pw::Status();
    }

    pw::Status GetRssi(ServerContext &, const ot_rpc_Empty &, ot_rpc_Rssi &response)
    {
        response.mRssi = otLinkRawGetRssi(mInstance);
        otLogCritMac("RcpService::GetRssi(%d)", response.mRssi);
        return pw::Status();
    }

    pw::Status RadioEnergyScan(ServerContext &, const ot_rpc_RadioScanConfig &request, ot_rpc_Error &response)
    {
        otError error =
            otLinkRawEnergyScan(mInstance, request.mScanChannel, request.mScanDuration, LinkRawEnergyScanDone);

        mIsCalled = true;

        otLogCritMac("RadioEnergyScan(): mScanChannel:%u, mScanDuration:%u, open:%u", request.mScanChannel,
                     request.mScanDuration, mEnergyScanDoneWriter.open());

        response.mError = error;
        return pw::Status();
    }

    pw::Status EnableSrcMatch(ServerContext &, const ot_rpc_EnableVal &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::EnableSrcMatch()");
        response.mError = otLinkRawSrcMatchEnable(mInstance, request.mEnable);
        return pw::Status();
    }

    pw::Status AddSrcMatchShortEntry(ServerContext &, const ot_rpc_ShortAddress &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::AddSrcMatchShortEntry()");
        response.mError = otLinkRawSrcMatchAddShortEntry(mInstance, static_cast<uint16_t>(request.mAddr));
        return pw::Status();
    }

    pw::Status AddSrcMatchExtEntry(ServerContext &, const ot_rpc_ExtAddress &request, ot_rpc_Error &response)
    {
        otExtAddress addr;

        OT_ASSERT(request.mAddr.size == OT_EXT_ADDRESS_SIZE);
        memcpy(addr.m8, request.mAddr.bytes, OT_EXT_ADDRESS_SIZE);

        otLogCritMac("RcpService::AddSrcMatchExtEntry()");
        response.mError = otLinkRawSrcMatchAddExtEntry(mInstance, &addr);
        return pw::Status();
    }

    pw::Status ClearSrcMatchShortEntry(ServerContext &, const ot_rpc_ShortAddress &request, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::ClearSrcMatchShortEntry()");
        response.mError = otLinkRawSrcMatchClearShortEntry(mInstance, static_cast<uint16_t>(request.mAddr));
        return pw::Status();
    }

    pw::Status ClearSrcMatchExtEntry(ServerContext &, const ot_rpc_ExtAddress &request, ot_rpc_Error &response)
    {
        otExtAddress addr;

        OT_ASSERT(request.mAddr.size == OT_EXT_ADDRESS_SIZE);
        memcpy(addr.m8, request.mAddr.bytes, OT_EXT_ADDRESS_SIZE);

        otLogCritMac("RcpService::ClearSrcMatchExtEntry()");
        response.mError = otLinkRawSrcMatchClearExtEntry(mInstance, &addr);
        return pw::Status();
    }

    pw::Status ClearSrcMatchShortEntries(ServerContext &, const ot_rpc_Empty &, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::ClearSrcMatchShortEntries()");
        response.mError = otLinkRawSrcMatchClearShortEntries(mInstance);
        return pw::Status();
    }

    pw::Status ClearSrcMatchExtEntries(ServerContext &, const ot_rpc_Empty &, ot_rpc_Error &response)
    {
        otLogCritMac("RcpService::ClearSrcMatchExtEntries()");
        response.mError = otLinkRawSrcMatchClearExtEntries(mInstance);
        return pw::Status();
    }

    pw::Status GetSupportedChannelMask(ServerContext &, const ot_rpc_Empty &, ot_rpc_ChannelMask &response)
    {
        response.mChannelMask = otPlatRadioGetSupportedChannelMask(mInstance);
        otLogCritMac("RcpService::GetSupportedChannelMask(0x%08x)", response.mChannelMask);
        return pw::Status();
    }

    pw::Status GetPreferredChannelMask(ServerContext &, const ot_rpc_Empty &, ot_rpc_ChannelMask &response)
    {
        response.mChannelMask = otPlatRadioGetPreferredChannelMask(mInstance);
        otLogCritMac("RcpService::GetPreferredChannelMask(0x%08x)", response.mChannelMask);
        return pw::Status();
    }

    void ReceiveDoneHandler(ServerContext &, const ot_rpc_Empty &, ServerWriter<ot_rpc_RadioRxDoneFrame> &writer)
    {
        otLogCritMac("RcpService::SetReceiveDoneHandler()");
        mReceiveDoneWriter = std::move(writer);
    }

    void TransmitDoneHandler(ServerContext &, const ot_rpc_Empty &, ServerWriter<ot_rpc_RadioTxDoneFrame> &writer)
    {
        otLogCritMac("RcpService::SetTransmitDoneHandler()");
        mTransmitDoneWriter = std::move(writer);
    }

    void EnergyScanDoneHandler(ServerContext &, const ot_rpc_Empty &, ServerWriter<ot_rpc_RadioScanResult> &writer)
    {
        mEnergyScanDoneWriter = std::move(writer);
        otLogCritMac("RcpService::SetEnergyScanDoneHandler(): open:%u, mIsCalled:%u", mEnergyScanDoneWriter.open(),
                     mIsCalled);
    }

    void SendEnergyScanResponse(void)
    {
        ot_rpc_RadioScanResult frame;

        VerifyOrExit(mIsCalled);

        otLogCritMac("RcpService::SendEnergyScanResponse()");
        VerifyOrExit(mEnergyScanDoneWriter.open());

        mIsCalled = false;

        frame.mMaxRssi = -50;

        otLogCritMac("RcpService::SendEnergyScanResponse(): Write()");
        mEnergyScanDoneWriter.Write(frame);
        // mEnergyScanDoneWriter.Finish();

    exit:
        return;
    }

private:
    static void LinkRawEnergyScanDone(otInstance *aInstance, int8_t aEnergyScanMaxRssi);
    void        LinkRawEnergyScanDone(int8_t aEnergyScanMaxRssi);

    static void LinkRawReceiveDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError);
    void        LinkRawReceiveDone(otRadioFrame *aFrame, otError aError);

    static void LinkRawTransmitDone(otInstance *  aInstance,
                                    otRadioFrame *aFrame,
                                    otRadioFrame *aAckFrame,
                                    otError       aError);
    void        LinkRawTransmitDone(otRadioFrame *aFrame, otRadioFrame *aAckFrame, otError aError);
    void        EncodeRadioRxFrame(otRadioFrame *aFrame, ot_rpc_RadioRxFrame *aRpcRadioRxFrame);

    otInstance *                          mInstance;
    ServerWriter<ot_rpc_RadioRxDoneFrame> mReceiveDoneWriter;
    ServerWriter<ot_rpc_RadioTxDoneFrame> mTransmitDoneWriter;
    ServerWriter<ot_rpc_RadioScanResult>  mEnergyScanDoneWriter;
    bool                                  mIsCalled;
};

class RcpOutput : public pw::rpc::ChannelOutput
{
public:
    RcpOutput(const char *aName = "RcpOutput")
        : pw::rpc::ChannelOutput(aName)
    {
    }

    std::span<std::byte> AcquireBuffer() { return mBuffer; }

    pw::Status SendAndReleaseBuffer(std::span<const std::byte> aBuffer)
    {
        pw::Status status = PW_STATUS_OK;

        VerifyOrExit(!aBuffer.empty(), status = PW_STATUS_INVALID_ARGUMENT);
        VerifyOrExit(aBuffer.data() == mBuffer.data(), status = PW_STATUS_INVALID_ARGUMENT);

        // otLogCritMac("otPlatStreamSend(): %s",
        //             HexToString(reinterpret_cast<const uint8_t *>(aBuffer.data()), aBuffer.size_bytes()));
        if (otPlatStreamSend(reinterpret_cast<const uint8_t *>(aBuffer.data()), aBuffer.size_bytes()) != OT_ERROR_NONE)
        {
            status = PW_STATUS_INTERNAL;
        }

    exit:
        return status;
    }

private:
    enum
    {
        kBufferSize = 128,
    };

    const char *HexToString(const uint8_t *aBuf, uint16_t aLength)
    {
        static char str[1000];
        char *      start = str;
        char *      end   = str + sizeof(str);

        for (uint16_t i = 0; i < aLength; i++)
        {
            start += snprintf(start, end - start, "%02x ", aBuf[i]);
        }
        *start = '\0';

        return str;
    }

    std::array<std::byte, kBufferSize> mBuffer;
};

class RcpServer
{
public:
    RcpServer(otInstance *aInstance)
      : mChannels{
            pw::rpc::Channel::Create<1>(&mOutput),
            pw::rpc::Channel::Create<2>(&mOutput),
            pw::rpc::Channel(),  // available for assignment
        }
        ,mServer(mChannels)
        ,mService(aInstance)
    {
        sRcpServer = this;

        otPlatStreamEnable();
        mServer.RegisterService(mService);
    }

    std::span<const std::byte> EncodeRequest(pw::rpc::internal::PacketType type,
                                             uint32_t                      channel_id,
                                             uint32_t                      service_id,
                                             uint32_t                      method_id,
                                             std::span<const std::byte>    payload)
    {
        auto result =
            pw::rpc::internal::Packet(type, channel_id, service_id, method_id, payload).Encode(mRequestBuffer);
        return result.value_or(pw::ConstByteSpan());
    }

    void Test(void)
    {
        otLogCritMac("ProcessPacket()");

        mServer.ProcessPacket(
            EncodeRequest(pw::rpc::internal::PacketType::REQUEST, 1, mService.id(), 0x9fa12c2b, kDefaultPayload),
            mOutput);
    }

    void ProcessPacket(std::span<const std::byte> aPacket) { mServer.ProcessPacket(aPacket, mOutput); }

    RcpService &GetRcpService(void) { return mService; }

    static RcpServer &GetInstance(void) { return *sRcpServer; }

private:
    static constexpr std::byte kDefaultPayload[] = {std::byte(0x82), std::byte(0x02), std::byte(0xff), std::byte(0xff)};
    RcpOutput                  mOutput;
    std::array<pw::rpc::Channel, 3> mChannels;

    pw::rpc::Server mServer;
    RcpService      mService;
    std::byte       mRequestBuffer[64];

    static RcpServer *sRcpServer;
};
#endif // OPENTHREAD_RCP_RPC_SERVER_ENABLE
#endif // RCP_SERVER_HPP_
