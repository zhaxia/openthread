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

#include <cli/cli_command.hpp>
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

void Serial::ReceiveTask(void *context)
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
    uint16_t payloadLength = mRxLength;
    char *cmd;
    char *last;
    char *cur;
    char *end;
    int argc;
    char *argv[kMaxArgs];

    if (mRxBuffer[payloadLength - 1] == '\n')
    {
        mRxBuffer[--payloadLength] = '\0';
    }

    if (mRxBuffer[payloadLength - 1] == '\r')
    {
        mRxBuffer[--payloadLength] = '\0';
    }

    VerifyOrExit((cmd = strtok_r(mRxBuffer, " ", &last)) != NULL, ;);

    if (strncmp(cmd, "?", 1) == 0)
    {
        cur = mRxBuffer;
        end = cur + sizeof(mRxBuffer);

        snprintf(cur, end - cur, "%s", "Commands:\r\n");
        cur += strlen(cur);

        for (Command *command = mCommands; command; command = command->GetNext())
        {
            snprintf(cur, end - cur, "%s\r\n", command->GetName());
            cur += strlen(cur);
        }

        SuccessOrExit(error = otSerialSend(reinterpret_cast<const uint8_t *>(mRxBuffer), cur - mRxBuffer));
    }
    else
    {
        for (argc = 0; argc < kMaxArgs; argc++)
        {
            if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL)
            {
                break;
            }
        }

        for (Command *command = mCommands; command; command = command->GetNext())
        {
            if (strcmp(cmd, command->GetName()) == 0)
            {
                command->Run(argc, argv, *this);
                break;
            }
        }
    }

exit:
    mRxLength = 0;

    return error;
}

ThreadError Serial::Output(const char *buf, uint16_t bufLength)
{
    return otSerialSend(reinterpret_cast<const uint8_t *>(buf), bufLength);
}

}  // namespace Cli
}  // namespace Thread
