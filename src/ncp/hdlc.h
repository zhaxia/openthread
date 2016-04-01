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

#ifndef HDLC_H_
#define HDLC_H_

#include <common/message.h>
#include <common/thread_error.h>

namespace Thread {

class Hdlc
{
public:
    typedef void (*ReceiveHandler)(void *context, uint8_t protocol, uint8_t *frame, uint16_t frame_length);
    typedef void (*SendDoneHandler)(void *context);
    typedef void (*SendMessageDoneHandler)(void *context);

    static ThreadError Init(void *context, ReceiveHandler receive_handler, SendDoneHandler send_done_handler,
                            SendMessageDoneHandler send_message_done_handler);
    static ThreadError Start();
    static ThreadError Stop();

    static ThreadError Send(uint8_t protocol, uint8_t *frame, uint16_t frame_length);
    static ThreadError SendMessage(uint8_t protocol, Message &message);
};

}  // namespace Thread

#endif  // HDLC_H_
