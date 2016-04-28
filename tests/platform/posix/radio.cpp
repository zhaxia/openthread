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
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <platform/posix/cmdline.h>

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>
#include <mac/mac.hpp>
#include <platform/radio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct gengetopt_args_info args_info;

namespace Thread {

enum PhyState
{
    kStateDisabled = 0,
    kStateSleep = 1,
    kStateIdle = 2,
    kStateListen = 3,
    kStateReceive = 4,
    kStateTransmit = 5,
};

static void *phy_receive_thread(void *arg);

static PhyState s_state = kStateDisabled;
static RadioPacket *s_receive_frame = NULL;
static RadioPacket *s_transmit_frame = NULL;
static RadioPacket m_ack_packet;
static bool s_data_pending = false;

static uint8_t s_extended_address[8];
static uint16_t s_short_address;
static uint16_t s_panid;

static pthread_t s_pthread;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_condition_variable = PTHREAD_COND_INITIALIZER;
static int s_sockfd;

ThreadError otRadioSetPanId(uint16_t panid)
{
    s_panid = panid;
    return kThreadError_None;
}

ThreadError otRadioSetExtendedAddress(uint8_t *address)
{
    for (unsigned i = 0; i < sizeof(s_extended_address); i++)
    {
        s_extended_address[i] = address[7 - i];
    }

    return kThreadError_None;
}

ThreadError otRadioSetShortAddress(uint16_t address)
{
    s_short_address = address;
    return kThreadError_None;
}

void otRadioInit()
{
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(9000 + args_info.nodeid_arg);
    sockaddr.sin_addr.s_addr = INADDR_ANY;

    s_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(s_sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));

    pthread_create(&s_pthread, NULL, &phy_receive_thread, NULL);
}

ThreadError otRadioEnable()
{
    ThreadError error = kThreadError_None;

    pthread_mutex_lock(&s_mutex);
    VerifyOrExit(s_state == kStateDisabled, error = kThreadError_Busy);
    s_state = kStateSleep;
    pthread_cond_signal(&s_condition_variable);

exit:
    pthread_mutex_unlock(&s_mutex);
    return error;
}

ThreadError otRadioDisable()
{
    pthread_mutex_lock(&s_mutex);
    s_state = kStateDisabled;
    pthread_cond_signal(&s_condition_variable);
    pthread_mutex_unlock(&s_mutex);
    return kThreadError_None;
}

ThreadError otRadioSleep()
{
    ThreadError error = kThreadError_None;

    pthread_mutex_lock(&s_mutex);
    VerifyOrExit(s_state == kStateIdle, error = kThreadError_Busy);
    s_state = kStateSleep;
    pthread_cond_signal(&s_condition_variable);

exit:
    pthread_mutex_unlock(&s_mutex);
    return error;
}

ThreadError otRadioIdle()
{
    ThreadError error = kThreadError_None;

    pthread_mutex_lock(&s_mutex);

    switch (s_state)
    {
    case kStateSleep:
        s_state = kStateIdle;
        pthread_cond_signal(&s_condition_variable);
        break;

    case kStateIdle:
        break;

    case kStateListen:
    case kStateTransmit:
        s_state = kStateIdle;
        pthread_cond_signal(&s_condition_variable);
        break;

    case kStateDisabled:
    case kStateReceive:
        ExitNow(error = kThreadError_Busy);
    }

exit:
    pthread_mutex_unlock(&s_mutex);
    return error;
}

ThreadError otRadioReceive(RadioPacket *packet)
{
    ThreadError error = kThreadError_None;

    pthread_mutex_lock(&s_mutex);
    VerifyOrExit(s_state == kStateIdle, error = kThreadError_Busy);
    s_state = kStateListen;
    pthread_cond_signal(&s_condition_variable);

    s_receive_frame = packet;

exit:
    pthread_mutex_unlock(&s_mutex);
    return error;
}

ThreadError otRadioTransmit(RadioPacket *packet)
{
    ThreadError error = kThreadError_None;
    struct sockaddr_in sockaddr;

    pthread_mutex_lock(&s_mutex);
    VerifyOrExit(s_state == kStateIdle, error = kThreadError_Busy);
    s_state = kStateTransmit;
    pthread_cond_signal(&s_condition_variable);

    s_transmit_frame = packet;
    s_data_pending = false;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr);

    for (int i = 1; i < 34; i++)
    {
        if (args_info.nodeid_arg == i)
        {
            continue;
        }

        sockaddr.sin_port = htons(9000 + i);
        sendto(s_sockfd, s_transmit_frame->mPsdu, s_transmit_frame->mLength, 0,
               (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    }

    if (!reinterpret_cast<Mac::Frame *>(s_transmit_frame)->GetAckRequest())
    {
        otRadioSignalTransmitDone();
    }

exit:
    pthread_mutex_unlock(&s_mutex);
    return error;
}

int8_t otRadioGetNoiseFloor()
{
    return 0;
}

ThreadError otRadioHandleTransmitDone(bool *rxPending)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(s_state == kStateTransmit, error = kThreadError_InvalidState);

    pthread_mutex_lock(&s_mutex);
    s_state = kStateIdle;
    pthread_cond_signal(&s_condition_variable);
    pthread_mutex_unlock(&s_mutex);

    if (rxPending != NULL)
    {
        *rxPending = s_data_pending;
    }

exit:
    return error;
}

void *phy_receive_thread(void *arg)
{
    fd_set fds;
    int rval;
    RadioPacket receive_frame;
    int length;
    uint8_t tx_sequence, rx_sequence;
    uint8_t command_id;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(s_sockfd, &fds);

        rval = select(s_sockfd + 1, &fds, NULL, NULL, NULL);

        if (rval < 0 || !FD_ISSET(s_sockfd, &fds))
        {
            continue;
        }

        pthread_mutex_lock(&s_mutex);

        while (s_state == kStateIdle)
        {
            pthread_cond_wait(&s_condition_variable, &s_mutex);
        }

        switch (s_state)
        {
        case kStateDisabled:
        case kStateIdle:
        case kStateSleep:
            recvfrom(s_sockfd, NULL, 0, 0, NULL, NULL);
            break;

        case kStateTransmit:
            length = recvfrom(s_sockfd, receive_frame.mPsdu, Mac::Frame::kMTU, 0, NULL, NULL);

            if (length < 0)
            {
                dprintf("recvfrom error\n");
                assert(false);
            }

            if (reinterpret_cast<Mac::Frame *>(&receive_frame)->GetType() != Mac::Frame::kFcfFrameAck)
            {
                break;
            }

            tx_sequence = reinterpret_cast<Mac::Frame *>(s_transmit_frame)->GetSequence();

            rx_sequence = reinterpret_cast<Mac::Frame *>(&receive_frame)->GetSequence();

            if (tx_sequence != rx_sequence)
            {
                break;
            }

            if (reinterpret_cast<Mac::Frame *>(s_transmit_frame)->GetType() == Mac::Frame::kFcfFrameMacCmd)
            {
                reinterpret_cast<Mac::Frame *>(s_transmit_frame)->GetCommandId(command_id);

                if (command_id == Mac::Frame::kMacCmdDataRequest)
                {
                    s_data_pending = true;
                }
            }

            dprintf("Received ack %d\n", rx_sequence);
            otRadioSignalTransmitDone();
            break;

        case kStateListen:
            s_state = kStateReceive;
            otRadioSignalReceiveDone();

            while (s_state == kStateReceive)
            {
                pthread_cond_wait(&s_condition_variable, &s_mutex);
            }

            break;

        case kStateReceive:
            assert(false);
            break;
        }

        pthread_mutex_unlock(&s_mutex);
    }
}

void send_ack()
{
    Mac::Frame *ack_frame;
    uint8_t sequence;
    struct sockaddr_in sockaddr;

    sequence = reinterpret_cast<Mac::Frame *>(s_receive_frame)->GetSequence();

    ack_frame = reinterpret_cast<Mac::Frame *>(&m_ack_packet);
    ack_frame->InitMacHeader(Mac::Frame::kFcfFrameAck, Mac::Frame::kSecNone);
    ack_frame->SetSequence(sequence);

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr);

    for (int i = 1; i < 34; i++)
    {
        if (args_info.nodeid_arg == i)
        {
            continue;
        }

        sockaddr.sin_port = htons(9000 + i);
        sendto(s_sockfd, m_ack_packet.mPsdu, m_ack_packet.mLength, 0,
               (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    }

    dprintf("Sent ack %d\n", sequence);
}

ThreadError otRadioHandleReceiveDone()
{
    ThreadError error = kThreadError_None;
    Mac::Frame *receive_frame;
    uint16_t dstpan;
    Mac::Address dstaddr;
    int length;

    VerifyOrExit(s_state == kStateReceive, error = kThreadError_InvalidState);

    length = recvfrom(s_sockfd, s_receive_frame->mPsdu, Mac::Frame::kMTU, 0, NULL, NULL);
    receive_frame = reinterpret_cast<Mac::Frame *>(s_receive_frame);

    receive_frame->GetDstAddr(dstaddr);

    switch (dstaddr.mLength)
    {
    case 0:
        break;

    case 2:
        receive_frame->GetDstPanId(dstpan);
        VerifyOrExit((dstpan == Mac::kShortAddrBroadcast || dstpan == s_panid) &&
                     (dstaddr.mShortAddress == Mac::kShortAddrBroadcast || dstaddr.mShortAddress == s_short_address),
                     error = kThreadError_Abort);
        break;

    case 8:
        receive_frame->GetDstPanId(dstpan);
        VerifyOrExit((dstpan == Mac::kShortAddrBroadcast || dstpan == s_panid) &&
                     memcmp(&dstaddr.mExtAddress, s_extended_address, sizeof(dstaddr.mExtAddress)) == 0,
                     error = kThreadError_Abort);
        break;

    default:
        ExitNow(error = kThreadError_Abort);
    }

    s_receive_frame->mLength = length;
    s_receive_frame->mPower = -20;

    // generate acknowledgment
    if (reinterpret_cast<Mac::Frame *>(s_receive_frame)->GetAckRequest())
    {
        send_ack();
    }

exit:
    pthread_mutex_lock(&s_mutex);

    if (s_state != kStateDisabled)
    {
        s_state = kStateIdle;
    }

    pthread_cond_signal(&s_condition_variable);
    pthread_mutex_unlock(&s_mutex);

    return error;
}

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread
