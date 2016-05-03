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
 *   This file implements an HDLC interface to the Thread stack.
 */

#include <common/code_utils.hpp>
#include <ncp/ncp.hpp>
#include <platform/serial.h>

namespace Thread {

static Tasklet sSendDoneTask(&Ncp::SendDoneTask, NULL);
static Tasklet sReceiveTask(&Ncp::ReceiveTask, NULL);
static Ncp *sNcp;

Ncp::Ncp():
    NcpBase()
{
}

ThreadError Ncp::Init()
{
    super_t::Init();
    mHdlcDecoder.Init(mReceiveFrame, sizeof(mReceiveFrame), &HandleFrame, this);
    sNcp = this;

    return kThreadError_None;
}

ThreadError Ncp::Start()
{
    otSerialEnable();
    return super_t::Start();
}

ThreadError Ncp::Stop()
{
    otSerialDisable();
    return super_t::Stop();
}

ThreadError Ncp::Send(uint8_t protocol, uint8_t *frame,
                      uint16_t frameLength)
{
    uint8_t *cur = mSendFrame;
    uint16_t outLength;

    outLength = sizeof(mSendFrame) - (cur - mSendFrame);
    mHdlcEncoder.Init(cur, outLength);
    cur += outLength;

    outLength = sizeof(mSendFrame) - (cur - mSendFrame);
    mHdlcEncoder.Encode(&protocol, sizeof(protocol), cur, outLength);
    cur += outLength;

    outLength = sizeof(mSendFrame) - (cur - mSendFrame);
    mHdlcEncoder.Encode(frame, frameLength, cur, outLength);
    cur += outLength;

    outLength = sizeof(mSendFrame) - (cur - mSendFrame);
    mHdlcEncoder.Finalize(cur, outLength);
    cur += outLength;

    return otSerialSend(mSendFrame, cur - mSendFrame);
}

/// TODO: queue
ThreadError Ncp::SendMessage(uint8_t protocol, Message &message)
{
    uint8_t *cur = mSendFrame;
    uint16_t outLength;
    uint16_t inLength;
    uint8_t inBuf[16];

    outLength = sizeof(mSendFrame) - (cur - mSendFrame);
    mHdlcEncoder.Init(cur, outLength);
    cur += outLength;

    outLength = sizeof(mSendFrame) - (cur - mSendFrame);
    mHdlcEncoder.Encode(&protocol, sizeof(protocol), cur, outLength);
    cur += outLength;


    for (int offset = 0; offset < message.GetLength(); offset += sizeof(inBuf))
    {
        inLength = message.Read(offset, sizeof(inBuf), inBuf);
        outLength = sizeof(mSendFrame) - (cur - mSendFrame);
        mHdlcEncoder.Encode(inBuf, inLength, cur, outLength);
        cur += outLength;
    }

    outLength = sizeof(mSendFrame) - (cur - mSendFrame);
    mHdlcEncoder.Finalize(cur, outLength);
    cur += outLength;

    return otSerialSend(mSendFrame, cur - mSendFrame);
}

extern "C" void otSerialSignalSendDone()
{
    sSendDoneTask.Post();
}

void Ncp::SendDoneTask(void *context)
{
    sNcp->SendDoneTask();
}

void Ncp::SendDoneTask()
{
    if (mSendMessage == NULL)
    {
        super_t::HandleSendDone();
    }
    else
    {
        mSendMessage = NULL;
        super_t::HandleSendMessageDone();
    }

}

extern "C" void otSerialSignalReceive()
{
    sReceiveTask.Post();
}

void Ncp::ReceiveTask(void *context)
{
    sNcp->ReceiveTask();
}

void Ncp::ReceiveTask()
{
    const uint8_t *buf;
    uint16_t bufLength;

    buf = otSerialGetReceivedBytes(&bufLength);

    mHdlcDecoder.Decode(buf, bufLength);

    otSerialHandleReceiveDone();
}

void Ncp::HandleFrame(void *context, uint8_t *aBuf, uint16_t aBufLength)
{
    sNcp->HandleFrame(aBuf, aBufLength);
}

void Ncp::HandleFrame(uint8_t *aBuf, uint16_t aBufLength)
{
    super_t::HandleReceive(aBuf[0], aBuf + 1, aBufLength - 1);
}

}  // namespace Thread
