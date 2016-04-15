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

#include <common/code_utils.hpp>
#include <ncp/ncp.hpp>

namespace Thread {

Ncp::Ncp():
    NcpBase()
{
}

ThreadError Ncp::Init()
{
    super_t::Init();
    Hdlc::Init(this,
               &super_t::HandleReceive,
               &super_t::HandleSendDone,
               &super_t::HandleSendMessageDone);
    return kThreadError_None;
}

ThreadError Ncp::Start()
{
    super_t::Start();
    return mHdlc.Start();
}

ThreadError Ncp::Stop()
{
    super_t::Stop();
    return mHdlc.Stop();
}

ThreadError Ncp::Send(uint8_t protocol, uint8_t *frame,
                      uint16_t frameLength)
{
    return mHdlc.Send(protocol, frame, frameLength);
}

/// TODO: queue
ThreadError Ncp::SendMessage(uint8_t protocol, Message &message)
{
    return mHdlc.SendMessage(protocol, message);
}


}  // namespace Thread
