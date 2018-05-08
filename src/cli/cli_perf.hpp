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
 *   This file contains definitions for a simple CLI CoAP server and client.
 */

#ifndef CLI_PERF_HPP_
#define CLI_PERF_HPP_

#include "openthread-core-config.h"

#include <stdio.h>
#include <openthread/platform/toolchain.h>
#include <openthread/udp.h>
#include <openthread/types.h>

#include "cli/cli_server.hpp"
#include "common/encoding.hpp"
#include "common/timer.hpp"
#include "common/instance.hpp"
#include "net/ip6_headers.hpp"
#include "net/udp6.hpp"

using ot::Encoding::BigEndian::HostSwap32;
namespace ot {
namespace Cli {

class Interpreter;
class Perf;
class Session;
class Setting;

enum
{
    kFlagValid           = 1,
    kFlagClient          = 1 << 1,
    kFlagBandwidth       = 1 << 2,
    kFlagLength          = 1 << 3,
    kFlagPort            = 1 << 4,
    kFlagBindPort        = 1 << 5,
    kFlagInterval        = 1 << 6,
    kFlagPriority        = 1 << 7,
    kFlagTime            = 1 << 8,
    kFlagNumber          = 1 << 9,
    kFlagSessionId       = 1 << 10,
    kFlagFormatCvs       = 1 << 11,
    kFlagFormatQuiet     = 1 << 12,
    kFlagEcho            = 1 << 13,
    kFlagFinDelay        = 1 << 14,
};

enum
{
    kStateIdle,
    kStateListen,
    kStateSendData,
    kStateRecvData,
    kStateSendFin,
    kStateSendAckFin,
    kStateFreeSession,
};

enum
{
    kHeaderVersion1    = 0x80000000,
    kHeaderExtend      = 0x40000000,
};

struct ReportPacket
{
    uint32_t mPacketId;
    uint16_t mPacketLength;
    uint32_t mSentSec;
    uint32_t mSentUsec;
    uint32_t mRecvSec;
    uint32_t mRecvUsec;
    uint32_t mLatency;
};

OT_TOOL_PACKED_BEGIN
class UdpData
{
public:
    uint32_t GetPacketId() { return HostSwap32(mPacketId); }
    void SetPacketId(uint32_t aPacketId) { mPacketId = HostSwap32(aPacketId); }

    uint32_t GetSec() { return HostSwap32(mSec); }
    void SetSec(uint32_t aSec) { mSec = HostSwap32(aSec); }

    uint32_t GetUsec() { return HostSwap32(mUsec); }
    void SetUsec(uint32_t aUsec) { mUsec = HostSwap32(aUsec); }

    uint32_t GetTxUsec() { return HostSwap32(mTxUsec); }
    void SetTxUsec(uint32_t aTxUsec) { mTxUsec = HostSwap32(aTxUsec); }

    uint32_t GetFinDelay() { return HostSwap32(mFinDelay); }
    void SetFinDelay(uint32_t aFinDelay) { mFinDelay = HostSwap32(aFinDelay); }

    uint8_t GetSessionId() { return mSessionId; }
    void SetSessionId(uint8_t aSessionId) { mSessionId = aSessionId; }

    uint8_t GetEchoFlag() { return mEchoFlag; }
    void SetEchoFlag(uint8_t aEchoFlag) { mEchoFlag = aEchoFlag; }

private:
    uint32_t mPacketId;
    uint32_t mSec;
    uint32_t mUsec;
    uint32_t mTxUsec;
    uint32_t mFinDelay;
    uint8_t mSessionId;
    uint8_t mEchoFlag;
}OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
class ServerHdrV1 {
public:
   uint32_t GetFlags() { return HostSwap32(mFlags); }
   void SetFlags(uint32_t aFlags) { mFlags = HostSwap32(aFlags); }

   uint32_t GetTotalLen1() { return HostSwap32(mTotalLen1); }
   uint32_t GetTotalLen2() { return HostSwap32(mTotalLen2); }

   uint64_t GetTotalLen() { return (static_cast<uint64_t>(HostSwap32(mTotalLen1)) << 32) + HostSwap32(mTotalLen2); }
   void SetTotalLen(uint64_t aTotalLen) { mTotalLen1 = HostSwap32(aTotalLen >> 32); mTotalLen2 = HostSwap32(aTotalLen & 0xffffffff); }

   uint32_t GetStopSec() { return HostSwap32(mStopSec); }
   void SetStopSec(uint32_t aStopSec) { mStopSec = HostSwap32(aStopSec); }

   uint32_t GetStopUsec() { return HostSwap32(mStopUsec); }
   void SetStopUsec(uint32_t aStopUsec) { mStopUsec = HostSwap32(aStopUsec); }

   uint32_t GetCntError() { return HostSwap32(mCntError); }
   void SetCntError(uint32_t aCntError) { mCntError = HostSwap32(aCntError); }

   uint32_t GetCntOutOfOrder() { return HostSwap32(mCntOutOfOrder); }
   void SetCntOutOfOrder(uint32_t aCntOutOfOrder) { mCntOutOfOrder = HostSwap32(aCntOutOfOrder); }

   uint32_t GetCntDatagram() { return HostSwap32(mCntDatagram); }
   void SetCntDatagram(uint32_t aCntDatagram) { mCntDatagram = HostSwap32(aCntDatagram); }

   int64_t GetJitter()
   {
       return (static_cast<int64_t>(HostSwap32(mJitter1)) << 32) + HostSwap32(mJitter2);
   }

   void SetJitter(int64_t aJitter)
   {
       mJitter1 = HostSwap32(static_cast<uint32_t>(aJitter >> 32));
       mJitter2 = HostSwap32(static_cast<uint32_t>(aJitter & 0xffffffff));
   }

private:
/*
 * flags is a bitmap for different options
 * the most significant bits are for determining
 * which information is available. So 1.7 uses
 * 0x80000000 and the next time information is added
 * the 1.7 bit will be set and 0x40000000 will be
 * set signifying additional information. If no
 * information bits are set then the header is ignored.
 */
    uint32_t mFlags;
    uint32_t mTotalLen1;
    uint32_t mTotalLen2;
    uint32_t mStopSec;
    uint32_t mStopUsec;
    uint32_t mCntError;
    uint32_t mCntOutOfOrder;
    uint32_t mCntDatagram;
    uint32_t mJitter1;
    uint32_t mJitter2;
} OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
class ServerHdrExtension {
/*
    uint32_t mMinTransit1;
    uint32_t mMinTransit2;
    uint32_t mMaxTransit1;
    uint32_t mMaxTransit2;
    uint32_t mSumTransit1;
    uint32_t mSumTransit2;
    uint32_t mMeanTransit1;
    uint32_t mMeanTransit2;
    uint32_t mM2Transit1;
    uint32_t mM2Transit2;
    uint32_t mVdTransit1;
    uint32_t mVdTransit2;
    uint32_t mCntTransit;
    uint32_t mIpgCnt;
    uint32_t mIpgSum;
*/
} OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
class ServerHdr: public ServerHdrV1, public ServerHdrExtension {
} OT_TOOL_PACKED_END;

struct Stats
{
public:
    uint32_t mCurLength;
    uint32_t mCurCntDatagram;
    uint32_t mCurCntOutOfOrder;     ///<
    uint32_t mCurCntError;          ///<

    uint64_t mTotalLength;
    uint32_t mTotalCntDatagram;
    uint32_t mTotalCntOutOfOrder;   ///<
    uint32_t mTotalCntError;        ///<

    int64_t  mJitter;               ///<
    int64_t  mLastTransit;              ///<
    int64_t  mDeltaTransit;              ///<

    uint32_t mCurMinLatency;
    uint32_t mCurMaxLatency;
    uint32_t mCurLatency;

    uint32_t mTotalMinLatency;
    uint32_t mTotalMaxLatency;
    uint32_t mTotalLatency;
};


enum
{
    kReportTypeUnknown   = 0,
    kReportTypeClient    = 1,
    kReportTypeClientEnd = 2,
    kReportTypeServer    = 3,
    kReportTypeServerEnd = 4,
};

struct Report {
  bool     mIsFormatCvs;
  uint8_t  mReportType;

  uint32_t mSessionId;
  uint32_t mStartTime;
  uint32_t mEndTime;
  uint64_t mNumBytes;
  uint32_t mJitter;
  uint32_t mCntError;
  uint32_t mCntDatagram;
  uint32_t mCntOutOfOrder;
  uint32_t mLatency;
  uint32_t mMinLatency;
  uint32_t mMaxLatency;
};

class Setting
{
public:
    Setting()
        : mFlags(0)
        , mLength(kDefaultLength)
        , mBandwidth(kDefaultBandwidth)
        , mInterval(kDefaultInterval)
        , mTime(kDefaultTime)
        , mPriority(OT_MESSAGE_PRIORITY_LOW)
        , mFinDelay(0)
    {
        memset(&mAddr, 0, sizeof(otIp6Address));
    }

    void SetFlag(uint32_t aFlag) { mFlags |= aFlag; }
    void ClearFlag(uint32_t aFlag) { mFlags &= (~aFlag); }
    bool IsFlagSet(uint32_t aFlag) { return (mFlags & aFlag)? true: false; }

    otIp6Address *GetAddr() { return &mAddr; }
    void SetAddr(otIp6Address &aAddr) { memcpy(mAddr.mFields.m8, aAddr.mFields.m8, sizeof(otIp6Address)); }

    uint32_t GetBandwidth() { return mBandwidth; }
    void SetBandwidth(uint32_t aBandwidth) { mBandwidth = aBandwidth; }

    uint16_t GetLength() { return mLength; }
    void SetLength(uint16_t aLength) { mLength = aLength; }

    uint32_t GetInterval() { return mInterval; }
    void SetInterval(uint32_t aInterval) { mInterval = aInterval; }

    uint32_t GetTime() { return mTime; }
    void SetTime(uint32_t aTime) { mTime = aTime; }

    uint32_t GetNumber() { return mNumber; }
    void SetNumber(uint32_t aNumber) { mNumber = aNumber; }

    otMessagePriority GetPriority() { return mPriority; }
    void SetPriority(otMessagePriority aPriority) { mPriority = aPriority; }

    uint8_t GetSessionId() { return mSessionId; }
    void SetSessionId(uint8_t aSessionId) { mSessionId = aSessionId; }

    uint8_t GetFinDelay() { return mFinDelay; }
    void SetFinDelay(uint8_t aFinDelay) { mFinDelay = aFinDelay; }

    enum
    {
        kDefaultBandwidth  = 10000,
        kDefaultLength     = 64,
        kDefaultPort       = 5001,
        kDefaultInterval   = 1000,    // microsecond
        kDefaultTime       = 11000,   // microsecond
    };
private:


    ///< perf client fe80::1 bandwidth 10000
    ///< perf server interval  2
    ///< perf start
    ///< perf stop
    ///< perf show
    ///< perf clear

    uint16_t          mFlags;
    uint16_t          mLength;    ///< -l           length      byte
    otIp6Address      mAddr;      ///< 
    uint32_t          mBandwidth; ///< -b           bandwidth   bit/sec
    uint32_t          mInterval;  ///< -i           interval    second
    uint32_t          mTime;      ///< -t           time        second
    uint32_t          mNumber;    ///< -n           number
    otMessagePriority mPriority;  ///< -S           tos
    uint8_t           mSessionId;
    uint8_t           mFinDelay;
};

struct otPerfContext
{
    Perf *mPerf;
    Session *mSession;
};

enum
{
    kTypeClient,
    kTypeListener,
    kTypeServer,
};

class Session
{
public:

    Session() { }
    Session(Setting &aSetting) {
        memset(this, 0, sizeof(Session));

        mStats.mCurMinLatency = 0xffffffff;
        mStats.mTotalMinLatency = 0xffffffff;
        mSetting = &aSetting;
    }

    uint8_t GetType() { return mType; }
    void SetType(uint8_t aType) { mType = aType; }

    uint8_t GetState() { return mState; }
    void SetState(uint8_t aState) { mState = aState; }

    Setting &GetSetting() { return *mSetting; }

    otUdpSocket *GetSocket() { return &mSocket; }
    void SetSocket(otUdpSocket aSocket) { mSocket = aSocket; }

    otIp6Address *GetLocalAddr() { return &mLocalAddr; }
    void SetLocalAddr(otIp6Address &aLocalAddr)
    {
        memcpy(mLocalAddr.mFields.m8, aLocalAddr.mFields.m8, sizeof(otIp6Address));
    }

    uint16_t GetLocalPort() { return mLocalPort; }
    void SetLocalPort(uint16_t aLocalPort) { mLocalPort = aLocalPort; }

    otIp6Address *GetPeerAddr() { return &mPeerAddr; }
    void SetPeerAddr(otIp6Address &aPeerAddr)
    {
        memcpy(mPeerAddr.mFields.m8, aPeerAddr.mFields.m8, sizeof(otIp6Address));
    }

    uint16_t GetPeerPort() { return mPeerPort; }
    void SetPeerPort(uint16_t aPeerPort) { mPeerPort = aPeerPort; }

    uint8_t GetSessionId() { return mSessionId; }
    void SetSessionId(uint8_t aSessionId) { mSessionId = aSessionId; }

    void SetTransferTime(uint32_t aTransferTime) { mTransferTime = aTransferTime; }
    uint32_t GetTransferTime() { return mTransferTime; }

    void SetPacketId(uint32_t aPacketId) { mPacketId = aPacketId; }
    uint32_t GetPacketId() { return mPacketId; }

    void SetSendInterval(uint32_t aSendInterval) { mSendInterval = aSendInterval; }
    uint32_t GetSendInterval() { return mSendInterval; }

    void SetFinTime(uint32_t aFinTime) { mFinTime = aFinTime; }
    uint32_t GetFinTime() { return mFinTime; }

    void SetFinOrAckCount(uint8_t aFinOrAckCount) { mFinOrAckCount = aFinOrAckCount; }
    uint8_t GetFinOrAckCount() { return mFinOrAckCount; }

    void SetContext(Perf &aPerf, Session &aSession) { mContext.mPerf = &aPerf; mContext.mSession = &aSession; }
    otPerfContext * GetContext() { return &mContext; }

    Session *GetNext() { return mNext; }
    void SetNext(Session *aSession) { mNext = aSession; }

    Stats *GetStats() { return &mStats; }

    uint32_t GetTransferTimeDt(uint32_t aTime)
    {
        return mTransferTime - aTime;
    }

    bool IsTransferTimeBeforeOrEqual(uint32_t aTime)
    {
       return (static_cast<int32_t>(aTime - mTransferTime) >= 0);
    }

    void BuildServerHeader(ServerHdr &aServerHdr);

    void UpdatePacketStats(ReportPacket &aPacket);

    void IncreasePacketId()
    {
        mPacketId++;
        if (mPacketId & 0x80000000)
        {
            mPacketId = 0;
        }
    }

    void DecreasePacketId()
    {
        if ((mPacketId - 1) & 0x80000000)
        {
            mPacketId--;
        }
    }

    void NegativePacketId()
    {
        mPacketId |= 0x80000000;
    }

    void SetSessionStartTime(uint32_t aSessionStartTime) { mSessionStartTime = aSessionStartTime; }
    uint32_t GetSessionStartTime() { return mSessionStartTime; }

    void SetSessionEndTime(uint32_t aSessionEndTime) { mSessionEndTime = aSessionEndTime; }
    uint32_t GetSessionEndTime() { return mSessionEndTime; }

    bool IsSessionEndTimeBeforeOrEqual(uint32_t aTime)
    {
       return (static_cast<int32_t>(aTime - mSessionEndTime) >= 0);
    }

    void SetIntervalStartTime(uint32_t aIntervalStartTime) { mIntervalStartTime = aIntervalStartTime; }
    uint32_t GetIntervalStartTime() { return mIntervalStartTime; }

    void SetIntervalEndTime(uint32_t aIntervalEndTime) { mIntervalEndTime = aIntervalEndTime; }
    uint32_t GetIntervalEndTime() { return mIntervalEndTime; }

private:

    uint8_t        mType;
    uint8_t        mState;
    uint8_t        mSessionId;
    uint8_t        mFinOrAckCount;
    Setting       *mSetting;

    otUdpSocket    mSocket;          ///>
    otIp6Address   mLocalAddr;
    uint16_t       mLocalPort;
    otIp6Address   mPeerAddr;
    uint16_t       mPeerPort;


    uint32_t       mTransferTime;
    uint32_t       mSessionStartTime;
    uint32_t       mSessionEndTime;
    uint32_t       mIntervalStartTime;
    uint32_t       mIntervalEndTime;

    uint32_t       mFinTime;

    uint32_t       mPacketId;
    uint32_t       mSendInterval;    ///>
    Stats          mStats;

    otPerfContext  mContext;
    Session       *mNext;
};

/**
 * This class implements a CLI-based UDP example.
 *
 */
class Perf
{
public:

    /**
     * Constructor
     *
     * @param[in]  aInterpreter  The CLI interpreter.
     *
     */
    Perf(Instance *aInstance);

    /**
     * This method interprets a list of CLI arguments.
     *
     * @param[in]  argc  The number of elements in argv.
     * @param[in]  argv  A pointer to an array of command line arguments.
     *
     */
    otError Process(int argc, char *argv[], Server &aServer);

private:
    struct Command
    {
        const char *mName;
        otError(Perf::*mCommand)(int argc, char *argv[]);
    };

    enum
    {
        kSyncModeUninit = 0,
        kSyncModeMaster = 1,
        kSyncModeSlave  = 2,
    };

    enum
    {
        //kNumSettings      = 5,  ///< The number of the Perf settings.
        //kNumSession       = 6,
        kNumSettings      = 10,  ///< The number of the Perf settings.
        kNumSession       = 10,
        kMaxNumFin        = 20,
        kMaxNumAckFin     = 20,
        kMinSendInterval  = 2000,
        kFinInterval      = 250000,
        kAckFinInterval   = 250000,
        kSyncInterval     = 5000,
        kMaxPayloadLength = Ip6::Ip6::kMaxDatagramLength - sizeof(Ip6::Header) - sizeof(Ip6::UdpHeader),
    };

    otError SetClientSetting(Setting &aSetting, int argc, char *argv[]);
    otError SetServerSetting(Setting &aSetting, int argc, char *argv[]);
    otError ProcessHelp(int argc, char *argv[]);
    otError ProcessClient(int argc, char *argv[]);
    otError ProcessServer(int argc, char *argv[]);
    otError ProcessStart(int argc, char *argv[]);
    otError ProcessStop(int argc, char *argv[]);
    otError ProcessSync(int argc, char *argv[]);
    otError ProcessShow(int argc, char *argv[]);
    otError ProcessClear(int argc, char *argv[]);

    void SessionStop(uint8_t aType);
    otError ServerStart();
    otError ClientStart();
    otError ServerStop();
    otError ClientStop();
    void UpdateClientState();

    otError OpenSocket(Session &aSession);
    otError CloseSocket(Session &aSession);

    static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);
    void HandleUdpReceive(otMessage *aMessage, const otMessageInfo *aMessageInfo, Session &aSession);

    otError HandleServerMsg(otMessage &aMessage, const otMessageInfo &aMessageInfo, Session &aSession);
    otError HandleConnectMsg(otMessage &aMessage, const otMessageInfo &aMessageInfo, Session &aSession);
    otError HandleClientMsg(otMessage &aMessage, const otMessageInfo &aMessageInfo, Session &aSession);

    otError SendData(Session &aSession);
    otError SendReply(Session &aSession, UdpData &aData, otMessagePriority aPriority, uint16_t aLength);
    otError SendFin(Session &aSession);
    otError SendAckFin(Session &aSession);

    void PrintReport(Report &aReport, bool aIsServer);
    void PrintConnection(Session &aSession);
    void PrintClientReportHeader(Session &aSession);
    void PrintClientReport(Session &aSession);
    void PrintClientReportEnd(Session &aSession);
    void PrintServerStats(Session &aSession, ServerHdr &aServerHdr);

    void PrintServerReportHeader(Session &aSession);
    void PrintServerReport(Session &aSession);
    void PrintServerReportEnd(Session &aSession);

    void PrintSetting(Setting &aSetting);

    Session *NewSession(Setting &aSetting);
    void FreeSession(Session &aSession);
    Session *FindSession(const otMessageInfo &aMessageInfo);

    static Perf &GetOwner(OwnerLocator &aOwnerLocator);

    Setting *NewSetting();
    void FreeSetting(Setting &aSetting);

    otError FindMinTransferInterval(uint32_t &aInterval);
    void StartTransferTimer();

    static void HandleSyncTimer(Timer &aTimer);
    void HandleSyncTimer();

    static void HandleTransferTimer(Timer &aTimer);
    void HandleTransferTimer();

    static void HandleSyncEvent(void *aContext);
    void HandleSyncEvent();

    static const Command sCommands[];

    bool         mServerRunning         : 1;
    bool         mClientRunning         : 1;
    bool         mPrintServerHeaderFlag : 1;
    bool         mPrintClientHeaderFlag : 1;
    uint8_t      mSyncMode              : 2;
    uint8_t      mSyncCnt               : 1;

    uint32_t     mSyncTime;

    Instance    *mInstance;
    Server      *mServer; 

    TimerMicro   mTransferTimer;
    TimerMilli   mSyncTimer;

    Session      mSessions[kNumSession];
    Session     *mSession;
    Setting      mSettings[kNumSettings];
};

}  // namespace Cli
}  // namespace ot

#endif  // CLI_PERF_HPP_
