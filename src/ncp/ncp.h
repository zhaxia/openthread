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

#ifndef NCP_NCP_H_
#define NCP_NCP_H_

#include <ncp/ncp_base.h>
#include <ncp/hdlc.h>

namespace Thread {

class Ncp : public NcpBase
{
    typedef NcpBase super_t;

public:
    Ncp();

    ThreadError Init() final;
    ThreadError Start() final;
    ThreadError Stop() final;

    ThreadError SendMessage(uint8_t protocol, Message &message) final;
    ThreadError Send(uint8_t protocol, uint8_t *frame, 
		     uint16_t frame_length) final;

private:
    Hdlc m_hdlc;
};

}  // namespace Thread

#endif  // NCP_NCP_H_
