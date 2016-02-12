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

#ifndef COAP_COAP_SERVER_H_
#define COAP_COAP_SERVER_H_

#include <coap/coap_message.h>
#include <common/message.h>
#include <net/udp6.h>

namespace Thread {

class CoapServer {
 public:
  class Resource {
    friend CoapServer;

   public:
    typedef void (*HandleCoapMessage)(void *context, CoapMessage *coap, Message *message,
                                      const Ip6MessageInfo *message_info);
    Resource(const char *uri_path, HandleCoapMessage callback, void *context) {
      uri_path_ = uri_path;
      callback_ = callback;
      context_ = context;
    }

   private:
    const char *uri_path_;
    HandleCoapMessage callback_;
    void *context_;
    Resource *next_ = NULL;
  };

  explicit CoapServer(uint16_t port);
  ThreadError Start();
  ThreadError Stop();
  ThreadError AddResource(Resource *resource);
  ThreadError SendMessage(Message *message, const Ip6MessageInfo *message_info);

 private:
  static void RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info);
  void RecvFrom(Message *message, const Ip6MessageInfo *message_info);

  Udp6Socket socket_;
  uint16_t port_;
  Resource *resources_ = NULL;
};

}  // namespace Thread

#endif  // COAP_COAP_SERVER_H_
