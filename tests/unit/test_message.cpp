/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#include "test_util.h"
#include <common/debug.hpp>
#include <common/message.hpp>
#include <string.h>

void TestMessage()
{
    Thread::Message *message;
    uint8_t write_buffer[1024];
    uint8_t read_buffer[1024];

    Thread::Message::Init();

    for (int i = 0; i < sizeof(write_buffer); i++)
    {
        write_buffer[i] = random();
    }

    VerifyOrQuit((message = Thread::Message::New(Thread::Message::kTypeIp6, 0)) != NULL,
                 "Message::New failed\n");
    SuccessOrQuit(message->SetLength(sizeof(write_buffer)),
                  "Message::SetLength failed\n");
    VerifyOrQuit(message->Write(0, sizeof(write_buffer), write_buffer) == sizeof(write_buffer),
                 "Message::Write failed\n");
    VerifyOrQuit(message->Read(0, sizeof(read_buffer), read_buffer) == sizeof(read_buffer),
                 "Message::Read failed\n");
    VerifyOrQuit(memcmp(write_buffer, read_buffer, sizeof(write_buffer)) == 0,
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
