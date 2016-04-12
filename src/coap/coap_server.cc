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

#include <coap/coap_server.h>
#include <common/code_utils.h>
#include <common/thread_error.h>

namespace Thread {
namespace Coap {

Server::Server(uint16_t port):
    m_socket(&HandleUdpReceive, this)
{
    m_port = port;
}

ThreadError Server::Start()
{
    ThreadError error;
    struct sockaddr_in6 sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin6_port = m_port;
    SuccessOrExit(error = m_socket.Bind(&sockaddr));

exit:
    return error;
}

ThreadError Server::Stop()
{
    return m_socket.Close();
}

ThreadError Server::AddResource(Resource &resource)
{
    ThreadError error = kThreadError_None;

    for (Resource *cur = m_resources; cur; cur = cur->m_next)
    {
        VerifyOrExit(cur != &resource, error = kThreadError_Busy);
    }

    resource.m_next = m_resources;
    m_resources = &resource;

exit:
    return error;
}

void Server::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    Server *obj = reinterpret_cast<Server *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void Server::HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info)
{
    Header header;
    char uri_path[32];
    char *cur_uri_path = uri_path;
    const Header::Option *coap_option;

    SuccessOrExit(header.FromMessage(message));
    message.MoveOffset(header.GetLength());

    coap_option = header.GetCurrentOption();

    while (coap_option != NULL)
    {
        switch (coap_option->number)
        {
        case Header::Option::kOptionUriPath:
            VerifyOrExit(coap_option->length < sizeof(uri_path) - (cur_uri_path - uri_path), ;);
            memcpy(cur_uri_path, coap_option->value, coap_option->length);
            cur_uri_path[coap_option->length] = '/';
            cur_uri_path += coap_option->length + 1;
            break;

        case Header::Option::kOptionContentFormat:
            break;

        default:
            ExitNow();
        }

        coap_option = header.GetNextOption();
    }

    cur_uri_path[-1] = '\0';

    for (Resource *resource = m_resources; resource; resource = resource->m_next)
    {
        if (strcmp(resource->m_uri_path, uri_path) == 0)
        {
            resource->m_handler(resource->m_context, header, message, message_info);
            ExitNow();
        }
    }

exit:
    {}
}

ThreadError Server::SendMessage(Message &message, const Ip6MessageInfo &message_info)
{
    return m_socket.SendTo(message, message_info);
}

}  // namespace Coap
}  // namespace Thread
