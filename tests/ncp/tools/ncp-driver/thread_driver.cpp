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

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cmdline.h>

#include <common/code_utils.hpp>
#include <common/debug.hpp>
#include <thread_driver.hpp>
#include <serial_port.hpp>

namespace Thread {

ThreadDriver::ThreadDriver():
    mHdlcDecoder(mSerialFrame, sizeof(mSerialFrame), &HandleFrame, this)
{
    ipc_fd_ = -1;
}

ThreadError ThreadDriver::Start()
{
    ThreadError error = kThreadError_None;

    // setup communication with NCP
    serial_enable();

    // setup tun interface
    tun_netif_.Open();

    // get interface name
    char name[80];
    tun_netif_.GetName(name, sizeof(name));

    // setup communication with thread-ctl
    int ipc_listen_fd;
    ipc_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un sockaddr_un;
    memset(&sockaddr_un, 0, sizeof(sockaddr_un));
    sockaddr_un.sun_family = AF_UNIX;
    snprintf(sockaddr_un.sun_path, sizeof(sockaddr_un.sun_path), "/tmp/thread-driver-%s", name);
    unlink(sockaddr_un.sun_path);

    VerifyOrExit(bind(ipc_listen_fd, reinterpret_cast<struct sockaddr *>(&sockaddr_un),
                      sizeof(sockaddr_un.sun_family) + strlen(sockaddr_un.sun_path) + 1) == 0, perror("ipc bind"));
    VerifyOrExit(listen(ipc_listen_fd, 1) == 0, perror("listen"));

    while (1)
    {
        fd_set fds;
        int fd;
        int maxfd = -1;

        FD_ZERO(&fds);

        fd = serial_get_fd();

        if (maxfd < fd)
        {
            maxfd = fd;
        }

        FD_SET(fd, &fds);

        fd = tun_netif_.GetFileDescriptor();

        if (maxfd < fd)
        {
            maxfd = fd;
        }

        FD_SET(fd, &fds);

        fd = ipc_listen_fd;

        if (maxfd < fd)
        {
            maxfd = fd;
        }

        FD_SET(fd, &fds);

        if (ipc_fd_ >= 0)
        {
            fd = ipc_fd_;

            if (maxfd < fd)
            {
                maxfd = fd;
            }

            FD_SET(fd, &fds);
        }

        int rval = select(maxfd + 1, &fds, NULL, NULL, NULL);

        if (rval >= 0)
        {
            if (FD_ISSET(ipc_listen_fd, &fds))
            {
                struct sockaddr_un from_sockaddr_un;
                socklen_t from_length = sizeof(from_sockaddr_un);
                ipc_fd_ = accept(ipc_listen_fd, reinterpret_cast<struct sockaddr *>(&from_sockaddr_un), &from_length);

                if (ipc_fd_ < 0)
                {
                    printf("%s\n", strerror(errno));
                }

                printf("accept %d\n", ipc_fd_);
            }
            else if (FD_ISSET(serial_get_fd(), &fds))
            {
                uint8_t buf[2048];
                uint16_t buf_length = sizeof(buf);

                serial_read(buf, buf_length);

                mHdlcDecoder.Decode(buf, buf_length);

                printf("serial read\n");
            }
            else if (FD_ISSET(tun_netif_.GetFileDescriptor(), &fds))
            {
                uint8_t buf[2048];
                int buf_length;
                buf_length = tun_netif_.Read(buf, sizeof(buf));

                Hdlc::Encoder encoder;
                uint8_t hdlc[4096];
                uint8_t *hdlcCur = hdlc;
                uint16_t hdlcLength;

                // header
                hdlcLength = sizeof(hdlc);
                encoder.Init(hdlcCur, hdlcLength);
                hdlcCur += hdlcLength;

                // protocol
                uint8_t protocol = 2;
                hdlcLength = sizeof(hdlc) - (hdlcCur - hdlc);
                encoder.Encode(&protocol, sizeof(protocol), hdlcCur, hdlcLength);
                hdlcCur += hdlcLength;

                // message
                hdlcLength = sizeof(hdlc) - (hdlcCur - hdlc);
                encoder.Encode(buf, buf_length, hdlcCur, hdlcLength);
                hdlcCur += hdlcLength;

                // footer
                hdlcLength = sizeof(hdlc) - (hdlcCur - hdlc);
                encoder.Finalize(hdlcCur, hdlcLength);
                hdlcCur += hdlcLength;

                serial_send(hdlc, hdlcCur - hdlc);
                printf("tun read\n");
            }
            else if (FD_ISSET(ipc_fd_, &fds))
            {
                uint8_t buf[1024];
                int buf_length = read(ipc_fd_, buf, sizeof(buf));

                if (buf_length <= 0)
                {
                    close(ipc_fd_);
                    ipc_fd_ = -1;
                }
                else
                {
                    Hdlc::Encoder encoder;
                    uint8_t hdlc[4096];
                    uint8_t *hdlcCur = hdlc;
                    uint16_t hdlcLength;

                    // header
                    hdlcLength = sizeof(hdlc);
                    encoder.Init(hdlcCur, hdlcLength);
                    hdlcCur += hdlcLength;

                    // protocol
                    uint8_t protocol = 0;
                    hdlcLength = sizeof(hdlc) - (hdlcCur - hdlc);
                    encoder.Encode(&protocol, sizeof(protocol), hdlcCur, hdlcLength);
                    hdlcCur += hdlcLength;

                    // message
                    hdlcLength = sizeof(hdlc) - (hdlcCur - hdlc);
                    encoder.Encode(buf, buf_length, hdlcCur, hdlcLength);
                    hdlcCur += hdlcLength;

                    // footer
                    hdlcLength = sizeof(hdlc) - (hdlcCur - hdlc);
                    encoder.Finalize(hdlcCur, hdlcLength);
                    hdlcCur += hdlcLength;

                    serial_send(hdlc, hdlcCur - hdlc);

                    printf("ipc read\n");
                }
            }
        }
    }

exit:
    return error;
}

void ThreadDriver::HandleReceive(void *context, uint8_t protocol, uint8_t *buf, uint16_t buf_length)
{
    ThreadDriver *obj = reinterpret_cast<ThreadDriver *>(context);
    obj->HandleReceive(protocol, buf, buf_length);
}

void ThreadDriver::HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length)
{

}

ThreadError ThreadDriver::ProcessThreadControl(uint8_t *buf, uint16_t buf_length)
{
    ThreadError error = kThreadError_None;
    ThreadControl thread_control;

    VerifyOrExit(thread_control__unpack(buf_length, reinterpret_cast<uint8_t *>(buf), &thread_control) != NULL,
                 error = kThreadError_Parse; printf("protobuf unpack error\n"));

    switch (thread_control.message_case)
    {
    case THREAD_CONTROL__MESSAGE_PRIMITIVE:
        ProcessPrimitive(&thread_control.primitive);
        break;

    case THREAD_CONTROL__MESSAGE_ADDRESSES:
        ProcessAddresses(&thread_control.addresses);
        break;

    default:
        break;
    }

exit:
    return error;
}

ThreadError ThreadDriver::ProcessPrimitive(ThreadPrimitive *primitive)
{
    switch (primitive->type)
    {
    case THREAD_PRIMITIVE__TYPE__THREAD_STATUS:
        if (primitive->bool_)
        {
            tun_netif_.Up();
        }
        else
        {
            tun_netif_.Down();
        }

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError ThreadDriver::ProcessAddresses(ThreadIp6Addresses *addresses)
{
    tun_netif_.SetIp6Addresses(addresses);
    return tun_netif_.SetIp6Addresses(addresses);
}

void ThreadDriver::HandleSendDone(void *context)
{
}

void ThreadDriver::HandleSendMessageDone(void *context)
{
}

void ThreadDriver::HandleFrame(void *context, uint8_t *buf, uint16_t buf_length)
{
    ThreadDriver *obj = reinterpret_cast<ThreadDriver *>(context);
    obj->HandleFrame(buf, buf_length);
}

void ThreadDriver::HandleFrame(uint8_t *buf, uint16_t buf_length)
{
    uint8_t protocol = buf[0];

    buf++;
    buf_length--;

    switch (protocol)
    {
    case 0:
        ProcessThreadControl(buf, buf_length);

        if (write(ipc_fd_, buf, buf_length) < 0)
        {
            printf("%s\n", strerror(errno));
        }

        break;

    case 1:
        ProcessThreadControl(buf, buf_length);
        break;

    case 2:
        tun_netif_.Write(buf, buf_length);
        break;
    }
}

}  // namespace Thread
