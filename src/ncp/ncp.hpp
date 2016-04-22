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
 *   This file contains definitions for an HDLC interface to the OpenThread stack.
 */

#ifndef NCP_HPP_
#define NCP_HPP_

#include <ncp/ncp_base.hpp>
#include <ncp/hdlc.hpp>

namespace Thread {

class Ncp : public NcpBase
{
    typedef NcpBase super_t;

public:
    Ncp();

    ThreadError Init() final;
    ThreadError Start() final;
    ThreadError Stop() final;

    ThreadError Send(uint8_t protocol, uint8_t *frame, uint16_t frameLength) final;
    ThreadError SendMessage(uint8_t protocol, Message &message) final;

    static void HandleFrame(void *context, uint8_t *aBuf, uint16_t aBufLength);
    static void SendDoneTask(void *context);
    static void ReceiveTask(void *context);

private:
    void HandleFrame(uint8_t *aBuf, uint16_t aBufLength);
    void SendDoneTask();
    void ReceiveTask();

    Hdlc::Encoder mHdlcEncoder;
    Hdlc::Decoder mHdlcDecoder;

    uint8_t mSendFrame[512];
    uint8_t mReceiveFrame[512];
    Message *mSendMessage;
};

}  // namespace Thread

#endif  // NCP_HPP_
