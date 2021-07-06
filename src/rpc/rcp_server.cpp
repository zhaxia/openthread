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
#include <openthread/platform/uart.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "mac/mac_frame.hpp"
#include "rpc/rcp_server.hpp"

#if OPENTHREAD_RCP_RPC_SERVER_ENABLE

RcpServer *RcpServer::sRcpServer = nullptr;

static OT_DEFINE_ALIGNED_VAR(sRcpServerRaw, sizeof(RcpServer), uint64_t);

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

void RcpService::LinkRawEnergyScanDone(otInstance *, int8_t aEnergyScanMaxRssi)
{
    RcpServer::GetInstance().GetRcpService().LinkRawEnergyScanDone(aEnergyScanMaxRssi);
}

void RcpService::LinkRawEnergyScanDone(int8_t aEnergyScanMaxRssi)
{
    ot_rpc_RadioScanResult frame;

    otLogCritMac("RcpService::LinkRawEnergyScanDone()");
    // VerifyOrExit(mEnergyScanDoneWriter.open());

    frame.mMaxRssi = aEnergyScanMaxRssi;

    otLogCritMac("RcpService::LinkRawEnergyScanDone(): Write(MaxRssi: %d)", aEnergyScanMaxRssi);
    mEnergyScanDoneWriter.Write(frame);
    // mEnergyScanDoneWriter.Finish();

    // exit:
    return;
}

void RcpService::LinkRawReceiveDone(otInstance *, otRadioFrame *aFrame, otError aError)
{
    RcpServer::GetInstance().GetRcpService().LinkRawReceiveDone(aFrame, aError);
}

void RcpService::LinkRawReceiveDone(otRadioFrame *aFrame, otError aError)
{
    ot_rpc_RadioRxDoneFrame frame;

    VerifyOrExit(mReceiveDoneWriter.open());

    memset(&frame, 0, sizeof(frame));

    frame.mError = aError;

    if (aError == OT_ERROR_NONE)
    {
        frame.has_mFrame = true;
        EncodeRadioRxFrame(aFrame, &frame.mFrame);
    }

    mReceiveDoneWriter.Write(frame);

exit:
    return;
}

void RcpService::LinkRawTransmitDone(otInstance *  aInstance,
                                     otRadioFrame *aFrame,
                                     otRadioFrame *aAckFrame,
                                     otError       aError)
{
    OT_UNUSED_VARIABLE(aInstance);
    RcpServer::GetInstance().GetRcpService().LinkRawTransmitDone(aFrame, aAckFrame, aError);
}

void RcpService::EncodeRadioRxFrame(otRadioFrame *aFrame, ot_rpc_RadioRxFrame *aRpcRadioRxFrame)
{
    memset(aRpcRadioRxFrame, 0, sizeof(ot_rpc_RadioRxFrame));

    memcpy(aRpcRadioRxFrame->mFrame.mPsdu.bytes, aFrame->mPsdu, aFrame->mLength);
    aRpcRadioRxFrame->mFrame.mPsdu.size              = aFrame->mLength;
    aRpcRadioRxFrame->mFrame.mChannel                = aFrame->mChannel;
    aRpcRadioRxFrame->mRxInfo.mTimestamp             = aFrame->mInfo.mRxInfo.mTimestamp;
    aRpcRadioRxFrame->mRxInfo.mAckFrameCounter       = aFrame->mInfo.mRxInfo.mAckFrameCounter;
    aRpcRadioRxFrame->mRxInfo.mAckKeyId              = aFrame->mInfo.mRxInfo.mAckKeyId;
    aRpcRadioRxFrame->mRxInfo.mRssi                  = aFrame->mInfo.mRxInfo.mRssi;
    aRpcRadioRxFrame->mRxInfo.mLqi                   = aFrame->mInfo.mRxInfo.mLqi;
    aRpcRadioRxFrame->mRxInfo.mAckedWithFramePending = aFrame->mInfo.mRxInfo.mAckedWithFramePending;
    aRpcRadioRxFrame->mRxInfo.mAckedWithSecEnhAck    = aFrame->mInfo.mRxInfo.mAckedWithSecEnhAck;
}

void RcpService::LinkRawTransmitDone(otRadioFrame *aFrame, otRadioFrame *aAckFrame, otError aError)
{
    OT_UNUSED_VARIABLE(aFrame);
    OT_UNUSED_VARIABLE(aAckFrame);
    OT_UNUSED_VARIABLE(aError);

    ot_rpc_RadioTxDoneFrame frame;

    memset(&frame, 0, sizeof(frame));

    frame.mError = aError;

    if (aError == OT_ERROR_NONE)
    {
        frame.has_mAck = true;
        EncodeRadioRxFrame(aAckFrame, &frame.mAck);
    }

    if (static_cast<ot::Mac::TxFrame *>(aFrame)->GetSecurityEnabled())
    {
        uint8_t  keyId;
        uint32_t frameCounter;

        if (static_cast<ot::Mac::TxFrame *>(aFrame)->GetKeyId(keyId) == OT_ERROR_NONE)
        {
            frame.has_mKeyId = true;
            frame.mKeyId     = keyId;
        }

        if (static_cast<ot::Mac::TxFrame *>(aFrame)->GetFrameCounter(frameCounter) == OT_ERROR_NONE)
        {
            frame.has_mFrameCounter = true;
            frame.mFrameCounter     = frameCounter;
        }
    }
}

extern "C" void otRcpInit(otInstance *aInstance)
{
    RcpServer *rcpServer = new (&sRcpServerRaw) RcpServer(aInstance);
    OT_ASSERT((rcpServer != nullptr) && (rcpServer == &RcpServer::GetInstance()));
}

extern "C" void otRcpProcess(otInstance *aInstance)
{
    (void)aInstance;

    RcpServer::GetInstance().GetRcpService().SendEnergyScanResponse();
}

otError otRpcServerCommand(otInstance *aInstance, const char *aCommand)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aCommand);

    otLogCritMac("otRpcServerCommand");

    // RcpServer::GetInstance().GetRcpService().RadioSendFrame();

    return OT_ERROR_NONE;
}

//=========================================================
#if 0
void otPlatStreamReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    otLogCritMac("otPlatStreamReceived");

    RcpServer::GetInstance().ProcessPacket(
        std::span<const std::byte>(reinterpret_cast<const std::byte *>(aBuf), aBufLength));
}

otError otPlatStreamSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    return otPlatUartSend(aBuf, aBufLength);
}
#endif

#include "pw_hdlc/decoder.h"
#include "pw_hdlc/encoder.h"
pw::hdlc::DecoderBuffer<1024> sHdlcDecoder;

void otPlatStreamReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    pw::Result<pw::hdlc::Frame> result = pw::Status::Unknown();
    LogBytes("HdlcReceived", aBuf, aBufLength);

    for (uint16_t i = 0; i < aBufLength; i++)
    {
        result = sHdlcDecoder.Process(std::byte(aBuf[i]));
        if (result.status() == pw::OkStatus())
        {
            LogBytes("RpcReceived ", reinterpret_cast<const uint8_t *>(result.value().data().data()),
                     result.value().data().size_bytes());
            // otLogCritMac("RpcReceived: ParseFrame");
            // PrintRpcPayload(reinterpret_cast<const uint8_t *>(result.value().data().data()),
            //                result.value().data().size_bytes());
            RcpServer::GetInstance().ProcessPacket(result.value().data());
        }
    }
}

#include "pw_bytes/array.h"
#include "pw_stream/memory_stream.h"
std::array<std::byte, 1024> sTxBuffer;
constexpr uint8_t           kAddress = 0x7B; // 123

otError otPlatStreamSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    otError                  error = OT_ERROR_NONE;
    pw::stream::MemoryWriter writer(sTxBuffer);

    LogBytes("RpcSend ", aBuf, aBufLength);
    // otLogCritMac("RpcSend: ParseFrame");
    // PrintRpcPayload(aBuf, aBufLength);

    VerifyOrExit(pw::hdlc::WriteUIFrame(
                     kAddress, std::span<const std::byte>(reinterpret_cast<const std::byte *>(aBuf), aBufLength),
                     writer) == pw::OkStatus(),
                 error = OT_ERROR_NONE);

    LogBytes("HdlcSend", reinterpret_cast<const uint8_t *>(writer.data()),
             static_cast<uint16_t>(writer.bytes_written()));
    error =
        otPlatUartSend(reinterpret_cast<const uint8_t *>(writer.data()), static_cast<uint16_t>(writer.bytes_written()));

exit:
    return error;
}

extern "C" void otNcpInit(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
}

extern "C" void otPlatUartSendDone(void)
{
}

extern "C" void otPlatUartReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    otPlatStreamReceived(aBuf, aBufLength);
}

otError otPlatStreamEnable(void)
{
    return otPlatUartEnable();
}

#if (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_APP)
extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    va_list args;

    va_start(args, aFormat);

    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);

    va_end(args);
}

extern "C" void otPlatLogLine(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aLogLine)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aLogLine);
}

#endif // (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_APP)

// Debug
#include "pw_sys_io/sys_io.h"
pw::StatusWithSize pw::sys_io::WriteBytes(std::span<const std::byte> src)
{
    otLogCritMac("PW: %s", src.data());
    return pw::StatusWithSize(src.size_bytes());
}
#endif // OPENTHREAD_RCP_RPC_SERVER_ENABLE
