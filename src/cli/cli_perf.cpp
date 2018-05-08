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
 *   This file implements a performance tool "Perf" for testing.
 */

#include "cli_perf.hpp"
#include "cli_udp_example.hpp"

#include <openthread/message.h>
#include <openthread/udp.h>

#include <openthread/platform/uart.h>

#include "cli/cli.hpp"
#include "cli/cli_uart.hpp"

#include "common/encoding.hpp"
#include "common/random.hpp"
#include "common/new.hpp"
#include "diag/diag_process.hpp"
#include "net/ip6_address.hpp"

using ot::Encoding::BigEndian::HostSwap16;
using ot::Encoding::BigEndian::HostSwap32;

namespace ot {
namespace Cli {
// *issue 1: priority range check
// *issue 2: edge issue,the last packet is out of interval, the formula is not correct
// *issue 3: server interval is not correct when loss rate is high
// issue 4: C,D send data to A at the same time, no server report
// *issue 5: server print jitter, the decimal aways is 0
// *issue 6: the mTotalLength is overflow when calculate bandwidth, long time test will produce it
// *issue 7: perf start server/client
// *issue 8: perf time 0 means unlimited sending time
// *issue 9: global session offset
// issue 10: four streams with the same priority, the first stream always has high loss rate
// *issue 11: perf offset
// *issue 12: server doesn't send ACKFIN when the server missed one FIN
// issue 13: send one more packet in the first second.
// *issue 14: add delay before sending FIN
// issue 15: send 0 packet
// *issue 16: the received packet id is smaller than the current packet id
const struct Perf::Command Perf::sCommands[] =
{
    { "help", &Perf::ProcessHelp },
    { "client", &Perf::ProcessClient },
    { "server", &Perf::ProcessServer },
    { "start", &Perf::ProcessStart },
    { "stop", &Perf::ProcessStop },
    { "sync", &Perf::ProcessSync },
    { "show", &Perf::ProcessShow },
    { "clear", &Perf::ProcessClear }
};

void Session::UpdatePacketStats(ReportPacket &aPacket)
{
    int64_t transit      = 0;
    int64_t deltaTransit = 0;

    if ((aPacket.mPacketId != 0) && (aPacket.mPacketId != mPacketId + 1))
    {
        if (aPacket.mPacketId < mPacketId + 1)
        {
            mStats.mCurCntOutOfOrder++;
            mStats.mTotalCntOutOfOrder++;

            mStats.mCurCntError--;
            mStats.mTotalCntError--;
        }
        else
        {
            mStats.mCurCntError += aPacket.mPacketId - (mPacketId + 1);
            mStats.mTotalCntError += aPacket.mPacketId - (mPacketId + 1);
        }
    }

    transit = (int64_t)aPacket.mRecvSec * 1000000LL + aPacket.mRecvUsec -
              ((int64_t)aPacket.mSentSec * 1000000LL + aPacket.mSentUsec);

    if (mStats.mTotalCntDatagram != 0)
    {
        deltaTransit = transit - mStats.mLastTransit;
        if (deltaTransit < 0)
        {
            deltaTransit = -deltaTransit;
        }

        mStats.mJitter += (deltaTransit - mStats.mJitter) / 16;;
    }

    mStats.mDeltaTransit = deltaTransit;
    mStats.mLastTransit = transit;

    if (aPacket.mPacketId > mPacketId)
    {
        mPacketId = aPacket.mPacketId;
    }

    if (aPacket.mLatency < mStats.mCurMinLatency)
    {
        mStats.mCurMinLatency = aPacket.mLatency;
    }

    if (aPacket.mLatency > mStats.mCurMaxLatency)
    {
        mStats.mCurMaxLatency = aPacket.mLatency;
    }

    mStats.mCurLatency += aPacket.mLatency;
    mStats.mCurLength += aPacket.mPacketLength;
    mStats.mCurCntDatagram++;

    if (aPacket.mLatency < mStats.mTotalMinLatency)
    {
        mStats.mTotalMinLatency = aPacket.mLatency;
    }

    if (aPacket.mLatency > mStats.mTotalMaxLatency)
    {
        mStats.mTotalMaxLatency = aPacket.mLatency;
    }

    mStats.mTotalLatency += aPacket.mLatency;
    mStats.mTotalLength += aPacket.mPacketLength;
    mStats.mTotalCntDatagram++;
}

void Session::BuildServerHeader(ServerHdr &aServerHdr)
{
    uint32_t interval = GetSessionEndTime() - GetSessionStartTime();

    aServerHdr.SetFlags(kHeaderVersion1 | kHeaderExtend);
    aServerHdr.SetTotalLen(mStats.mTotalLength);
    aServerHdr.SetStopSec(interval / 1000);
    aServerHdr.SetStopUsec((interval % 1000) * 1000);
    aServerHdr.SetCntError(mStats.mTotalCntError);
    aServerHdr.SetCntOutOfOrder(mStats.mTotalCntOutOfOrder);
    aServerHdr.SetCntDatagram(mStats.mTotalCntDatagram + mStats.mTotalCntOutOfOrder);
    aServerHdr.SetJitter(mStats.mJitter);
}

Perf::Perf(Instance *aInstance)
    : mServerRunning(false)
    , mClientRunning(false)
    , mPrintServerHeaderFlag(false)
    , mPrintClientHeaderFlag(false)
    , mSyncMode(kSyncModeUninit)
    , mSyncCnt(0)
    , mSyncTime(0)
    , mInstance(aInstance)
    , mServer(NULL)
    , mTransferTimer(*aInstance, &Perf::HandleTransferTimer, this)
    , mSyncTimer(*aInstance, &Perf::HandleSyncTimer, this)
    , mSession(NULL)
{
    memset(mSettings, 0, sizeof(mSettings));
    memset(mSessions, 0, sizeof(mSessions));

    for (size_t i = 0; i < sizeof(mSessions) / sizeof(mSessions[0]); i++)
    {
       mSessions[i].SetNext(&mSessions[i]);
    }

    otPlatLedPinInit();
}

otError Perf::Process(int argc, char *argv[], Server &aServer)
{
    otError error = OT_ERROR_PARSE;

    VerifyOrExit(argc != 0);

    mServer = &aServer;

    for (size_t i = 0; i < sizeof(sCommands) / sizeof(sCommands[0]); i++)
    {
        if (strcmp(argv[0], sCommands[i].mName) == 0)
        {
            error = (this->*sCommands[i].mCommand)(argc - 1, argv + 1);
            break;
        }
    }

exit:
    return error;
}

otError Perf::SetClientSetting(Setting &aSetting, int argc, char *argv[])
{
    otError error = OT_ERROR_NONE;
    long    value;

    for (int i = 0; i < argc; i += 2)
    {
        if (i == argc - 1)
        {
            ExitNow(error = OT_ERROR_INVALID_ARGS);
        }
        else if (strcmp(argv[i], "bandwidth") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value != 0, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagBandwidth);
            aSetting.SetBandwidth(static_cast<uint32_t>(value));
        }
        else if (strcmp(argv[i], "length") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(static_cast<uint16_t>(value) >= sizeof(UdpData), error = OT_ERROR_INVALID_ARGS);
            VerifyOrExit(value <= kMaxPayloadLength, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagLength);
            aSetting.SetLength(static_cast<uint16_t>(value));
        }
        else if (strcmp(argv[i], "interval") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value != 0, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagInterval);
            aSetting.SetInterval(static_cast<uint32_t>(value) * 1000);
        }
        else if (strcmp(argv[i], "priority") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value <= OT_MESSAGE_PRIORITY_VERY_LOW, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagPriority);
            aSetting.SetPriority(static_cast<otMessagePriority>(value));
        }
        else if (strcmp(argv[i], "time") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));

            aSetting.SetFlag(kFlagTime);
            aSetting.SetTime(static_cast<uint32_t>(value) * 1000);
        }
        else if (strcmp(argv[i], "number") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value != 0, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagNumber);
            aSetting.SetNumber(static_cast<uint32_t>(value));
        }
        else if (strcmp(argv[i], "format") == 0)
        {
            if (strcmp(argv[i + 1], "cvs") == 0)
            {
                aSetting.SetFlag(kFlagFormatCvs);
                aSetting.ClearFlag(kFlagFormatQuiet);
            }
            else if (strcmp(argv[i + 1], "quiet") == 0)
            {
                aSetting.SetFlag(kFlagFormatQuiet);
                aSetting.ClearFlag(kFlagFormatCvs);
            }
            else
            {
                ExitNow(error = OT_ERROR_INVALID_ARGS);
            }
        }
        else if (strcmp(argv[i], "id") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value <= 0xff, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagSessionId);
            aSetting.SetSessionId(static_cast<uint8_t>(value));
        }
        else if (strcmp(argv[i], "delay") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value <= 0xff, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagFinDelay);
            aSetting.SetFinDelay(static_cast<uint8_t>(value));
        }
        else if (strcmp(argv[i], "echo") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value <= 0xff, error = OT_ERROR_INVALID_ARGS);
            if (value > 0)
            {
                aSetting.SetFlag(kFlagEcho);
            }
            else
            {
                aSetting.ClearFlag(kFlagEcho);
            }
        }
        else
        {
            error = OT_ERROR_INVALID_ARGS;
            break;
        }
    }

exit:
    return error;
}

otError Perf::SetServerSetting(Setting &aSetting, int argc, char *argv[])
{
    otError error = OT_ERROR_NONE;
    long    value;

    for (int i = 0; i < argc - 1; i += 2)
    {
        if (i == argc - 1)
        {
            ExitNow(error = OT_ERROR_INVALID_ARGS);
        }
        else if (strcmp(argv[i], "interval") == 0)
        {
            SuccessOrExit(error = Interpreter::ParseLong(argv[i + 1], value));
            VerifyOrExit(value != 0, error = OT_ERROR_INVALID_ARGS);

            aSetting.SetFlag(kFlagInterval);
            aSetting.SetInterval(static_cast<uint32_t>(value) * 1000);
        }
        else if (strcmp(argv[i], "format") == 0)
        {
            if (strcmp(argv[i + 1], "cvs") == 0)
            {
                aSetting.SetFlag(kFlagFormatCvs);
                aSetting.ClearFlag(kFlagFormatQuiet);
            }
            else if (strcmp(argv[i + 1], "quiet") == 0)
            {
                aSetting.SetFlag(kFlagFormatQuiet);
                aSetting.ClearFlag(kFlagFormatCvs);
            }
            else
            {
                ExitNow(error = OT_ERROR_INVALID_ARGS);
            }
        }
        else
        {
            error = OT_ERROR_INVALID_ARGS;
            break;
        }
    }

exit:
    return error;
}

otError Perf::ProcessHelp(int argc, char *argv[])
{
    for (size_t i = 0; i < sizeof(sCommands) / sizeof(sCommands[0]); i++)
    {
        mServer->OutputFormat("%s\r\n", sCommands[i].mName);
    }

    OT_UNUSED_VARIABLE(argc);
    OT_UNUSED_VARIABLE(argv);

    return OT_ERROR_NONE;
}

otError Perf::ProcessClient(int argc, char *argv[])
{
    otError      error   = OT_ERROR_NONE;
    Setting *    setting = NULL;
    otIp6Address addr;

    VerifyOrExit(!(mServerRunning || mClientRunning), error = OT_ERROR_BUSY);
    VerifyOrExit(argc >= 1, error = OT_ERROR_PARSE);
    VerifyOrExit((setting = NewSetting()) != NULL, error = OT_ERROR_NO_BUFS);

    SuccessOrExit(error = otIp6AddressFromString(argv[0], &addr));
    SuccessOrExit(error = SetClientSetting(*setting, argc - 1, argv + 1));

    setting->SetFlag(kFlagClient);
    setting->SetAddr(addr);

exit:

    if ((error != OT_ERROR_NONE) && (setting != NULL))
    {
        FreeSetting(*setting);
    }

    return error;
}

otError Perf::ProcessServer(int argc, char *argv[])
{
    otError  error   = OT_ERROR_NONE;
    Setting *setting = NULL;

    VerifyOrExit(!(mServerRunning || mClientRunning), error = OT_ERROR_BUSY);

    for (size_t i = 0; i < sizeof(mSettings) / sizeof(mSettings[0]); i++)
    {
        if (mSettings[i].IsFlagSet(kFlagValid) && !mSettings[i].IsFlagSet(kFlagClient))
        {
            ExitNow(error = OT_ERROR_ALREADY);
        }
    }

    VerifyOrExit((setting = NewSetting()) != NULL, error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = SetServerSetting(*setting, argc, argv));

    setting->ClearFlag(kFlagClient);

exit:

    if ((error != OT_ERROR_NONE) && (setting != NULL))
    {
        FreeSetting(*setting);
    }

    return error;
}

void Perf::SessionStop(uint8_t aType)
{
    Session *cur, *next;

    cur = mSession;
    while (cur != NULL)
    {
        next = cur->GetNext();
        if (cur->GetType() == aType)
        {
            CloseSocket(*cur);
            FreeSession(*cur);
        }

        cur = next;
    }
}

otError Perf::ServerStart()
{
    otError  error   = OT_ERROR_NONE;
    Setting *setting = NULL;
    Session *session;

    VerifyOrExit(!mServerRunning);

    for (size_t i = 0; i < sizeof(mSettings) / sizeof(mSettings[0]); i++)
    {
        if (!mSettings[i].IsFlagSet(kFlagValid) || mSettings[i].IsFlagSet(kFlagClient))
        {
            continue;
        }

        setting = &mSettings[i];
        VerifyOrExit((session = NewSession(*setting)) != NULL, error = OT_ERROR_NO_BUFS);

        session->SetType(kTypeListener);
        session->SetState(kStateListen);
        session->SetLocalPort(Setting::kDefaultPort);

        if (OpenSocket(*session) != OT_ERROR_NONE)
        {
            FreeSession(*session);
            ExitNow(error = OT_ERROR_FAILED);
        }
    }

    VerifyOrExit(setting != NULL);

    mServerRunning = true;
    StartTransferTimer();

exit:
    if (error != OT_ERROR_NONE)
    {
        SessionStop(kTypeListener);
    }

    return error;
}

otError Perf::ClientStart()
{
    otError  error       = OT_ERROR_NONE;
    uint32_t transferNow = mTransferTimer.GetNow();
    uint32_t milliNow    = otPlatAlarmMilliGetNow();
    uint8_t  numSession  = 0;
    Setting *setting     = NULL;
    Session *session;
    uint32_t interval;
    uint32_t startTime;

    VerifyOrExit(!mClientRunning);

    for (size_t i = 0; i < sizeof(mSettings) / sizeof(mSettings[0]); i++)
    {
        if (!mSettings[i].IsFlagSet(kFlagValid) || !mSettings[i].IsFlagSet(kFlagClient))
        {
            continue;
        }

        numSession++;
    }

    VerifyOrExit(numSession != 0);

    for (size_t i = 0; i < sizeof(mSettings) / sizeof(mSettings[0]); i++)
    {
        if (!mSettings[i].IsFlagSet(kFlagValid) || !mSettings[i].IsFlagSet(kFlagClient))
        {
            continue;
        }

        setting = &mSettings[i];
        VerifyOrExit((session = NewSession(*setting)) != NULL, error = OT_ERROR_NO_BUFS);
 
        if (session->GetSetting().IsFlagSet(kFlagSessionId))
        {
            session->SetSessionId(session->GetSetting().GetSessionId());
        }
        else
        {
            session->SetSessionId(static_cast<uint8_t>(i));
        }

        interval = (setting->GetLength() * 8000000) / setting->GetBandwidth();
        if (interval < kMinSendInterval)
        {
            interval = kMinSendInterval;
        }

        session->SetType(kTypeClient);
        session->SetState(kStateSendData);
        session->SetPeerAddr(*setting->GetAddr());
        session->SetPeerPort(Setting::kDefaultPort);
        session->SetSendInterval(interval);

        // avoid streams with the same send interval
        startTime = transferNow + ((static_cast<uint32_t>(i) * interval) / numSession);
        session->SetTransferTime(startTime);
        session->SetSessionStartTime(milliNow);
        session->SetSessionEndTime(milliNow + session->GetSetting().GetInterval());
        if (OpenSocket(*session) != OT_ERROR_NONE)
        {
            FreeSession(*session);
            ExitNow(error = OT_ERROR_FAILED);
        }

        if (mPrintClientHeaderFlag == false)
        {
            mPrintClientHeaderFlag = true;
            PrintClientReportHeader(*session);
        }
    }

    VerifyOrExit(setting != NULL);

    mClientRunning = true;
    StartTransferTimer();

exit:
    if (error != OT_ERROR_NONE)
    {
        SessionStop(kTypeClient);
    }

    return error;
}

otError Perf::ProcessStart(int argc, char *argv[])
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(!(mServerRunning && mClientRunning), error = OT_ERROR_BUSY);
    VerifyOrExit(argc <= 1, error = OT_ERROR_INVALID_ARGS);

    if (argc == 0)
    {
        SuccessOrExit(error = ServerStart());
        SuccessOrExit(error = ClientStart());
    }
    else
    {
        if (strcmp(argv[0], "server") == 0)
        {
            SuccessOrExit(error = ServerStart());
        }
        else if (strcmp(argv[0], "client") == 0)
        {
            SuccessOrExit(error = ClientStart());
        }
        else
        {
            ExitNow(error = OT_ERROR_INVALID_ARGS);
        }
    }

exit:

    return error;
}

otError Perf::ServerStop()
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mServerRunning);

    SessionStop(kTypeListener);
    SessionStop(kTypeServer);

    mServerRunning = false;
    mPrintServerHeaderFlag = false;

    if (mClientRunning == false)
    {
        mTransferTimer.Stop();
    }

exit:

    return error;
}

otError Perf::ClientStop()
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mClientRunning);

    SessionStop(kTypeClient);

    mClientRunning = false;
    mPrintClientHeaderFlag = false;

    if (mServerRunning == false)
    {
        mTransferTimer.Stop();
    }

exit:

    return error;
}

otError Perf::ProcessStop(int argc, char *argv[])
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(mServerRunning || mClientRunning);
    VerifyOrExit(argc <= 1, error = OT_ERROR_INVALID_ARGS);

    if (argc == 0)
    {
        SuccessOrExit(error = ServerStop());
        SuccessOrExit(error = ClientStop());
    }
    else
    {
        if (strcmp(argv[0], "server") == 0)
        {
            SuccessOrExit(error = ServerStop());
        }
        else if (strcmp(argv[0], "client") == 0)
        {
            SuccessOrExit(error = ClientStop());
        }
        else
        {
            ExitNow(error = OT_ERROR_INVALID_ARGS);
        }
    }

exit:

    return error;
}

otError Perf::ProcessSync(int argc, char *argv[])
{
    otError error = OT_ERROR_NONE;

    if (argc == 0)
    {
        if (mSyncMode == kSyncModeMaster)
        {
            mServer->OutputFormat("master\r\n");
        }
        else if (mSyncMode == kSyncModeSlave)
        {
            mServer->OutputFormat("slave\r\n");
        }
        else if (mSyncMode == kSyncModeUninit)
        {
            mServer->OutputFormat("uninit\r\n");
        }
        else
        {
            ExitNow(error = OT_ERROR_INVALID_STATE);
        }
    }
    else if (argc == 1)
    {
        if (strcmp(argv[0], "master") == 0)
        {
            if (mSyncMode == kSyncModeMaster)
            {
                ExitNow();
            }
            mSyncMode = kSyncModeMaster;
            otPlatSyncPinMasterInit();
 
            mSyncCnt = 0;
            otPlatSyncPinClear();
 
            mSyncTimer.Start(kSyncInterval);
        }
        else if (strcmp(argv[0], "slave") == 0)
        {
            if (mSyncMode == kSyncModeSlave)
            {
                ExitNow();
            }
 
            mSyncMode = kSyncModeSlave;
            otPlatSyncPinSlaveInit(&Perf::HandleSyncEvent, this);
        }
        else if ((argc == 1) && (strcmp(argv[0], "stop") == 0))
        {
            if (mSyncTimer.IsRunning())
            {
                mSyncTimer.Stop();
            }

            otPlatSyncPinUninit();
            mSyncTime = 0;
            mSyncMode = kSyncModeUninit;
        }
        else
        {
            ExitNow(error = OT_ERROR_INVALID_ARGS);
        }
    }
    else
    {
        ExitNow(error = OT_ERROR_INVALID_ARGS);
    }

exit:

    return error;
}

otError Perf::ProcessShow(int argc, char *argv[])
{
    for (size_t i = 0; i < sizeof(mSettings) / sizeof(mSettings[0]); i++)
    {
        if (!mSettings[i].IsFlagSet(kFlagValid))
        {
            continue;
        }

        PrintSetting(mSettings[i]);
    }

    OT_UNUSED_VARIABLE(argc);
    OT_UNUSED_VARIABLE(argv);

    return OT_ERROR_NONE;
}

otError Perf::ProcessClear(int argc, char *argv[])
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(!(mServerRunning || mClientRunning), error = OT_ERROR_BUSY);

    for (size_t i = 0; i < sizeof(mSettings) / sizeof(mSettings[0]); i++)
    {
        if (mSettings[i].IsFlagSet(kFlagValid))
        {
            FreeSetting(mSettings[i]);
        }
    }

    OT_UNUSED_VARIABLE(argc);
    OT_UNUSED_VARIABLE(argv);

exit:
    return error;
}

otError Perf::OpenSocket(Session &aSession)
{
    otError    error = OT_ERROR_NONE;
    otSockAddr sockaddr;

    aSession.SetContext(*this, aSession);
    SuccessOrExit(otUdpOpen(mInstance, aSession.GetSocket(), HandleUdpReceive, aSession.GetContext()));

    switch (aSession.GetType())
    {
    case kTypeClient:
        memcpy(sockaddr.mAddress.mFields.m8, aSession.GetPeerAddr()->mFields.m8, sizeof(otIp6Address));
        sockaddr.mPort = aSession.GetPeerPort();
        sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;
        SuccessOrExit(error = otUdpConnect(aSession.GetSocket(), &sockaddr));

        if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs) &&
            !aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
        {
            mServer->OutputFormat("Client connecting to  %x:%x:%x:%x:%x:%x:%x:%x , ",
                 HostSwap16(sockaddr.mAddress.mFields.m16[0]),
                 HostSwap16(sockaddr.mAddress.mFields.m16[1]),
                 HostSwap16(sockaddr.mAddress.mFields.m16[2]),
                 HostSwap16(sockaddr.mAddress.mFields.m16[3]),
                 HostSwap16(sockaddr.mAddress.mFields.m16[4]),
                 HostSwap16(sockaddr.mAddress.mFields.m16[5]),
                 HostSwap16(sockaddr.mAddress.mFields.m16[6]),
                 HostSwap16(sockaddr.mAddress.mFields.m16[7]));
            mServer->OutputFormat("UDP port %d\n\r", sockaddr.mPort);
        }
        break;

    case kTypeListener:
        memset(sockaddr.mAddress.mFields.m8, 0, sizeof(otIp6Address));
        sockaddr.mPort = aSession.GetLocalPort();
        sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;

        SuccessOrExit(error = otUdpBind(aSession.GetSocket(), &sockaddr));

        if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs) &&
            !aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
        {
            mServer->OutputFormat("Server listening on UDP port %d\r\n", sockaddr.mPort);
        }
        break;

    case kTypeServer:
        memcpy(sockaddr.mAddress.mFields.m8, aSession.GetLocalAddr()->mFields.m8, sizeof(otIp6Address));
        sockaddr.mPort = aSession.GetLocalPort();
        sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;
        SuccessOrExit(error = otUdpBind(aSession.GetSocket(), &sockaddr));
        break;

    default:
        error = OT_ERROR_NOT_IMPLEMENTED;
        break;
    }

exit:
    return error;
}

otError Perf::CloseSocket(Session &aSession)
{
    return otUdpClose(aSession.GetSocket());
}

void Perf::HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    otPerfContext *context = static_cast<otPerfContext *>(aContext);
    context->mPerf->HandleUdpReceive(aMessage, aMessageInfo, *context->mSession);
}

void Perf::HandleUdpReceive(otMessage *aMessage, const otMessageInfo *aMessageInfo, Session &aSession)
{
    switch(aSession.GetType())
    {
    case kTypeClient:
        //mServer->OutputFormat("HandleServerMsg\r\n");
        IgnoreReturnValue(HandleServerMsg(*aMessage, *aMessageInfo, aSession));
        break;
    case kTypeListener:
        //mServer->OutputFormat("HandleConnectMsg\r\n");
        IgnoreReturnValue(HandleConnectMsg(*aMessage, *aMessageInfo, aSession));
        break;
    case kTypeServer:
        //mServer->OutputFormat("HandleClientMsg\r\n");
        IgnoreReturnValue(HandleClientMsg(*aMessage, *aMessageInfo, aSession));
        break;
    default:
        break;
    }
}

otError Perf::HandleServerMsg(otMessage &aMessage, const otMessageInfo &aMessageInfo, Session &aSession)
{
    otError   error = OT_ERROR_NONE;
    UdpData   data;
    ServerHdr hdr;

    VerifyOrExit(aSession.GetState() == kStateSendFin, error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(otMessageRead(&aMessage, otMessageGetOffset(&aMessage), &data, sizeof(UdpData)) == sizeof(UdpData),
                 error = OT_ERROR_PARSE);
    if (!(data.GetPacketId() & 0x80000000))
    {
        return OT_ERROR_PARSE;
    }

    otMessageSetOffset(&aMessage, otMessageGetOffset(&aMessage) + sizeof(UdpData));

    VerifyOrExit(otMessageRead(&aMessage, otMessageGetOffset(&aMessage), &hdr, sizeof(ServerHdr)) == sizeof(ServerHdr),
                 error = OT_ERROR_PARSE);

    //mServer->OutputFormat("Received AckFin\r\n");
    // stop sending
    CloseSocket(aSession);
    FreeSession(aSession);
    UpdateClientState();

    // read server stats
    PrintServerStats(aSession, hdr);
    OT_UNUSED_VARIABLE(aMessageInfo);

exit:
    return error;
}

otError Perf::HandleConnectMsg(otMessage &aMessage, const otMessageInfo &aMessageInfo, Session &aSession)
{
    otError      error    = OT_ERROR_NONE;
    uint32_t     now      = otPlatAlarmMilliGetNow();
    otIp6Address sockAddr = aMessageInfo.mSockAddr;
    otIp6Address peerAddr = aMessageInfo.mPeerAddr;
    UdpData      data;
    ReportPacket packet;
    Session *    session;

    packet.mPacketLength = otMessageGetLength(&aMessage) - otMessageGetOffset(&aMessage);
    VerifyOrExit(otMessageRead(&aMessage, otMessageGetOffset(&aMessage), &data, sizeof(UdpData)) == sizeof(UdpData),
                 error = OT_ERROR_PARSE);

    if (data.GetPacketId() & 0x80000000)
    {
        ExitNow();
    }

    if (FindSession(aMessageInfo) != NULL)
    {
        ExitNow();
    }

    VerifyOrExit((session = NewSession(aSession.GetSetting())) != NULL, error = OT_ERROR_NO_BUFS);
    session->SetSessionId(data.GetSessionId());
    session->SetType(kTypeServer);
    session->SetState(kStateRecvData);

    session->SetLocalAddr(sockAddr);
    session->SetLocalPort(aMessageInfo.mSockPort);

    session->SetPeerAddr(peerAddr);
    session->SetPeerPort(aMessageInfo.mPeerPort);

    session->SetSessionStartTime(now);
    session->SetSessionEndTime(now + session->GetSetting().GetInterval());

    packet.mPacketId = data.GetPacketId();
    packet.mRecvSec  = now / 1000;
    packet.mRecvUsec = (now % 1000) * 1000;
    packet.mSentSec  = data.GetSec();
    packet.mSentUsec = data.GetUsec();

    {
        uint32_t ab_time = mTransferTimer.GetNow() - mSyncTime;
        if (ab_time > data.GetTxUsec())
        {
            packet.mLatency = ab_time - data.GetTxUsec();
        }
        else
        {
            packet.mLatency = data.GetTxUsec() - ab_time;
        }
    }

    session->UpdatePacketStats(packet);

    if (OpenSocket(*session) != OT_ERROR_NONE)
    {
        FreeSession(*session);
        ExitNow(error = OT_ERROR_FAILED);
    }

    if (mPrintServerHeaderFlag == false)
    {
        mPrintServerHeaderFlag = true;
        PrintServerReportHeader(*session);
    }

    PrintConnection(*session);

    if (data.GetEchoFlag() > 0)
    {
        Message &         message  = *static_cast<Message *>(&aMessage);
        otMessagePriority priority = static_cast<otMessagePriority>(message.GetPriority());

        //mServer->OutputFormat("HandleConnectMsg: SendReply() priority=%d\r\n", priority);
        error = SendReply(aSession, data, priority, packet.mPacketLength);
    }

exit:
    return error;
}

otError Perf::HandleClientMsg(otMessage &aMessage, const otMessageInfo &aMessageInfo, Session &aSession)
{
    otError      error = OT_ERROR_NONE;
    uint32_t transferNow = mTransferTimer.GetNow();
    uint32_t     now   = otPlatAlarmMilliGetNow();
    ReportPacket packet;
    UdpData      data;

    if ((aSession.GetPeerPort() != aMessageInfo.mPeerPort) ||
        (memcmp(aSession.GetPeerAddr()->mFields.m8, aMessageInfo.mPeerAddr.mFields.m8, sizeof(otIp6Address)) != 0))
    {
        return OT_ERROR_DROP;
    }

    //VerifyOrExit(aSession.GetState() == kStateRecvData, error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(aSession.GetState() == kStateRecvData || aSession.GetState() == kStateFreeSession, error = OT_ERROR_INVALID_STATE);

    packet.mPacketLength = otMessageGetLength(&aMessage) - otMessageGetOffset(&aMessage);
    VerifyOrExit(otMessageRead(&aMessage, otMessageGetOffset(&aMessage), &data, sizeof(UdpData)) == sizeof(UdpData),
                 error = OT_ERROR_PARSE);

    if (data.GetPacketId() & 0x80000000)
    {
        Stats *stats = aSession.GetStats();


        //Message &         message  = *static_cast<Message *>(&aMessage);
        //otMessagePriority priority = static_cast<otMessagePriority>(message.GetPriority());
        //mServer->OutputFormat("HandleClientMsg: RecvFin priority=%d\r\n", priority);

        /*if (data.GetEchoFlag() > 0)
        {
            aSession.SetState(kStateFreeSession);
        }
        else
        {
            aSession.SetState(kStateSendAckFin);
        }*/

        if (aSession.GetState() != kStateFreeSession)
        {
            aSession.SetState(kStateFreeSession);
            aSession.SetPacketId(data.GetPacketId());
            aSession.SetFinOrAckCount(0);
            //aSession.SetTransferTime(now + kAckFinInterval);
            aSession.SetTransferTime(transferNow + kFinInterval * kMaxNumFin);
            //mServer->OutputFormat("Delay: %d\r\n", kFinInterval * kMaxNumFin);
         
            StartTransferTimer();

            aSession.SetSessionEndTime(now - data.GetFinDelay());
            aSession.SetIntervalEndTime(now - data.GetFinDelay() - aSession.GetSessionStartTime());
            // aSession.SetSessionEndTime(now);
            // aSession.SetIntervalEndTime(now - aSession.GetSessionStartTime());
         
            if (stats->mCurCntDatagram != 0)
            {
                PrintServerReport(aSession);
            }
         
            PrintServerReportEnd(aSession);
        }
    }
    else
    {
        uint32_t ab_time = mTransferTimer.GetNow() - mSyncTime;

        //Message &         message  = *static_cast<Message *>(&aMessage);
        //otMessagePriority priority = static_cast<otMessagePriority>(message.GetPriority());
        //mServer->OutputFormat("HandleClientMsg: RecvData priority=%d\r\n", priority);

        //mServer->OutputFormat("Received Data: PacketId=%d\r\n", data.GetPacketId());

        packet.mPacketId = data.GetPacketId();
        packet.mRecvSec  = now / 1000;
        packet.mRecvUsec = (now % 1000) * 1000;
        packet.mSentSec  = data.GetSec();
        packet.mSentUsec = data.GetUsec();

        if (ab_time > data.GetTxUsec())
        {
            packet.mLatency = ab_time - data.GetTxUsec();
        }
        else
        {
            packet.mLatency = ab_time + 2000 * kSyncInterval - data.GetTxUsec();
        }

        aSession.UpdatePacketStats(packet);
/*
        {
            Stats *stats = aSession.GetStats();
            mServer->OutputFormat("mLatency=%d curMinLatency=%d curAvgLatency=%d curMaxLatency=%d \r\n",
                                  packet.mLatency, stats->mCurMinLatency,
                                  stats->mCurLatency / stats->mCurCntDatagram,
                                  stats->mCurMaxLatency);
        }
*/
        if (aSession.IsSessionEndTimeBeforeOrEqual(now))
        {
            Stats *stats = aSession.GetStats();

            aSession.SetIntervalStartTime(aSession.GetIntervalEndTime());
            aSession.SetIntervalEndTime(now - aSession.GetSessionStartTime());
            aSession.SetSessionEndTime(aSession.GetSessionEndTime() + aSession.GetSetting().GetInterval());
            while (aSession.IsSessionEndTimeBeforeOrEqual(now)) {
                aSession.SetSessionEndTime(aSession.GetSessionEndTime() + aSession.GetSetting().GetInterval());
            }

            PrintServerReport(aSession);

            stats->mCurLength        = 0;
            stats->mCurCntError      = 0;
            stats->mCurCntDatagram   = 0;
            stats->mCurCntOutOfOrder = 0;
            stats->mCurMinLatency    = 0xffffffff;
            stats->mCurMaxLatency    = 0;
            stats->mCurLatency       = 0;
        }
    }

    if (data.GetEchoFlag() > 0)
    {
        Message &         message  = *static_cast<Message *>(&aMessage);
        otMessagePriority priority = static_cast<otMessagePriority>(message.GetPriority());

        //mServer->OutputFormat("HandleClientMsg: SendReply() priority=%d\r\n", priority);
        error = SendReply(aSession, data, priority, packet.mPacketLength);
    }

exit:
    return error;
}

otError Perf::SendReply(Session &aSession, UdpData &aData, otMessagePriority aPriority, uint16_t aLength)
{
    otError       error   = OT_ERROR_NONE;
    otMessage *   message = NULL;
    otMessageInfo messageInfo;

    OT_UNUSED_VARIABLE(aPriority);

    VerifyOrExit(aLength >= sizeof(UdpData), error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit((message = otUdpNewMessage(mInstance, true)) != NULL,
                 error = OT_ERROR_NO_BUFS);

    aData.SetEchoFlag(0);
    SuccessOrExit(error = otMessageAppend(message, &aData, sizeof(UdpData)));
    SuccessOrExit(error = otMessageSetLength(message, aLength));

    memset(&messageInfo, 0, sizeof(otMessageInfo));
    memcpy(messageInfo.mPeerAddr.mFields.m8, aSession.GetPeerAddr()->mFields.m8, sizeof(otIp6Address));
    messageInfo.mPeerPort = Setting::kDefaultPort;//aSession.GetPeerPort();
    messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

    error = otUdpSend(aSession.GetSocket(), message, &messageInfo);

exit:
    if (error != OT_ERROR_NONE)
    {
        if (message != NULL)
        {
            otMessageFree(message);
        }
    }

    return error;
}

otError Perf::SendData(Session &aSession)
{
    otError       error   = OT_ERROR_NONE;
    uint32_t      now     = otPlatAlarmMilliGetNow();
    otMessage *   message = NULL;
    Stats *       stats   = aSession.GetStats();
    otMessageInfo messageInfo;
    UdpData       data;

    VerifyOrExit(aSession.GetSetting().GetLength() >= sizeof(UdpData), error = OT_ERROR_INVALID_ARGS);

    data.SetPacketId(aSession.GetPacketId());
    data.SetSec(now / 1000);
    data.SetUsec((now % 1000) * 1000);
    data.SetSessionId(aSession.GetSessionId());
    data.SetTxUsec(mTransferTimer.GetNow() - mSyncTime);
    data.SetEchoFlag(aSession.GetSetting().IsFlagSet(kFlagEcho)? 1: 0);

    //mServer->OutputFormat("SendData() Echo=%d \r\n", data.GetEchoFlag());

    //mServer->OutputFormat("SendData() mPacketId=%d send_sec=%d send_usec=%d \r\n",
    //                       data.GetPacketId(), data.GetSec(), data.GetUsec());

    aSession.IncreasePacketId();

    VerifyOrExit((message = otUdpNewMessage(mInstance, true)) != NULL,
                 error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = otMessageAppend(message, &data, sizeof(UdpData)));
    SuccessOrExit(error = otMessageSetLength(message, aSession.GetSetting().GetLength()));

    memset(&messageInfo, 0, sizeof(otMessageInfo));
    memcpy(messageInfo.mPeerAddr.mFields.m8, aSession.GetPeerAddr()->mFields.m8, sizeof(otIp6Address));
    messageInfo.mPeerPort = aSession.GetPeerPort();
    messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

    error = otUdpSend(aSession.GetSocket(), message, &messageInfo);

    if ((aSession.GetLocalPort() == 0) && (aSession.GetSocket()->mSockName.mPort != 0))
    {
        aSession.SetLocalAddr(aSession.GetSocket()->mSockName.mAddress);
        aSession.SetLocalPort(aSession.GetSocket()->mSockName.mPort);

        PrintConnection(aSession);
    }

exit:
    if (error == OT_ERROR_NONE)
    {
        stats->mCurCntDatagram++;
        stats->mTotalCntDatagram++;
        stats->mCurLength   += aSession.GetSetting().GetLength();
        stats->mTotalLength += aSession.GetSetting().GetLength();
    }
    else
    {
        stats->mCurCntError++;
        stats->mTotalCntError++;
        if (message != NULL)
        {
            otMessageFree(message);
        }
    }

    if (aSession.IsSessionEndTimeBeforeOrEqual(now))
    {
        aSession.SetIntervalStartTime(aSession.GetIntervalEndTime());
        aSession.SetIntervalEndTime(now - aSession.GetSessionStartTime());

        while (aSession.IsSessionEndTimeBeforeOrEqual(now)) {
            aSession.SetSessionEndTime(aSession.GetSessionEndTime() + aSession.GetSetting().GetInterval());
        }

        PrintClientReport(aSession);

        stats->mCurLength        = 0;
        stats->mCurCntError      = 0;
        stats->mCurCntDatagram   = 0;
        stats->mCurCntOutOfOrder = 0;
        stats->mCurMinLatency    = 0xffffffff;
        stats->mCurMaxLatency    = 0;
        stats->mCurLatency       = 0;
    }

    return error;
}

otError Perf::SendFin(Session &aSession)
{
    otError       error   = OT_ERROR_NONE;
    uint32_t      now     = otPlatAlarmMilliGetNow();
    otMessage *   message = NULL;
    otMessageInfo messageInfo;
    UdpData       data;

    VerifyOrExit(aSession.GetSetting().GetLength() >= sizeof(UdpData), error = OT_ERROR_INVALID_ARGS);

    data.SetPacketId(aSession.GetPacketId());
    data.SetSec(now / 1000);
    data.SetUsec((now % 1000) * 1000);
    data.SetEchoFlag(aSession.GetSetting().IsFlagSet(kFlagEcho)? 1: 0);
    data.SetFinDelay(now - aSession.GetFinTime());

    aSession.DecreasePacketId();

    VerifyOrExit((message = otUdpNewMessage(mInstance, true)) != NULL,
                 error = OT_ERROR_NO_BUFS);

    SuccessOrExit(error = otMessageAppend(message, &data, sizeof(UdpData)));
    SuccessOrExit(error = otMessageSetLength(message, aSession.GetSetting().GetLength()));

    memset(&messageInfo, 0, sizeof(otMessageInfo));
    memcpy(messageInfo.mPeerAddr.mFields.m8, aSession.GetPeerAddr()->mFields.m8, sizeof(otIp6Address));
    messageInfo.mPeerPort = aSession.GetPeerPort();
    messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

    error = otUdpSend(aSession.GetSocket(), message, &messageInfo);

    if (error == OT_ERROR_NONE)
    {
        // mServer->OutputFormat("SendFin() mPacketId=%d  transferNow:%d\r\n", data.GetPacketId(), mTransferTimer.GetNow());
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        if (message != NULL)
        {
            otMessageFree(message);
        }
    }

    return error;
}

otError Perf::SendAckFin(Session &aSession)
{
    otError       error = OT_ERROR_NONE;
    otMessage *   message;
    otMessageInfo messageInfo;
    UdpData       data;
    ServerHdr     hdr;

    VerifyOrExit((message = otUdpNewMessage(mInstance, true)) != NULL,
                 error = OT_ERROR_NO_BUFS);

    data.SetPacketId(aSession.GetPacketId());
    data.SetSec(0);
    data.SetUsec(0);

    memset(&hdr, 0, sizeof(hdr));
    aSession.BuildServerHeader(hdr);

    SuccessOrExit(error = otMessageAppend(message, &data, sizeof(UdpData)));
    SuccessOrExit(error = otMessageAppend(message, &hdr, sizeof(ServerHdr)));

    memset(&messageInfo, 0, sizeof(otMessageInfo));
    memcpy(messageInfo.mPeerAddr.mFields.m8, aSession.GetPeerAddr()->mFields.m8, sizeof(otIp6Address));
    messageInfo.mPeerPort = aSession.GetPeerPort();
    messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

    error = otUdpSend(aSession.GetSocket(), message, &messageInfo);
    if (error == OT_ERROR_NONE)
    {
        //mServer->OutputFormat("SendAckFin() \r\n");
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        if (message != NULL)
        {
            otMessageFree(message);
        }
    }

    return error;
}

void Perf::PrintReport(Report &aReport, bool aIsServer)
{
    uint32_t interval  = aReport.mEndTime - aReport.mStartTime;
    uint32_t bandwidth = static_cast<uint32_t>((interval == 0)? 0: (aReport.mNumBytes * 8000UL / interval));
    uint32_t lossRate  = (aReport.mCntDatagram == 0)? 0: 100 * aReport.mCntError / aReport.mCntDatagram;
    uint32_t latency = (aReport.mCntDatagram == aReport.mCntError)? 0: aReport.mLatency / (aReport.mCntDatagram - aReport.mCntError);

    if (aReport.mIsFormatCvs)
    {
/*
        mServer->OutputFormat("%d,%d.%03d,%d.%03d,%d,%d,",
                              aReport.mSessionId,
                              aReport.mStartTime / 1000, aReport.mStartTime % 1000,
                              aReport.mEndTime / 1000, aReport.mEndTime % 1000,
                              aReport.mNumBytes, bandwidth);
        if (aIsServer)
        {
            mServer->OutputFormat("%d.%03d,%d,%d,%d,%d,",
                                  aReport.mJitter / 1000, aReport.mJitter % 1000,
                                  aReport.mCntError, aReport.mCntDatagram,
                                  lossRate, aReport.mCntOutOfOrder);
        }
*/

        mServer->OutputFormat("%d,", aReport.mReportType);
        mServer->OutputFormat("%d,%d.%03d,%d.%03d,",
                              aReport.mSessionId,
                              aReport.mStartTime / 1000, aReport.mStartTime % 1000,
                              aReport.mEndTime / 1000, aReport.mEndTime % 1000);

        mServer->OutputFormat("%d,", aReport.mNumBytes);
        mServer->OutputFormat("%d,", bandwidth);

        if (aIsServer)
        {
/*
            mServer->OutputFormat("%d.%03d,%d,%d,%d,%d.%03d,",
                                  aReport.mJitter / 1000, aReport.mJitter % 1000,
                                  aReport.mCntError, aReport.mCntDatagram,
                                  lossRate, latency / 1000, latency % 1000);
*/
            mServer->OutputFormat("%d.%03d,%d,%d,%d,",
                                  aReport.mJitter / 1000, aReport.mJitter % 1000,
                                  aReport.mCntError, aReport.mCntDatagram,
                                  lossRate);
            mServer->OutputFormat("%d.%03d,%d.%03d,%d.%03d",
                                  aReport.mMinLatency / 1000, aReport.mMinLatency % 1000,
                                  latency / 1000, latency % 1000,
                                  aReport.mMaxLatency / 1000, aReport.mMaxLatency % 1000);
        }

        mServer->OutputFormat("\r\n");
    }
    else
    {
/*
        mServer->OutputFormat("[%3d] %2d.%03d - %2d.%03d sec  %6d Bytes  %6d bits/sec  ",
                              aReport.mSessionId,
                              aReport.mStartTime / 1000, aReport.mStartTime % 1000,
                              aReport.mEndTime / 1000, aReport.mEndTime % 1000,
                              aReport.mNumBytes,
                              bandwidth);
*/

        mServer->OutputFormat("[%3d] %2d.%03d - %2d.%03d sec  ",
                              aReport.mSessionId,
                              aReport.mStartTime / 1000, aReport.mStartTime % 1000,
                              aReport.mEndTime / 1000, aReport.mEndTime % 1000);

        mServer->OutputFormat("%6d Bytes  ", aReport.mNumBytes);
        mServer->OutputFormat("%6d bits/sec  ", bandwidth);

        if (aIsServer)
        {
            mServer->OutputFormat("%2d.%03dms  ", aReport.mJitter / 1000, aReport.mJitter % 1000);
            mServer->OutputFormat("%3d/%3d (%2d%%) ",
                                  aReport.mCntError, aReport.mCntDatagram, lossRate);
            //mServer->OutputFormat("%d.%03dms", latency / 1000, latency % 1000);
            mServer->OutputFormat("%d.%03dms  %d.%03dms  %d.%03dms",
                                  aReport.mMinLatency / 1000, aReport.mMinLatency % 1000,
                                  latency / 1000, latency % 1000,
                                  aReport.mMaxLatency / 1000, aReport.mMaxLatency % 1000);
        }
        mServer->OutputFormat("\r\n");

        if (aReport.mCntOutOfOrder != 0)
        {
            mServer->OutputFormat("[%3d] %2d.%03d - %2d.%03d sec  ",
                                  aReport.mSessionId,
                                  aReport.mStartTime / 1000, aReport.mStartTime % 1000,
                                  aReport.mEndTime / 1000, aReport.mEndTime % 1000);
            mServer->OutputFormat("%d datagrams received out-of-order\r\n", aReport.mCntOutOfOrder);
        }
    }
}

void Perf::PrintConnection(Session &aSession)
{
    if (aSession.GetSetting().IsFlagSet(kFlagFormatCvs) ||
        aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        return;
    }

    mServer->OutputFormat("[%3d] local ", aSession.GetSessionId());
    mServer->OutputFormat("%x:%x:%x:%x:%x:%x:%x:%x ",
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[0]),
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[1]),
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[2]),
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[3]),
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[4]),
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[5]),
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[6]),
         HostSwap16(aSession.GetLocalAddr()->mFields.m16[7]));
    mServer->OutputFormat("port %d ", aSession.GetLocalPort());

    mServer->OutputFormat("connected with %x:%x:%x:%x:%x:%x:%x:%x ",
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[0]),
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[1]),
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[2]),
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[3]),
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[4]),
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[5]),
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[6]),
         HostSwap16(aSession.GetPeerAddr()->mFields.m16[7]));
    mServer->OutputFormat("port %d\r\n", aSession.GetPeerPort());
}

void Perf::PrintClientReportHeader(Session &aSession)
{
    if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs) &&
        !aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        mServer->OutputFormat("[ ID]  Interval              Transfer     Bandwidth\r\n");
    }
}

void Perf::PrintClientReport(Session &aSession)
{
    Stats *stats = aSession.GetStats();
    Report report;

    if (aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        return;
    }

    memset(&report, 0, sizeof(Report));

    report.mIsFormatCvs = aSession.GetSetting().IsFlagSet(kFlagFormatCvs);
    report.mReportType  = kReportTypeClient;
    report.mSessionId   = aSession.GetSessionId();
    report.mStartTime   = aSession.GetIntervalStartTime();
    report.mEndTime     = aSession.GetIntervalEndTime();
    report.mNumBytes    = static_cast<uint32_t>(stats->mCurLength);
    report.mCntError    = stats->mCurCntError;
    report.mCntDatagram = stats->mCurCntError + stats->mCurCntDatagram;

    PrintReport(report, false);
}

void Perf::PrintClientReportEnd(Session &aSession)
{
    Stats *stats = aSession.GetStats();
    Report report;

    if (aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        return;
    }

    memset(&report, 0, sizeof(Report));

    report.mIsFormatCvs = aSession.GetSetting().IsFlagSet(kFlagFormatCvs);
    report.mReportType  = kReportTypeClientEnd;

    report.mSessionId   = aSession.GetSessionId();
    report.mStartTime   = 0;
    report.mEndTime     = aSession.GetIntervalEndTime();
    report.mNumBytes    = stats->mTotalLength;
    report.mCntError    = stats->mTotalCntError;
    report.mCntDatagram = stats->mTotalCntError + stats->mTotalCntDatagram;

    if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs))
    {
        mServer->OutputFormat("\033[31m");
    }

    PrintReport(report, false);

    if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs))
    {
        mServer->OutputFormat("\033[0m");
    }
}

void Perf::PrintServerStats(Session &aSession, ServerHdr &aServerHdr)
{
    Report report;

    if (aSession.GetSetting().IsFlagSet(kFlagFormatCvs) ||
        aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        return;
    }

    memset(&report, 0, sizeof(Report));

    report.mIsFormatCvs   = aSession.GetSetting().IsFlagSet(kFlagFormatCvs);
    report.mSessionId     = aSession.GetSessionId();
    report.mStartTime     = 0;
    report.mEndTime       = aServerHdr.GetStopSec() * 1000 + aServerHdr.GetStopUsec() / 1000;
    report.mNumBytes      = aServerHdr.GetTotalLen();
    report.mJitter        = static_cast<uint32_t>(aServerHdr.GetJitter());
    report.mCntError      = aServerHdr.GetCntError();
    report.mCntDatagram   = aServerHdr.GetCntError() + aServerHdr.GetCntDatagram();
    report.mCntOutOfOrder = aServerHdr.GetCntOutOfOrder();

    if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs))
    {
        mServer->OutputFormat("[%3d] Server Report:\r\n", aSession.GetSessionId());
    }

    PrintReport(report, true);
}

void Perf::PrintServerReportHeader(Session &aSession)
{
    if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs) &&
        !aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        mServer->OutputFormat("[ ID] Interval             Transfer     Bandwidth         "
                              "Jitter    Lost/Total Datagrams\r\n");
    }
}

void Perf::PrintServerReport(Session &aSession)
{
    Stats  *stats = aSession.GetStats();
    Report report;

    if (aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        return;
    }

    memset(&report, 0, sizeof(Report));

    report.mIsFormatCvs   = aSession.GetSetting().IsFlagSet(kFlagFormatCvs);
    report.mReportType    = kReportTypeServer;

    report.mSessionId     = aSession.GetSessionId();
    report.mStartTime     = aSession.GetIntervalStartTime();
    report.mEndTime       = aSession.GetIntervalEndTime();
    report.mNumBytes      = static_cast<uint32_t>(stats->mCurLength);
    report.mJitter        = static_cast<uint32_t>(stats->mJitter);
    report.mCntError      = stats->mCurCntError;
    report.mCntDatagram   = stats->mCurCntError + stats->mCurCntDatagram;
    report.mCntOutOfOrder = stats->mCurCntOutOfOrder;
    report.mMinLatency    = stats->mCurMinLatency;
    report.mMaxLatency    = stats->mCurMaxLatency;
    report.mLatency       = stats->mCurLatency;

    PrintReport(report, true);
}

void Perf::PrintServerReportEnd(Session &aSession)
{
    Stats * stats = aSession.GetStats();
    Report  report;

    if (aSession.GetSetting().IsFlagSet(kFlagFormatQuiet))
    {
        return;
    }

    memset(&report, 0, sizeof(Report));

    report.mIsFormatCvs   = aSession.GetSetting().IsFlagSet(kFlagFormatCvs);
    report.mReportType    = kReportTypeServerEnd;

    report.mSessionId     = aSession.GetSessionId();
    report.mStartTime     = 0;
    report.mEndTime       = aSession.GetIntervalEndTime();
    report.mNumBytes      = stats->mTotalLength;
    report.mJitter        = static_cast<uint32_t>(stats->mJitter);
    report.mCntError      = stats->mTotalCntError;
    report.mCntDatagram   = stats->mTotalCntError + stats->mTotalCntDatagram;
    report.mCntOutOfOrder = stats->mTotalCntOutOfOrder;
    report.mMinLatency    = stats->mTotalMinLatency;
    report.mMaxLatency    = stats->mTotalMaxLatency;
    report.mLatency       = stats->mTotalLatency;

    if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs))
    {
        mServer->OutputFormat("\033[31m");
    }

    PrintReport(report, true);

    if (!aSession.GetSetting().IsFlagSet(kFlagFormatCvs))
    {
        mServer->OutputFormat("\033[0m");
    }
}

void Perf::PrintSetting(Setting &aSetting)
{
    otIp6Address *addr = aSetting.GetAddr();

    if (aSetting.IsFlagSet(kFlagClient))
    {
        mServer->OutputFormat("perf client ");
        mServer->OutputFormat("%x:%x:%x:%x:%x:%x:%x:%x ",
            HostSwap16(addr->mFields.m16[0]),
            HostSwap16(addr->mFields.m16[1]),
            HostSwap16(addr->mFields.m16[2]),
            HostSwap16(addr->mFields.m16[3]),
            HostSwap16(addr->mFields.m16[4]),
            HostSwap16(addr->mFields.m16[5]),
            HostSwap16(addr->mFields.m16[6]),
            HostSwap16(addr->mFields.m16[7]));
    }
    else
    {
        mServer->OutputFormat("perf server ");
    }

    if (aSetting.IsFlagSet(kFlagBandwidth))
    {
        mServer->OutputFormat("bandwidth %u ", aSetting.GetBandwidth());
    }

    if (aSetting.IsFlagSet(kFlagLength))
    {
        mServer->OutputFormat("length %u ", aSetting.GetLength());
    }

    if (aSetting.IsFlagSet(kFlagInterval))
    {
        mServer->OutputFormat("interval %u ", aSetting.GetInterval() / 1000);
    }

    if (aSetting.IsFlagSet(kFlagFormatCvs))
    {
        mServer->OutputFormat("format cvs ");
    }
    else if (aSetting.IsFlagSet(kFlagFormatQuiet))
    {
        mServer->OutputFormat("format quiet ");
    }

    if (aSetting.IsFlagSet(kFlagTime))
    {
        mServer->OutputFormat("time %u ", aSetting.GetTime() / 1000);
    }

    if (aSetting.IsFlagSet(kFlagNumber))
    {
        mServer->OutputFormat("number %u ", aSetting.GetNumber());
    }

    if (aSetting.IsFlagSet(kFlagPriority))
    {
        mServer->OutputFormat("priority %u ", aSetting.GetPriority());
    }

    if (aSetting.IsFlagSet(kFlagSessionId))
    {
        mServer->OutputFormat("id %u ", aSetting.GetSessionId());
    }

    if (aSetting.IsFlagSet(kFlagFinDelay))
    {
        mServer->OutputFormat("delay %u ", aSetting.GetFinDelay());
    }

    if (aSetting.IsFlagSet(kFlagEcho))
    {
        mServer->OutputFormat("echo 1 ");
    }

    mServer->OutputFormat("\r\n");
}

Setting *Perf::NewSetting()
{
    Setting *setting = NULL;

    for (size_t i = 0; i < sizeof(mSettings) / sizeof(mSettings[0]); i++)
    {
        if (!mSettings[i].IsFlagSet(kFlagValid))
        {
            setting = new(static_cast<void*>(&mSettings[i])) Setting();
            setting->SetFlag(kFlagValid);
            break;
        }
    }

    return setting;
}

void Perf::FreeSetting(Setting &aSetting)
{
    aSetting.ClearFlag(kFlagValid);
}

Session *Perf::NewSession(Setting &aSetting)
{
    Session *session = NULL;

    for (size_t i = 0; i < sizeof(mSessions) / sizeof(mSessions[0]); i++)
    {
        if (mSessions[i].GetNext() == &mSessions[i])
        {
            session = new(static_cast<void*>(&mSessions[i])) Session(aSetting);
            session->SetNext(mSession);
            mSession = session;

            break;
        }
    }

    return session;
}

void Perf::UpdateClientState()
{
    bool stop = true;

    if (!mClientRunning)
    {
       return;
    }

    if (mSession != NULL)
    {
        Session *cur, *next;
 
        cur = mSession;
        while (cur != NULL)
        {
            next = cur->GetNext();
            if (cur->GetType() == kTypeClient)
            {
                stop = false;
                break;
            }
 
            cur = next;
        }
    }

    if (stop)
    {
        ClientStop();
    }
}

void Perf::FreeSession(Session &aSession)
{
    Session *cur = mSession;
    Session *pre = NULL;

    while (cur != NULL)
    {
        if (cur == &aSession)
        {
            if (pre == NULL)
            {
                mSession = cur->GetNext();
            }
            else
            {
                pre->SetNext(cur->GetNext());
            }
        }

        pre = cur;
        cur = cur->GetNext();
    }

    aSession.SetNext(&aSession);
}

Session *Perf::FindSession(const otMessageInfo &aMessageInfo)
{
    Session *session = mSession;

    while(session != NULL)
    {
        if ((aMessageInfo.mPeerPort == session->GetPeerPort()) &&
            (memcmp(aMessageInfo.mPeerAddr.mFields.m8, session->GetPeerAddr()->mFields.m8, sizeof(otIp6Address)) == 0) &&
            (aMessageInfo.mSockPort == session->GetLocalPort()))
        {
            return session;
        }

        session = session->GetNext();
    }

    return NULL;
}

Perf &Perf::GetOwner(OwnerLocator &aOwnerLocator)
{
#if OPENTHREAD_ENABLE_MULTIPLE_INSTANCES
    Perf &perf = (aOwnerLocator.GetOwner<Perf>());
#else
    Perf &perf = Uart::sUartServer->GetInterpreter().GetPerf();
    OT_UNUSED_VARIABLE(aOwnerLocator);
#endif

   return perf;
}

otError Perf::FindMinTransferInterval(uint32_t &aInterval)
{
    otError  error       = OT_ERROR_NONE;
    uint32_t now         = mTransferTimer.GetNow();
    uint32_t minInterval = 0xffffffff;
    Session *session     = mSession;
    uint32_t interval;

    while(session != NULL)
    {
        if ((session->GetState() == kStateSendData) || (session->GetState() == kStateSendFin) ||
            (session->GetState() == kStateSendAckFin))
        {
            if (session->IsTransferTimeBeforeOrEqual(now))
            {
                aInterval = 0;
                ExitNow();
            }

            interval = session->GetTransferTimeDt(now);
            if (interval < minInterval)
            {
                minInterval = interval;
            }
        }

        session = session->GetNext();
    }

    VerifyOrExit(minInterval != 0xffffffff, error = OT_ERROR_NOT_FOUND);

    //mServer->OutputFormat("minInterval = %u \r\n", minInterval);
    aInterval = minInterval;

exit:
    return error;
}

void Perf::StartTransferTimer()
{
    uint32_t interval;

    if (mTransferTimer.IsRunning())
    {
        return;
    }

    if (FindMinTransferInterval(interval) == OT_ERROR_NONE)
    {
        mTransferTimer.Start(interval);
    }
}

void Perf::HandleSyncEvent(void *aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    Perf &perf = Uart::sUartServer->GetInterpreter().GetPerf();
    perf.HandleSyncEvent();
}

void Perf::HandleSyncEvent()
{
    mSyncTime = mTransferTimer.GetNow();

    otPlatLedPinToggle();

    // mServer->OutputFormat("SyncPinEvent:%d\r\n", mSyncTime);
}

void Perf::HandleSyncTimer(Timer &aTimer)
{
    GetOwner(aTimer).HandleSyncTimer();
}

void Perf::HandleSyncTimer()
{
    mSyncCnt++;
    if (mSyncCnt % 2 == 0)
    {
        otPlatSyncPinClear();
        mSyncTime = mTransferTimer.GetNow();
        otPlatLedPinToggle();
    }
    else
    {
        otPlatSyncPinSet();
    }

    mSyncTimer.StartAt(mSyncTimer.GetFireTime(), kSyncInterval);
}

void Perf::HandleTransferTimer(Timer &aTimer)
{
    GetOwner(aTimer).HandleTransferTimer();
}

void Perf::HandleTransferTimer()
{
    uint32_t      now         = otPlatAlarmMilliGetNow();
    uint32_t      transferNow = mTransferTimer.GetNow();
    Session *     session     = mSession, *nextSession;
    uint32_t      interval;
    Stats *       stats;
    Ip6::Address *localAddr;

    while (session != NULL)
    {
        nextSession = session->GetNext();

        if (!session->IsTransferTimeBeforeOrEqual(transferNow))
        {
            session = nextSession;
            continue;
        }
/*
        {
            otBufferInfo bufferInfo;
            otMessageGetBufferInfo(mInstance, &bufferInfo);
            mServer->OutputFormat("Timer(): session_id=%d now=%d total=%d free=%d\r\n",
                                 session->GetSessionId(), now,
                                 bufferInfo.mTotalBuffers, bufferInfo.mFreeBuffers);
        }
*/
        switch (session->GetState())
        {
        case kStateSendData:
            stats = session->GetStats();

            IgnoreReturnValue(SendData(*session));

            if ((session->GetSetting().GetTime() != 0) &&
                (now - session->GetSessionStartTime() >= session->GetSetting().GetTime()))
            {
                session->SetState(kStateSendFin);
            }

            if (session->GetSetting().IsFlagSet(kFlagNumber) &&
                (stats->mTotalCntDatagram >= session->GetSetting().GetNumber()))
            {
                session->SetState(kStateSendFin);
            }

            if (session->GetState() != kStateSendFin)
            {
                session->SetTransferTime(session->GetTransferTime() + session->GetSendInterval());
                break;
            }
 
            session->SetFinOrAckCount(0);
 
            // The negative datagram ID signifies termination to the server.
            session->NegativePacketId();
            session->SetIntervalEndTime(now - session->GetSessionStartTime());
            session->SetSessionEndTime(now);

            session->SetFinTime(now);

            PrintClientReportEnd(*session);
            if (session->GetSetting().GetFinDelay() != 0)
            {
                session->SetTransferTime(session->GetTransferTime() + kFinInterval);
                //mServer->OutputFormat("FinDelay: %d\r\n", session->GetSetting().GetFinDelay() * 1000000);
                session->SetTransferTime(session->GetTransferTime() + session->GetSetting().GetFinDelay() * 1000000);
                break;
            }

            // Fall through

        case kStateSendFin:
            if (SendFin(*session) == OT_ERROR_NONE)
            {
                session->SetFinOrAckCount(session->GetFinOrAckCount() + 1);
            }
 
            if (session->GetFinOrAckCount() >= kMaxNumFin)
            {
                CloseSocket(*session);
                FreeSession(*session);
                UpdateClientState();
            }
            else
            {
                session->SetTransferTime(session->GetTransferTime() + kFinInterval);
            }
            break;
 
        case kStateSendAckFin:
            localAddr = static_cast<Ip6::Address*>(session->GetLocalAddr());

            if (localAddr->IsMulticast())
            {
                CloseSocket(*session);
                FreeSession(*session);
            }
            else
            {
                if (SendAckFin(*session) == OT_ERROR_NONE)
                {
                    session->SetFinOrAckCount(session->GetFinOrAckCount() + 1);
                }
             
                if (session->GetFinOrAckCount() >= kMaxNumAckFin)
                {
                    CloseSocket(*session);
                    FreeSession(*session);
                }
                else
                {
                    session->SetTransferTime(session->GetTransferTime() + kAckFinInterval);
                }
            }
            break;

        case kStateFreeSession:
                //mServer->OutputFormat("kStateFreeSession: transferNow:%d\r\n", mTransferTimer.GetNow());
                CloseSocket(*session);
                FreeSession(*session);
            break;

        default:
            break;
        }

        session = nextSession;
    }

    if (FindMinTransferInterval(interval) == OT_ERROR_NONE)
    {
        mTransferTimer.StartAt(mTransferTimer.GetFireTime(), interval);
    }
}

}  // namespace Cli
}  // namespace ot
