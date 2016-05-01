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
 *   This file implements the CoAP server message dispatch.
 */

#include <coap/coap_server.hpp>
#include <common/code_utils.hpp>
#include <common/thread_error.hpp>

namespace Thread {
namespace Coap {

Server::Server(uint16_t aPort)
{
    mPort = aPort;
    mResources = NULL;
}

ThreadError Server::Start()
{
    ThreadError error;
    Ip6::SockAddr sockaddr = {};
    sockaddr.mPort = mPort;

    SuccessOrExit(error = mSocket.Open(&HandleUdpReceive, this));
    SuccessOrExit(error = mSocket.Bind(sockaddr));

exit:
    return error;
}

ThreadError Server::Stop()
{
    return mSocket.Close();
}

ThreadError Server::AddResource(Resource &aResource)
{
    ThreadError error = kThreadError_None;

    for (Resource *cur = mResources; cur; cur = cur->mNext)
    {
        VerifyOrExit(cur != &aResource, error = kThreadError_Busy);
    }

    aResource.mNext = mResources;
    mResources = &aResource;

exit:
    return error;
}

void Server::HandleUdpReceive(void *aContext, otMessage aMessage, const otMessageInfo *aMessageInfo)
{
    Server *obj = reinterpret_cast<Server *>(aContext);
    obj->HandleUdpReceive(*static_cast<Message *>(aMessage), *static_cast<const Ip6::MessageInfo *>(aMessageInfo));
}

void Server::HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    Header header;
    char uriPath[kMaxReceivedUriPath];
    char *curUriPath = uriPath;
    const Header::Option *coapOption;

    SuccessOrExit(header.FromMessage(aMessage));
    aMessage.MoveOffset(header.GetLength());

    coapOption = header.GetCurrentOption();

    while (coapOption != NULL)
    {
        switch (coapOption->mNumber)
        {
        case Header::Option::kOptionUriPath:
            VerifyOrExit(coapOption->mLength < sizeof(uriPath) - (curUriPath - uriPath), ;);
            memcpy(curUriPath, coapOption->mValue, coapOption->mLength);
            curUriPath[coapOption->mLength] = '/';
            curUriPath += coapOption->mLength + 1;
            break;

        case Header::Option::kOptionContentFormat:
            break;

        default:
            ExitNow();
        }

        coapOption = header.GetNextOption();
    }

    curUriPath[-1] = '\0';

    for (Resource *resource = mResources; resource; resource = resource->mNext)
    {
        if (strcmp(resource->mUriPath, uriPath) == 0)
        {
            resource->HandleRequest(header, aMessage, aMessageInfo);
            ExitNow();
        }
    }

exit:
    {}
}

ThreadError Server::SendMessage(Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    return mSocket.SendTo(aMessage, aMessageInfo);
}

}  // namespace Coap
}  // namespace Thread
