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

#include <coap/coap_server.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/thread_error.h>

using Thread::CoapMessage;
using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

CoapServer::CoapServer(uint16_t port):
    socket_(&RecvFrom, this) {
  port_ = port;
}

ThreadError CoapServer::Start() {
  ThreadError error;

  struct sockaddr_in6 sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin6_port = HostSwap16(port_);
  SuccessOrExit(error = socket_.Bind(&sockaddr));

exit:
  return error;
}

ThreadError CoapServer::Stop() {
  return socket_.Close();
}

ThreadError CoapServer::AddResource(Resource *resource) {
  ThreadError error = kThreadError_None;

  for (Resource *cur = resources_; cur; cur = cur->next_)
    VerifyOrExit(cur != resource, error = kThreadError_Busy);

  resource->next_ = resources_;
  resources_ = resource;

exit:
  return error;
}

void CoapServer::RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info) {
  CoapServer *obj = reinterpret_cast<CoapServer*>(context);
  obj->RecvFrom(message, message_info);
}

void CoapServer::RecvFrom(Message *message, const Ip6MessageInfo *message_info) {
  CoapMessage coap;
  char uri_path[32];
  char *cur_uri_path = uri_path;

  SuccessOrExit(coap.FromMessage(message));
  message->MoveOffset(coap.GetHeaderLength());

  const CoapMessage::Option *coap_option;
  coap_option = coap.GetCurrentOption();
  while (coap_option != NULL) {
    switch (coap_option->number) {
      case CoapMessage::Option::kOptionUriPath:
        VerifyOrExit(coap_option->length < sizeof(uri_path) - (cur_uri_path - uri_path), ;);
        memcpy(cur_uri_path, coap_option->value, coap_option->length);
        cur_uri_path[coap_option->length] = '/';
        cur_uri_path += coap_option->length + 1;
        break;
      case CoapMessage::Option::kOptionContentFormat:
        break;
      default:
        ExitNow();
    }
    coap_option = coap.GetNextOption();
  }
  cur_uri_path[-1] = '\0';

  for (Resource *resource = resources_; resource; resource = resource->next_) {
    if (strcmp(resource->uri_path_, uri_path) == 0) {
      resource->callback_(resource->context_, &coap, message, message_info);
      ExitNow();
    }
  }

exit:
  {}
}

ThreadError CoapServer::SendMessage(Thread::Message *message, const Ip6MessageInfo *message_info) {
  return socket_.SendTo(message, message_info);
}

}  // namespace Thread
