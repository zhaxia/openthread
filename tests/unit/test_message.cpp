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

#include "test_util.h"
#include <common/debug.hpp>
#include <common/message.hpp>
#include <string.h>

void TestMessage()
{
    Thread::Message *message;
    uint8_t writeBuffer[1024];
    uint8_t readBuffer[1024];

    Thread::Message::Init();

    for (unsigned i = 0; i < sizeof(writeBuffer); i++)
    {
        writeBuffer[i] = random();
    }

    VerifyOrQuit((message = Thread::Message::New(Thread::Message::kTypeIp6, 0)) != NULL,
                 "Message::New failed\n");
    SuccessOrQuit(message->SetLength(sizeof(writeBuffer)),
                  "Message::SetLength failed\n");
    VerifyOrQuit(message->Write(0, sizeof(writeBuffer), writeBuffer) == sizeof(writeBuffer),
                 "Message::Write failed\n");
    VerifyOrQuit(message->Read(0, sizeof(readBuffer), readBuffer) == sizeof(readBuffer),
                 "Message::Read failed\n");
    VerifyOrQuit(memcmp(writeBuffer, readBuffer, sizeof(writeBuffer)) == 0,
                 "Message compare failed\n");
    VerifyOrQuit(message->GetLength() == 1024,
                 "Message::GetLength failed\n");
    SuccessOrQuit(Thread::Message::Free(*message),
                  "Message::Free failed\n");
}

int main()
{
    TestMessage();
    printf("All tests passed\n");
    return 0;
}
