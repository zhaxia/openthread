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

#include <coap/coap_server.hpp>
#include <common/code_utils.hpp>
#include <common/thread_error.hpp>

namespace Thread {
namespace Coap {

Server::Server(uint16_t port):
    mSocket(&HandleUdpReceive, this)
{
    mPort = port;
}

ThreadError Server::Start()
{
    ThreadError error;
    struct sockaddr_in6 sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.mPort = mPort;
    SuccessOrExit(error = mSocket.Bind(&sockaddr));

exit:
    return error;
}

ThreadError Server::Stop()
{
    return mSocket.Close();
}

ThreadError Server::AddResource(Resource &resource)
{
    ThreadError error = kThreadError_None;

    for (Resource *cur = mResources; cur; cur = cur->mNext)
    {
        VerifyOrExit(cur != &resource, error = kThreadError_Busy);
    }

    resource.mNext = mResources;
    mResources = &resource;

exit:
    return error;
}

void Server::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &messageInfo)
{
    Server *obj = reinterpret_cast<Server *>(context);
    obj->HandleUdpReceive(message, messageInfo);
}

void Server::HandleUdpReceive(Message &message, const Ip6MessageInfo &messageInfo)
{
    Header header;
    char uriPath[32];
    char *curUriPath = uriPath;
    const Header::Option *coapOption;

    SuccessOrExit(header.FromMessage(message));
    message.MoveOffset(header.GetLength());

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
            resource->mHandler(resource->mContext, header, message, messageInfo);
            ExitNow();
        }
    }

exit:
    {}
}

ThreadError Server::SendMessage(Message &message, const Ip6MessageInfo &messageInfo)
{
    return mSocket.SendTo(message, messageInfo);
}

}  // namespace Coap
}  // namespace Thread
