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
#include <platform/phy.hpp>

#ifdef __cplusplus
extern "C" {
#endif

extern struct gengetopt_args_info args_info;

namespace Thread {

extern void phy_handle_transmit_done(PhyPacket *packet, bool rx_pending, ThreadError error);
extern void phy_handle_receive_done(PhyPacket *packet, ThreadError error);

static void *phy_receive_thread(void *arg);
static void phy_received_task(void *context);
static void phy_sent_task(void *context);

static PhyState s_state = kStateDisabled;
static PhyPacket *s_receive_frame = NULL;
static PhyPacket *s_transmit_frame = NULL;
static PhyPacket m_ack_packet;
static bool s_data_pending = false;

static uint8_t s_extended_address[8];
static uint16_t s_short_address;
static uint16_t s_panid;

static pthread_t s_pthread;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_condition_variable = PTHREAD_COND_INITIALIZER;
static int s_sockfd;

static Tasklet s_received_task(&phy_received_task, NULL);
static Tasklet s_sent_task(&phy_sent_task, NULL);

ThreadError phy_set_pan_id(uint16_t panid)
{
    s_panid = panid;
    return kThreadError_None;
}

ThreadError phy_set_extended_address(uint8_t *address)
{
    for (unsigned i = 0; i < sizeof(s_extended_address); i++)
    {
        s_extended_address[i] = address[7 - i];
    }

    return kThreadError_None;
}

ThreadError phy_set_short_address(uint16_t address)
{
    s_short_address = address;
    return kThreadError_None;
}

ThreadError phy_init()
{
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(9000 + args_info.nodeid_arg);
    sockaddr.sin_addr.s_addr = INADDR_ANY;

    s_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bind(s_sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));

    pthread_create(&s_pthread, NULL, &phy_receive_thread, NULL);

    return kThreadError_None;
}

ThreadError phy_start()
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

ThreadError phy_stop()
{
    pthread_mutex_lock(&s_mutex);
    s_state = kStateDisabled;
    pthread_cond_signal(&s_condition_variable);
    pthread_mutex_unlock(&s_mutex);
    return kThreadError_None;
}

ThreadError phy_sleep()
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

ThreadError phy_idle()
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

ThreadError phy_receive(PhyPacket *packet)
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

ThreadError phy_transmit(PhyPacket *packet)
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
        sendto(s_sockfd, s_transmit_frame->m_psdu, s_transmit_frame->m_length, 0,
               (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    }

    if (!reinterpret_cast<Mac::Frame *>(s_transmit_frame)->GetAckRequest())
    {
        s_sent_task.Post();
    }

exit:
    pthread_mutex_unlock(&s_mutex);
    return error;
}

PhyState phy_get_state()
{
    PhyState state;
    pthread_mutex_lock(&s_mutex);
    state = s_state;
    pthread_mutex_unlock(&s_mutex);
    return state;
}

int8_t phy_get_noise_floor()
{
    return 0;
}

void phy_sent_task(void *context)
{
    PhyState state;
    pthread_mutex_lock(&s_mutex);
    state = s_state;

    if (s_state != kStateDisabled)
    {
        s_state = kStateIdle;
    }

    pthread_cond_signal(&s_condition_variable);
    pthread_mutex_unlock(&s_mutex);

    if (state != kStateDisabled)
    {
        assert(state == kStateTransmit);
        phy_handle_transmit_done(s_transmit_frame, s_data_pending, kThreadError_None);
    }
}

void *phy_receive_thread(void *arg)
{
    fd_set fds;
    int rval;
    PhyPacket receive_frame;
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
            length = recvfrom(s_sockfd, receive_frame.m_psdu, Mac::Frame::kMTU, 0, NULL, NULL);

            if (length < 0)
            {
                dprintf("recvfrom error\n");
                assert(false);
            }

            if (reinterpret_cast<Mac::Frame *>(&receive_frame)->GetType() != Mac::Frame::kFcfFrameAck)
            {
                break;
            }

            reinterpret_cast<Mac::Frame *>(s_transmit_frame)->GetSequence(tx_sequence);

            reinterpret_cast<Mac::Frame *>(&receive_frame)->GetSequence(rx_sequence);

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
            s_sent_task.Post();
            break;

        case kStateListen:
            s_state = kStateReceive;
            s_received_task.Post();

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

    reinterpret_cast<Mac::Frame *>(s_receive_frame)->GetSequence(sequence);

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
        sendto(s_sockfd, m_ack_packet.m_psdu, m_ack_packet.m_length, 0,
               (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    }

    dprintf("Sent ack %d\n", sequence);
}

void phy_received_task(void *context)
{
    ThreadError error = kThreadError_None;
    Mac::Frame *receive_frame;
    uint16_t dstpan;
    Mac::Address dstaddr;
    int length;
    PhyState state;

    length = recvfrom(s_sockfd, s_receive_frame->m_psdu, Mac::Frame::kMTU, 0, NULL, NULL);
    receive_frame = reinterpret_cast<Mac::Frame *>(s_receive_frame);

    receive_frame->GetDstAddr(dstaddr);

    switch (dstaddr.length)
    {
    case 0:
        break;

    case 2:
        receive_frame->GetDstPanId(dstpan);
        VerifyOrExit((dstpan == Mac::kShortAddrBroadcast || dstpan == s_panid) &&
                     (dstaddr.address16 == Mac::kShortAddrBroadcast || dstaddr.address16 == s_short_address),
                     error = kThreadError_Abort);
        break;

    case 8:
        receive_frame->GetDstPanId(dstpan);
        VerifyOrExit((dstpan == Mac::kShortAddrBroadcast || dstpan == s_panid) &&
                     memcmp(&dstaddr.address64, s_extended_address, sizeof(dstaddr.address64)) == 0,
                     error = kThreadError_Abort);
        break;

    default:
        ExitNow(error = kThreadError_Abort);
    }

    s_receive_frame->m_length = length;
    s_receive_frame->m_power = -20;

    // generate acknowledgment
    if (reinterpret_cast<Mac::Frame *>(s_receive_frame)->GetAckRequest())
    {
        send_ack();
    }

exit:
    pthread_mutex_lock(&s_mutex);
    state = s_state;

    if (s_state != kStateDisabled)
    {
        s_state = kStateIdle;
    }

    pthread_cond_signal(&s_condition_variable);
    pthread_mutex_unlock(&s_mutex);

    if (state != kStateDisabled)
    {
        assert(state == kStateReceive);
        phy_handle_receive_done(s_receive_frame, error);
    }
}

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread
