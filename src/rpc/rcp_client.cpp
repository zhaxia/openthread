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

#include <openthread/ncp.h>
#include <openthread/rpc.h>
#include <openthread/platform/diag.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/uart.h>

#include "common/code_utils.hpp"
#include "common/debug.hpp"
#include "common/logging.hpp"
#include "mac/mac_frame.hpp"
#include "pw_protobuf/decoder.h"
#include "pw_protobuf/wire_format.h"
#include "pw_rpc/nanopb_client_call.h"
#include "rpc/rcp_client.hpp"
#include "rpc/rpc_decoder.hpp"

#if OPENTHREAD_RCP_RPC_CLIENT_ENABLE

static ot::Rcp::RcpClient sRcpClient;
static otInstance *       sInstance = nullptr;

uint8_t      mTxPsdu[OT_RADIO_FRAME_MAX_SIZE];
otRadioFrame mTxRadioFrame;

otError PlatStreamSend(const uint8_t *aBuf, uint16_t aBufLength);

static void LogBytes(const char *aName, const uint8_t *aBuf, uint16_t aLength)
{
    static char str[1000];
    char *      start = str;
    char *      end   = str + sizeof(str);

    for (uint16_t i = 0; i < aLength; i++)
    {
        start += snprintf(start, end - start, "%02x ", aBuf[i]);
    }
    *start = '\0';

    otLogCritMac("%s: %s", aName, str);
}

namespace ot {
namespace Rcp {

pw::Status RcpOutput::SendAndReleaseBuffer(std::span<const std::byte> aBuffer)
{
    pw::Status status = PW_STATUS_OK;

    VerifyOrExit(!aBuffer.empty(), status = PW_STATUS_INVALID_ARGUMENT);
    VerifyOrExit(aBuffer.data() == mBuffer.data(), status = PW_STATUS_INVALID_ARGUMENT);

    if (PlatStreamSend(reinterpret_cast<const uint8_t *>(aBuffer.data()), aBuffer.size_bytes()) != OT_ERROR_NONE)
    {
        status = PW_STATUS_INTERNAL;
    }

exit:
    return status;
}

void EnergyScanDoneCallback::ReceivedResponse(const ot_rpc_RadioScanResult &response)
{
    sRcpClient.Stream().EnergyScanDoneCall().Cancel();

    otPlatRadioEnergyScanDone(sInstance, static_cast<int8_t>(response.mMaxRssi));

    otLogCritMac("EnergyScanDoneCallback:ReceivedResponse: MaxRssi:%d", response.mMaxRssi);
}

void EnergyScanDoneCallback::Complete(pw::Status)
{
    otLogCritMac("EnergyScanDoneCallback:Complete");
}

void EnergyScanDoneCallback::RpcError(pw::Status)
{
    otLogCritMac("EnergyScanDoneCallback:RpcError");
}

void DecodeRpcRxFrame(const ot_rpc_RadioRxFrame &aRpcRadioRxFrame, otRadioFrame &aFrame)
{
    memcpy(aFrame.mPsdu, aRpcRadioRxFrame.mFrame.mPsdu.bytes, aRpcRadioRxFrame.mFrame.mPsdu.size);
    aFrame.mLength  = aRpcRadioRxFrame.mFrame.mPsdu.size;
    aFrame.mChannel = aRpcRadioRxFrame.mFrame.mChannel;

    aFrame.mInfo.mRxInfo.mTimestamp             = aRpcRadioRxFrame.mRxInfo.mTimestamp;
    aFrame.mInfo.mRxInfo.mAckFrameCounter       = aRpcRadioRxFrame.mRxInfo.mAckFrameCounter;
    aFrame.mInfo.mRxInfo.mAckKeyId              = aRpcRadioRxFrame.mRxInfo.mAckKeyId;
    aFrame.mInfo.mRxInfo.mRssi                  = aRpcRadioRxFrame.mRxInfo.mRssi;
    aFrame.mInfo.mRxInfo.mLqi                   = aRpcRadioRxFrame.mRxInfo.mLqi;
    aFrame.mInfo.mRxInfo.mAckedWithFramePending = aRpcRadioRxFrame.mRxInfo.mAckedWithFramePending;
    aFrame.mInfo.mRxInfo.mAckedWithSecEnhAck    = aRpcRadioRxFrame.mRxInfo.mAckedWithSecEnhAck;
}

void RadioReceiveDoneHandler::ReceivedResponse(const ot_rpc_RadioRxDoneFrame &response)
{
    otRadioFrame frame;

    otLogCritMac("RadioReceiveDoneHandler:ReceivedResponse: Error:%u", response.mError);

    if (response.mError == OT_ERROR_NONE)
    {
        DecodeRpcRxFrame(response.mFrame, frame);
    }

    otPlatRadioReceiveDone(sInstance, &frame, static_cast<otError>(response.mError));
}

void RadioReceiveDoneHandler::Complete(pw::Status)
{
    otLogCritMac("RadioReceiveDoneHandler:Complete");
}

void RadioReceiveDoneHandler::RpcError(pw::Status)
{
    otLogCritMac("RadioReceiveDoneHandler:RpcError");
}

void RadioTransmitDoneHandler::ReceivedResponse(const ot_rpc_RadioTxDoneFrame &aResponse)
{
    otLogCritMac("RadioTransmitDoneHandler:ReceivedResponse: Error:%u, ", aResponse.mError);

    otRadioFrame  ackFrame;
    otRadioFrame *ack = nullptr;

    VerifyOrExit(aResponse.mError == OT_ERROR_NONE);

    if (aResponse.has_mAck)
    {
        DecodeRpcRxFrame(aResponse.mAck, ackFrame);
        ack = &ackFrame;
    }

    if (static_cast<ot::Mac::TxFrame *>(&mTxRadioFrame)->GetSecurityEnabled())
    {
        if (aResponse.has_mKeyId)
        {
            static_cast<ot::Mac::TxFrame *>(&mTxRadioFrame)->SetKeyId(aResponse.mKeyId);
        }

        if (aResponse.has_mFrameCounter)
        {
            static_cast<ot::Mac::TxFrame *>(&mTxRadioFrame)->SetFrameCounter(aResponse.mFrameCounter);
        }
    }

exit:
    otPlatRadioTxDone(sInstance, &mTxRadioFrame, ack, static_cast<otError>(aResponse.mError));
    return;
}

void RadioTransmitDoneHandler::Complete(pw::Status)
{
    otLogCritMac("RadioTransmitDoneHandler:Complete");
}

void RadioTransmitDoneHandler::RpcError(pw::Status)
{
    otLogCritMac("RadioTransmitDoneHandler:RpcError");
}

} // namespace Rcp
} // namespace ot

//==========================================

#include "pw_hdlc/decoder.h"
#include "pw_hdlc/encoder.h"
#include "rpc/rpc_decoder.hpp"
pw::hdlc::DecoderBuffer<1024> sHdlcDecoder;

void otPlatStreamReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    pw::Result<pw::hdlc::Frame> result = pw::Status::Unknown();
    otLogCritMac("otPlatStreamReceived");

    LogBytes("HdlcReceived", aBuf, aBufLength);

    for (uint16_t i = 0; i < aBufLength; i++)
    {
        result = sHdlcDecoder.Process(std::byte(aBuf[i]));
        if (result.status() == pw::OkStatus())
        {
            LogBytes("RpcReceived", reinterpret_cast<const uint8_t *>(result.value().data().data()),
                     result.value().data().size_bytes());

            otLogCritMac("RpcReceived: ParseFrame");
            PrintRpcPayload(reinterpret_cast<const uint8_t *>(result.value().data().data()),
                            result.value().data().size_bytes());
            sRcpClient.ProcessPacket(result.value().data());

            sHdlcDecoder.Clear();
        }
    }
}

#include "pw_bytes/array.h"
#include "pw_stream/memory_stream.h"
std::array<std::byte, 1024> sTxBuffer;
constexpr uint8_t           kAddress = 0x7B; // 123

otError PlatStreamSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    otError                  error = OT_ERROR_NONE;
    pw::stream::MemoryWriter writer(sTxBuffer);

    LogBytes("RpcSend", aBuf, aBufLength);
    otLogCritMac("RpcSend: ParseFrame");
    PrintRpcPayload(aBuf, aBufLength);

    VerifyOrExit(pw::hdlc::WriteUIFrame(
                     kAddress, std::span<const std::byte>(reinterpret_cast<const std::byte *>(aBuf), aBufLength),
                     writer) == pw::OkStatus(),
                 error = OT_ERROR_FAILED);

    LogBytes("HdlcSend ", reinterpret_cast<const uint8_t *>(writer.data()),
             static_cast<uint16_t>(writer.bytes_written()));
    error = otPlatStreamSend(reinterpret_cast<const uint8_t *>(writer.data()),
                             static_cast<uint16_t>(writer.bytes_written()));

exit:
    return error;
}

//==========================================

template <typename T> otError WaitResponse(ot::Rcp::RpcUnaryResponseHandler<T> &aHandler)
{
    pw::Result<pw::hdlc::Frame> result = pw::Status::Unknown();
    otError                     error;
    uint64_t                    timeoutUs = 10000000;
    uint8_t                     buf[1024];
    uint16_t                    length = sizeof(buf);

    while (otPlatStreamBlockingRead(buf, &length, &timeoutUs) == OT_ERROR_NONE)
    {
        for (uint16_t i = 0; i < length; i++)
        {
            result = sHdlcDecoder.Process(std::byte(buf[i]));
            if (result.status() == pw::OkStatus())
            {
                LogBytes("RpcReceived", reinterpret_cast<const uint8_t *>(result.value().data().data()),
                         result.value().data().size_bytes());

                otLogCritMac("RpcReceived: ParseFrame");
                PrintRpcPayload(reinterpret_cast<const uint8_t *>(result.value().data().data()),
                                result.value().data().size_bytes());

                sRcpClient.ProcessPacket(result.value().data());
                if (aHandler.IsCalled())
                {
                    ExitNow(error = OT_ERROR_NONE);
                }

                sHdlcDecoder.Clear();
            }
        }
    }

    error = OT_ERROR_RESPONSE_TIMEOUT;

exit:
    return error;
}

template <typename Request, typename Response> Response SendAndWaitResponse(Request &aRequest)
{
    ot::Rcp::RpcUnaryResponseHandler<Response>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<Response>> call =
        ot::rpc::nanopb::RcpServiceClient::GetEui64(sRcpClient.channel(), aRequest, handler);

    OT_ASSERT(WaitResponse<Response>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    return handler.GetResponse();
}

void otPlatRadioGetIeeeEui64(otInstance *aInstance, uint8_t *aIeeeEui64)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};
    ot_rpc_Eui64 eui64;

    otLogCritMac("otPlatRadioGetIeeeEui64()");
    eui64 = SendAndWaitResponse<ot_rpc_Empty, ot_rpc_Eui64>(empty);
    OT_ASSERT(eui64.mEui64.size == OT_EXT_ADDRESS_SIZE);

    memcpy(aIeeeEui64, eui64.mEui64.bytes, OT_EXT_ADDRESS_SIZE);

    otLogCritMac("otPlatRadioGetIeeeEui64() Done");
}

uint32_t otPlatRadioGetSupportedChannelMask(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetSupportedChannelMask()");
    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_ChannelMask>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_ChannelMask>> call =
        ot::rpc::nanopb::RcpServiceClient::GetSupportedChannelMask(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_ChannelMask>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    otLogCritMac("otPlatRadioGetSupportedChannelMask(0x%08x) Done", handler.GetResponse().mChannelMask);

    return handler.GetResponse().mChannelMask;
}

otError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
{
    otLogCritMac("otPlatRadioEnergyScan()");

    sInstance = aInstance;

    ot_rpc_Empty           request = {.dummy_field = 0};
    ot_rpc_RadioScanConfig config  = {.mScanChannel = aScanChannel, .mScanDuration = aScanDuration};
    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::RadioEnergyScan(sRcpClient.channel(), config, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    if (static_cast<otError>(handler.GetResponse().mError) == OT_ERROR_NONE)
    {
        sRcpClient.Stream().EnergyScanDoneCall().SendRequest(&request);
    }

    otLogCritMac("otPlatRadioEnergyScan() Done");

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioReceive(otInstance *aInstance, uint8_t aChannel)
{
    OT_UNUSED_VARIABLE(aInstance);

    otLogCritMac("otPlatRadioReceive()");

    ot_rpc_Empty                                                             empty   = {.dummy_field = 0};
    ot_rpc_Channel                                                           request = {.mChannel = aChannel};
    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::Receive(sRcpClient.channel(), request, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    if (static_cast<otError>(handler.GetResponse().mError) == OT_ERROR_NONE)
    {
        sRcpClient.Stream().ReceiveDoneCall().SendRequest(&empty);
    }

    sInstance = aInstance;

    otLogCritMac("otPlatRadioReceive() Done");

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioCliCommand(otInstance *aInstance, uint8_t aArgsLength, char *aArgs[])
{
    (void)aInstance;
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aArgsLength >= 2, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(strcmp(aArgs[0], "client") == 0, error = OT_ERROR_INVALID_ARGS);

    if (strcmp(aArgs[1], "eui64") == 0)
    {
        uint8_t m8[8];
        otPlatRadioGetIeeeEui64(NULL, m8);
        otLogCritMac("otPlatRadioGetIeeeEui64: %02x%02x%02x%02x%02x%02x%02x%02x", m8[0], m8[1], m8[2], m8[3], m8[4],
                     m8[5], m8[6], m8[7]);
    }
    else if (strcmp(aArgs[1], "receive") == 0)
    {
        otPlatRadioReceive(NULL, 15);
    }
    else if (strcmp(aArgs[1], "send") == 0)
    {
        otRadioFrame *frame = otPlatRadioGetTransmitBuffer(NULL);

        for (uint16_t i = 0; i < 20; i++)
        {
            frame->mPsdu[i] = i;
        }

        frame->mLength                            = 20;
        frame->mChannel                           = 20;
        frame->mInfo.mTxInfo.mTxDelay             = 0;
        frame->mInfo.mTxInfo.mTxDelayBaseTime     = 0;
        frame->mInfo.mTxInfo.mMaxCsmaBackoffs     = 0;
        frame->mInfo.mTxInfo.mMaxFrameRetries     = 0;
        frame->mInfo.mTxInfo.mIsARetx             = 0;
        frame->mInfo.mTxInfo.mCsmaCaEnabled       = 0;
        frame->mInfo.mTxInfo.mCslPresent          = 0;
        frame->mInfo.mTxInfo.mIsSecurityProcessed = 0;

        otPlatRadioTransmit(NULL, frame);
    }
    else if (strcmp(aArgs[1], "scan") == 0)
    {
        otPlatRadioEnergyScan(NULL, 15, 1000);
    }
    else
    {
        error = OT_ERROR_INVALID_ARGS;
    }

exit:
    return error;
}

//====================================================================================
otRadioCaps otPlatRadioGetCaps(otInstance *aInstance)
{
    const otRadioCaps kRadioCaps = OT_RADIO_CAPS_TRANSMIT_SEC | OT_RADIO_CAPS_TRANSMIT_TIMING |
                                   OT_RADIO_CAPS_ACK_TIMEOUT | OT_RADIO_CAPS_TRANSMIT_RETRIES |
                                   OT_RADIO_CAPS_CSMA_BACKOFF;

    OT_UNUSED_VARIABLE(aInstance);
    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetCaps()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_RadioCaps>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_RadioCaps>> call =
        ot::rpc::nanopb::RcpServiceClient::GetRadioCaps(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_RadioCaps>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetCaps(%d) Done", handler.GetResponse().mCaps);

    return kRadioCaps | handler.GetResponse().mCaps;
}

const char *otPlatRadioGetVersionString(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otLogCritMac("otPlatRadioGetVersionString()");

    return otGetVersionString();
}

int8_t otPlatRadioGetReceiveSensitivity(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetReceiveSensitivity()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_ReceiveSensitivity>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_ReceiveSensitivity>> call =
        ot::rpc::nanopb::RcpServiceClient::GetReceiveSensitivity(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_ReceiveSensitivity>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetReceiveSensitivity(%d) Done", handler.GetResponse().mRssi);

    return handler.GetResponse().mRssi;
}

void otPlatRadioSetPanId(otInstance *aInstance, otPanId aPanId)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aPanId);
    ot_rpc_PanId panId = {.mPanId = aPanId};

    otLogCritMac("otPlatRadioSetPanId()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetPanId(sRcpClient.channel(), panId, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioSetPanId() Done, Error=%d", handler.GetResponse().mError);
}

void otPlatRadioSetExtendedAddress(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aExtAddress);

    ot_rpc_ExtAddress extAddr;

    otLogCritMac("otPlatRadioSetExtendedAddress()");

    memcpy(extAddr.mAddr.bytes, aExtAddress->m8, OT_EXT_ADDRESS_SIZE);
    extAddr.mAddr.size = OT_EXT_ADDRESS_SIZE;

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetExtendedAddress(sRcpClient.channel(), extAddr, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioSetExtendedAddress() Done, Error=%d", handler.GetResponse().mError);
}

void otPlatRadioSetShortAddress(otInstance *aInstance, otShortAddress aShortAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aShortAddress);
    ot_rpc_ShortAddress shortAddr = {.mAddr = aShortAddress};

    otLogCritMac("otPlatRadioSetShortAddress()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetShortAddress(sRcpClient.channel(), shortAddr, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioSetShortAddress() Done, Error=%d", handler.GetResponse().mError);
}

otError otPlatRadioSetTransmitPower(otInstance *aInstance, int8_t aPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aPower);

    ot_rpc_TxPower shortAddr = {.mTxPower = aPower};

    otLogCritMac("otPlatRadioSetTransmitPower()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetTransmitPower(sRcpClient.channel(), shortAddr, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioSetTransmitPower() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioGetTransmitPower(otInstance *aInstance, int8_t *aPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aPower);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetTransmitPower()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_TxPower>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_TxPower>> call =
        ot::rpc::nanopb::RcpServiceClient::GetTransmitPower(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_TxPower>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetTransmitPower(%d) Done", handler.GetResponse().mTxPower);

    return OT_ERROR_NONE;
}

otError otPlatRadioSetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aThreshold);

    ot_rpc_CcaEnergyDetectThreshold threshold = {.mThreshold = aThreshold};

    otLogCritMac("otPlatRadioSetCcaEnergyDetectThreshold()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetCcaEnergyDetectThreshold(sRcpClient.channel(), threshold, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioSetCcaEnergyDetectThreshold() Done");

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioGetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t *aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aThreshold);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetCcaEnergyDetectThreshold()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_CcaEnergyDetectThresholdResponse>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_CcaEnergyDetectThresholdResponse>> call =
        ot::rpc::nanopb::RcpServiceClient::GetCcaEnergyDetectThreshold(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_CcaEnergyDetectThresholdResponse>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetCcaEnergyDetectThreshold(%d) Done", handler.GetResponse().mThreshold);

    *aThreshold = static_cast<int8_t>(handler.GetResponse().mThreshold);

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioSetFemLnaGain(otInstance *aInstance, int8_t aGain)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aGain);

    ot_rpc_FemLnaGain gain = {.mGain = aGain};

    otLogCritMac("otPlatRadioSetFemLnaGain()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetFemLnaGain(sRcpClient.channel(), gain, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioSetFemLnaGain() Done");

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioGetFemLnaGain(otInstance *aInstance, int8_t *aGain)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aGain);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetFemLnaGain()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_FemLnaGainResponse>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_FemLnaGainResponse>> call =
        ot::rpc::nanopb::RcpServiceClient::GetFemLnaGain(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_FemLnaGainResponse>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetFemLnaGain(%d) Done", handler.GetResponse().mGain);

    *aGain = static_cast<int8_t>(handler.GetResponse().mGain);

    return static_cast<otError>(handler.GetResponse().mError);
}

bool otPlatRadioGetPromiscuous(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetPromiscuous()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_EnableVal>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_EnableVal>> call =
        ot::rpc::nanopb::RcpServiceClient::GetPromiscuous(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_EnableVal>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetPromiscuous(%d) Done", handler.GetResponse().mEnable);

    return handler.GetResponse().mEnable;
}

void otPlatRadioSetPromiscuous(otInstance *aInstance, bool aEnable)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aEnable);

    ot_rpc_EnableVal enable = {.mEnable = aEnable};

    otLogCritMac("otPlatRadioSetPromiscuous()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetPromiscuous(sRcpClient.channel(), enable, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioSetPromiscuous() Done");
}

//---

void otPlatRadioSetMacKey(otInstance *    aInstance,
                          uint8_t         aKeyIdMode,
                          uint8_t         aKeyId,
                          const otMacKey *aPrevKey,
                          const otMacKey *aCurrKey,
                          const otMacKey *aNextKey)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aKeyIdMode);
    OT_UNUSED_VARIABLE(aKeyId);
    OT_UNUSED_VARIABLE(aPrevKey);
    OT_UNUSED_VARIABLE(aCurrKey);
    OT_UNUSED_VARIABLE(aNextKey);

    ot_rpc_MacKey macKey;
    otLogCritMac("otPlatRadioSetMacKey()");

    macKey.mKeyIdMode = aKeyIdMode;
    macKey.mKeyId     = aKeyId;

    macKey.mPrevKey.size = OT_MAC_KEY_SIZE;
    memcpy(macKey.mPrevKey.bytes, aPrevKey, OT_MAC_KEY_SIZE);

    macKey.mCurrKey.size = OT_MAC_KEY_SIZE;
    memcpy(macKey.mCurrKey.bytes, aCurrKey, OT_MAC_KEY_SIZE);

    macKey.mNextKey.size = OT_MAC_KEY_SIZE;
    memcpy(macKey.mNextKey.bytes, aNextKey, OT_MAC_KEY_SIZE);

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetMacKey(sRcpClient.channel(), macKey, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);
    otLogCritMac("otPlatRadioSetMacKey() Done");
}

void otPlatRadioSetMacFrameCounter(otInstance *aInstance, uint32_t aMacFrameCounter)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aMacFrameCounter);

    ot_rpc_MacFrameCounter counter = {.mCounter = aMacFrameCounter};

    otLogCritMac("otPlatRadioSetMacFrameCounter()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::SetMacFrameCounter(sRcpClient.channel(), counter, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioSetMacFrameCounter() Done");
}

// TODO
uint64_t otPlatRadioGetNow(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otLogCritMac("otPlatRadioGetNow()");
    return 0;
}

// TODO
uint32_t otPlatRadioGetBusSpeed(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otLogCritMac("otPlatRadioGetBusSpeed()");

    return 115200;
}

// TODO
otRadioState otPlatRadioGetState(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otLogCritMac("otPlatRadioGetState()");

    return OT_RADIO_STATE_DISABLED;
}

otError otPlatRadioEnable(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};

    sInstance = aInstance;

    otLogCritMac("otPlatRadioEnable()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::Enable(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioEnable() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioDisable(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioDisable()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::Enable(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioDisable() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

// TODO
bool otPlatRadioIsEnabled(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otLogCritMac("otPlatRadioIsEnabled()");
    return false;
}

otError otPlatRadioSleep(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioSleep()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::Sleep(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioSleep() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

// TODO
otRadioFrame *otPlatRadioGetTransmitBuffer(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otLogCritMac("otPlatRadioGetTransmitBuffer()");

    mTxRadioFrame.mPsdu = mTxPsdu;

    return &mTxRadioFrame;
}

otError otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aFrame);

    ot_rpc_Empty        empty = {.dummy_field = 0};
    ot_rpc_RadioTxFrame frame;

    memset(&frame, 0, sizeof(frame));

    otLogCritMac("otPlatRadioTransmit()  mLength=%u", aFrame->mLength);

    memcpy(frame.mFrame.mPsdu.bytes, aFrame->mPsdu, aFrame->mLength);

    frame.mFrame.mPsdu.size            = aFrame->mLength;
    frame.mFrame.mChannel              = aFrame->mChannel;
    frame.mTxInfo.mTxDelay             = aFrame->mInfo.mTxInfo.mTxDelay;
    frame.mTxInfo.mTxDelayBaseTime     = aFrame->mInfo.mTxInfo.mTxDelayBaseTime;
    frame.mTxInfo.mMaxCsmaBackoffs     = aFrame->mInfo.mTxInfo.mMaxCsmaBackoffs;
    frame.mTxInfo.mMaxFrameRetries     = aFrame->mInfo.mTxInfo.mMaxFrameRetries;
    frame.mTxInfo.mIsARetx             = aFrame->mInfo.mTxInfo.mIsARetx;
    frame.mTxInfo.mCsmaCaEnabled       = aFrame->mInfo.mTxInfo.mCsmaCaEnabled;
    frame.mTxInfo.mCslPresent          = aFrame->mInfo.mTxInfo.mCslPresent;
    frame.mTxInfo.mIsSecurityProcessed = aFrame->mInfo.mTxInfo.mIsSecurityProcessed;

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::Transmit(sRcpClient.channel(), frame, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    if (static_cast<otError>(handler.GetResponse().mError) == OT_ERROR_NONE)
    {
        sRcpClient.Stream().TransmitDoneCall().SendRequest(&empty);
    }

    otLogCritMac("otPlatRadioTransmit() Done");

    return static_cast<otError>(handler.GetResponse().mError);
}

int8_t otPlatRadioGetRssi(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetRssi()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Rssi>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Rssi>> call =
        ot::rpc::nanopb::RcpServiceClient::GetRssi(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Rssi>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetRssi(%d) Done", handler.GetResponse().mRssi);

    return static_cast<int8_t>(handler.GetResponse().mRssi);
}

void otPlatRadioEnableSrcMatch(otInstance *aInstance, bool aEnable)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aEnable);

    ot_rpc_EnableVal enable = {.mEnable = aEnable};

    otLogCritMac("otPlatRadioEnableSrcMatch()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::EnableSrcMatch(sRcpClient.channel(), enable, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioEnableSrcMatch() Done");
}

otError otPlatRadioAddSrcMatchShortEntry(otInstance *aInstance, otShortAddress aShortAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aShortAddress);

    ot_rpc_ShortAddress shortAddr = {.mAddr = aShortAddress};

    otLogCritMac("otPlatRadioAddSrcMatchShortEntry()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::AddSrcMatchShortEntry(sRcpClient.channel(), shortAddr, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioAddSrcMatchShortEntry() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioAddSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aExtAddress);

    ot_rpc_ExtAddress extAddr;

    otLogCritMac("otPlatRadioAddSrcMatchExtEntry()");

    memcpy(extAddr.mAddr.bytes, aExtAddress->m8, OT_EXT_ADDRESS_SIZE);
    extAddr.mAddr.size = OT_EXT_ADDRESS_SIZE;

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::AddSrcMatchExtEntry(sRcpClient.channel(), extAddr, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioAddSrcMatchExtEntry() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioClearSrcMatchShortEntry(otInstance *aInstance, otShortAddress aShortAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aShortAddress);

    ot_rpc_ShortAddress shortAddr = {.mAddr = aShortAddress};

    otLogCritMac("otPlatRadioClearSrcMatchShortEntry()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::ClearSrcMatchShortEntry(sRcpClient.channel(), shortAddr, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioClearSrcMatchShortEntry() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

otError otPlatRadioClearSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aExtAddress);

    ot_rpc_ExtAddress extAddr;

    otLogCritMac("otPlatRadioClearSrcMatchExtEntry()");

    memcpy(extAddr.mAddr.bytes, aExtAddress->m8, OT_EXT_ADDRESS_SIZE);
    extAddr.mAddr.size = OT_EXT_ADDRESS_SIZE;

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::ClearSrcMatchExtEntry(sRcpClient.channel(), extAddr, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioClearSrcMatchExtEntry() Done, Error=%d", handler.GetResponse().mError);

    return static_cast<otError>(handler.GetResponse().mError);
}

void otPlatRadioClearSrcMatchShortEntries(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioClearSrcMatchShortEntries()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::ClearSrcMatchShortEntries(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioClearSrcMatchShortEntries() Done");
}

void otPlatRadioClearSrcMatchExtEntries(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioClearSrcMatchExtEntries()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_Error>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_Error>> call =
        ot::rpc::nanopb::RcpServiceClient::ClearSrcMatchExtEntries(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_Error>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());
    OT_ASSERT(handler.GetResponse().mError == OT_ERROR_NONE);

    otLogCritMac("otPlatRadioClearSrcMatchExtEntries() Done");
}

uint32_t otPlatRadioGetPreferredChannelMask(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    ot_rpc_Empty empty = {.dummy_field = 0};

    otLogCritMac("otPlatRadioGetPreferredChannelMask()");

    ot::Rcp::RpcUnaryResponseHandler<ot_rpc_ChannelMask>                           handler;
    pw::rpc::NanopbClientCall<::pw::rpc::UnaryResponseHandler<ot_rpc_ChannelMask>> call =
        ot::rpc::nanopb::RcpServiceClient::GetPreferredChannelMask(sRcpClient.channel(), empty, handler);

    OT_ASSERT(WaitResponse<ot_rpc_ChannelMask>(handler) == OT_ERROR_NONE);
    OT_ASSERT(handler.Status().ok());

    otLogCritMac("otPlatRadioGetPreferredChannelMask(0x%08x) Done", handler.GetResponse().mChannelMask);

    return handler.GetResponse().mChannelMask;
    // return 0x7fff800;
}

//---
otError otPlatRadioSetCoexEnabled(otInstance *aInstance, bool aEnabled)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aEnabled);
    otLogCritMac("otPlatRadioSetCoexEnabled()");
    return OT_ERROR_NONE;
}

bool otPlatRadioIsCoexEnabled(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otLogCritMac("otPlatRadioIsCoexEnabled()");

    return false;
}

otError otPlatRadioGetCoexMetrics(otInstance *aInstance, otRadioCoexMetrics *aCoexMetrics)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aCoexMetrics);
    otLogCritMac("otPlatRadioGetCoexMetrics()");
    return OT_ERROR_NONE;
}

otError otPlatRadioEnableCsl(otInstance *aInstance, uint32_t aCslPeriod, const otExtAddress *aExtAddr)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aCslPeriod);
    OT_UNUSED_VARIABLE(aExtAddr);
    otLogCritMac("otPlatRadioEnableCsl()");

    return OT_ERROR_NONE;
}

void otPlatRadioUpdateCslSampleTime(otInstance *aInstance, uint32_t aCslSampleTime)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aCslSampleTime);
    otLogCritMac("otPlatRadioUpdateCslSampleTime()");
}

otError otPlatRadioSetChannelMaxTransmitPower(otInstance *aInstance, uint8_t aChannel, int8_t aMaxPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aChannel);
    OT_UNUSED_VARIABLE(aMaxPower);
    otLogCritMac("otPlatRadioSetChannelMaxTransmitPower()");

    return OT_ERROR_NONE;
}

otError otPlatRadioConfigureEnhAckProbing(otInstance *        aInstance,
                                          otLinkMetrics       aLinkMetrics,
                                          otShortAddress      aShortAddress,
                                          const otExtAddress *aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aLinkMetrics);
    OT_UNUSED_VARIABLE(aShortAddress);
    OT_UNUSED_VARIABLE(aExtAddress);

    otLogCritMac("otPlatRadioConfigureEnhAckProbing()");
    return OT_ERROR_NONE;
}

#if 0
extern void otPlatRadioReceiveDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError) {}
extern void otPlatDiagRadioReceiveDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError) {}

extern void otPlatRadioTxStarted(otInstance *aInstance, otRadioFrame *aFrame){}

otError otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame)
extern void otPlatRadioTxDone(otInstance *aInstance, otRadioFrame *aFrame, otRadioFrame *aAckFrame, otError aError) {}
extern void otPlatDiagRadioTransmitDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError) {}

otError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
extern void otPlatRadioEnergyScanDone(otInstance *aInstance, int8_t aEnergyScanMaxRssi){}
#endif

//===========================================================================
otError otPlatDiagProcess(otInstance *aInstance,
                          uint8_t     aArgsLength,
                          char *      aArgs[],
                          char *      aOutput,
                          size_t      aOutputMaxLen)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aArgsLength);
    OT_UNUSED_VARIABLE(aArgs);
    OT_UNUSED_VARIABLE(aOutput);
    OT_UNUSED_VARIABLE(aOutputMaxLen);

    return OT_ERROR_NONE;
}

void otPlatDiagModeSet(bool aMode)
{
    OT_UNUSED_VARIABLE(aMode);
}

bool otPlatDiagModeGet(void)
{
    return false;
}

void otPlatDiagChannelSet(uint8_t aChannel)
{
    OT_UNUSED_VARIABLE(aChannel);
}

void otPlatDiagTxPowerSet(int8_t aTxPower)
{
    OT_UNUSED_VARIABLE(aTxPower);
}

void otPlatDiagRadioReceived(otInstance *aInstance, otRadioFrame *aFrame, otError aError)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aError);
}

void otPlatDiagAlarmCallback(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
}

// Debug
#include "pw_sys_io/sys_io.h"
pw::StatusWithSize pw::sys_io::WriteBytes(std::span<const std::byte> src)
{
    otLogCritMac("PW: %s", src.data());
    return pw::StatusWithSize(src.size_bytes());
}
#endif // OPENTHREAD_RCP_RPC_CLIENT_ENABLE
