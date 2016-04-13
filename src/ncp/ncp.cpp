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

#include <common/code_utils.h>
#include <ncp/ncp.h>

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
    return m_hdlc.Start();
}

ThreadError Ncp::Stop()
{
    super_t::Stop();
    return m_hdlc.Stop();
}

ThreadError Ncp::Send(uint8_t protocol, uint8_t *frame,
                      uint16_t frame_length)
{
    return m_hdlc.Send(protocol, frame, frame_length);
}

/// TODO: queue
ThreadError Ncp::SendMessage(uint8_t protocol, Message &message)
{
    return m_hdlc.SendMessage(protocol, message);
}


}  // namespace Thread
