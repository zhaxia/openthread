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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#include <ncp/ncp.h>
#include <common/code_utils.h>

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
ThreadError Ncp::SendMessage(uint8_t protocol, Message &message) {
  return m_hdlc.SendMessage(protocol, message);
}


}  // namespace Thread
