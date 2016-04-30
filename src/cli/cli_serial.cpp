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
 *   This file implements the CLI server on the serial service.
 */

#include <stdio.h>
#include <string.h>

#include <cli/cli_serial.hpp>
#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/tasklet.hpp>
#include <platform/serial.h>

namespace Thread {
namespace Cli {

static const uint8_t VT102_ERASE_EOL[] = "\033[K";
static const uint8_t CRNL[] = "\r\n";
static Serial *sServer;

static Tasklet sReceiveTask(&Serial::ReceiveTask, NULL);

Serial::Serial()
{
    sServer = this;
}

ThreadError Serial::Start()
{
    mRxLength = 0;
    otSerialEnable();
    return kThreadError_None;
}

extern "C" void otSerialSignalSendDone(void)
{
}

extern "C" void otSerialSignalReceive(void)
{
    sReceiveTask.Post();
}

void Serial::ReceiveTask(void *aContext)
{
    sServer->ReceiveTask();
}

void Serial::ReceiveTask()
{
    uint16_t bufLength;
    const uint8_t *buf;
    const uint8_t *cur;
    const uint8_t *end;

    buf = otSerialGetReceivedBytes(&bufLength);

    cur = buf;
    end = cur + bufLength;

    for (; cur < end; cur++)
    {
        switch (*cur)
        {
        case '\r':
            otSerialSend(CRNL, sizeof(CRNL));
            break;

        default:
            otSerialSend(cur, 1);
            break;
        }
    }

    while (bufLength > 0 && mRxLength < kRxBufferSize)
    {
        switch (*buf)
        {
        case '\r':
        case '\n':
            if (mRxLength > 0)
            {
                mRxBuffer[mRxLength] = '\0';
                ProcessCommand();
            }

            break;

        case '\b':
        case 127:
            if (mRxLength > 0)
            {
                mRxBuffer[--mRxLength] = '\0';
                otSerialSend(VT102_ERASE_EOL, sizeof(VT102_ERASE_EOL));
            }

            break;

        default:
            mRxBuffer[mRxLength++] = *buf;
            break;
        }

        buf++;
        bufLength--;
    }

    otSerialHandleReceiveDone();
}

ThreadError Serial::ProcessCommand()
{
    ThreadError error = kThreadError_None;

    if (mRxBuffer[mRxLength - 1] == '\n')
    {
        mRxBuffer[--mRxLength] = '\0';
    }

    if (mRxBuffer[mRxLength - 1] == '\r')
    {
        mRxBuffer[--mRxLength] = '\0';
    }

    ProcessLine(buf, length, *this);

exit:
    mRxLength = 0;

    return error;
}

ThreadError Serial::Output(const char *aBuf, uint16_t aBufLength)
{
    return otSerialSend(reinterpret_cast<const uint8_t *>(aBuf), aBufLength);
}

}  // namespace Cli
}  // namespace Thread
