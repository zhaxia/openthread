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

#include <platform/posix/phy.h>
#include <platform/posix/cmdline.h>
#include <common/code_utils.h>
#include <common/thread_error.h>
#include <mac/mac.h>
#include <platform/common/phy.h>
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

extern struct gengetopt_args_info args_info;

namespace Thread {

uint8_t *PhyPacket::GetPsdu() {
  return psdu_;
}

uint8_t PhyPacket::GetPsduLength() const {
  return psdu_length_;
}

void PhyPacket::SetPsduLength(uint8_t psdu_length) {
  psdu_length_ = psdu_length;
}

uint8_t PhyPacket::GetChannel() const {
  return channel_;
}

void PhyPacket::SetChannel(uint8_t channel) {
  channel_ = channel;
}

int8_t PhyPacket::GetPower() const {
  return power_;
}

void PhyPacket::SetPower(int8_t power) {
  power_ = power;
}

Phy::Phy(Callbacks *callbacks):
    PhyInterface(callbacks),
    received_task_(&ReceivedTask, this),
    sent_task_(&SentTask, this) {
  struct sockaddr_in sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(9000 + args_info.eui64_arg);
  sockaddr.sin_addr.s_addr = INADDR_ANY;

  sockfd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  bind(sockfd_, (struct sockaddr*)&sockaddr, sizeof(sockaddr));

  pthread_create(&thread_, NULL, ReceiveThread, this);
}

Phy::Error Phy::SetPanId(uint16_t panid) {
  panid_ = panid;
  return kErrorNone;
}

Phy::Error Phy::SetExtendedAddress(uint8_t *address) {
  for (int i = 0; i < sizeof(extended_address_); i++)
    extended_address_[i] = address[7-i];
  return kErrorNone;
}

Phy::Error Phy::SetShortAddress(uint16_t address) {
  printf("%x\n", address);
  short_address_ = address;
  return kErrorNone;
}

Phy::Error Phy::Start() {
  Phy::Error error = kErrorNone;

  pthread_mutex_lock(&mutex_);
  VerifyOrExit(state_ == kStateDisabled, error = kErrorInvalidState);
  state_ = kStateSleep;
  pthread_cond_signal(&condition_variable_);

exit:
  pthread_mutex_unlock(&mutex_);
  return error;
}

Phy::Error Phy::Stop() {
  pthread_mutex_lock(&mutex_);
  state_ = kStateDisabled;
  pthread_cond_signal(&condition_variable_);
  pthread_mutex_unlock(&mutex_);
  return kErrorNone;
}

Phy::Error Phy::Sleep() {
  Phy::Error error = kErrorNone;

  pthread_mutex_lock(&mutex_);
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateSleep;
  pthread_cond_signal(&condition_variable_);

exit:
  pthread_mutex_unlock(&mutex_);
  return error;
}

Phy::Error Phy::Idle() {
  Phy::Error error = kErrorNone;

  pthread_mutex_lock(&mutex_);
  switch (state_) {
    case kStateSleep:
      state_ = kStateIdle;
      pthread_cond_signal(&condition_variable_);
      break;
    case kStateIdle:
      break;
    case kStateListen:
    case kStateTransmit:
      state_ = kStateIdle;
      pthread_cond_signal(&condition_variable_);
      break;
    case kStateDisabled:
    case kStateReceive:
      ExitNow(error = kErrorInvalidState);
      break;
  }

exit:
  pthread_mutex_unlock(&mutex_);
  return error;
}

Phy::Error Phy::Receive(PhyPacket *packet) {
  Phy::Error error = kErrorNone;

  pthread_mutex_lock(&mutex_);
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateListen;
  pthread_cond_signal(&condition_variable_);

  receive_packet_ = packet;

exit:
  pthread_mutex_unlock(&mutex_);
  return error;
}

Phy::Error Phy::Transmit(PhyPacket *packet) {
  Phy::Error error = kErrorNone;

  pthread_mutex_lock(&mutex_);
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateTransmit;
  pthread_cond_signal(&condition_variable_);

  transmit_packet_ = packet;
  data_pending_ = false;

  struct sockaddr_in sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr);

  for (int i = 1; i < 34; i++) {
    if (args_info.eui64_arg == i)
      continue;
    sockaddr.sin_port = htons(9000 + i);
    sendto(sockfd_, transmit_packet_->GetPsdu(), transmit_packet_->GetPsduLength(), 0,
           (struct sockaddr*)&sockaddr, sizeof(sockaddr));
  }

  if (!reinterpret_cast<MacFrame*>(transmit_packet_)->GetAckRequest())
    sent_task_.Post();

exit:
  pthread_mutex_unlock(&mutex_);
  return error;
}

Phy::State Phy::GetState() {
  Phy::State state;
  pthread_mutex_lock(&mutex_);
  state = state_;
  pthread_mutex_unlock(&mutex_);
  return state;
}

int8_t Phy::GetNoiseFloor() {
  return 0;
}

void Phy::SentTask(void *context) {
  Phy *obj = reinterpret_cast<Phy*>(context);
  obj->SentTask();
}

void Phy::SentTask() {
  State state;
  pthread_mutex_lock(&mutex_);
  state = state_;
  if (state_ != kStateDisabled)
    state_ = kStateIdle;
  pthread_cond_signal(&condition_variable_);
  pthread_mutex_unlock(&mutex_);

  if (state != kStateDisabled) {
    assert(state == kStateTransmit);
    callbacks_->HandleTransmitDone(transmit_packet_, data_pending_, kErrorNone);
  }
}

void *Phy::ReceiveThread(void *arg) {
  Phy *driver = reinterpret_cast<Phy*>(arg);
  driver->ReceiveThread();
  return NULL;
}

void Phy::ReceiveThread() {
  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfd_, &fds);

    int rval = select(sockfd_ + 1, &fds, NULL, NULL, NULL);
    if (rval >= 0 && FD_ISSET(sockfd_, &fds)) {
      pthread_mutex_lock(&mutex_);

#if 0
      while (state_ != kStateDisabled &&
             state_ != kStateSleep &&
             state_ != kStateListen &&
             state_ != kStateTransmit)
        pthread_cond_wait(&condition_variable_, &mutex_);
#else
      while (state_ == kStateIdle)
        pthread_cond_wait(&condition_variable_, &mutex_);
#endif

      switch (state_) {
        case kStateDisabled:
          recvfrom(sockfd_, NULL, 0, 0, NULL, NULL);
          break;
        case kStateIdle:
          recvfrom(sockfd_, NULL, 0, 0, NULL, NULL);
          dprintf("phy drop!\n");
          break;
        case kStateSleep:
          recvfrom(sockfd_, NULL, 0, 0, NULL, NULL);
          break;

        case kStateTransmit: {
          PhyPacket receive_packet;
          int length;
          length = recvfrom(sockfd_, receive_packet.GetPsdu(), MacFrame::kMTU, 0, NULL, NULL);
          if (length < 0) {
            dprintf("recvfrom error\n");
            assert(false);
          }

          if (reinterpret_cast<MacFrame*>(&receive_packet)->GetType() == MacFrame::kFcfFrameAck) {
            uint8_t tx_sequence, rx_sequence;
            reinterpret_cast<MacFrame*>(transmit_packet_)->GetSequence(&tx_sequence);
            reinterpret_cast<MacFrame*>(&receive_packet)->GetSequence(&rx_sequence);
            if (tx_sequence == rx_sequence) {
              if (reinterpret_cast<MacFrame*>(transmit_packet_)->GetType() == MacFrame::kFcfFrameMacCmd) {
                uint8_t command_id;
                reinterpret_cast<MacFrame*>(transmit_packet_)->GetCommandId(&command_id);
                if (command_id == MacFrame::kMacCmdDataRequest)
                  data_pending_ = true;
              }
              dprintf("Received ack %d\n", rx_sequence);
              sent_task_.Post();
            } else {
              dprintf("phy drop!\n");
            }
          }
          break;
        }
        case kStateListen:
          state_ = kStateReceive;
          received_task_.Post();
          while (state_ == kStateReceive)
            pthread_cond_wait(&condition_variable_, &mutex_);
          break;

        case kStateReceive:
          assert(false);
          break;
      }

      pthread_mutex_unlock(&mutex_);
    }
  }
}

void Phy::ReceivedTask(void *context) {
  Phy *obj = reinterpret_cast<Phy*>(context);
  obj->ReceivedTask();
}

void Phy::ReceivedTask() {
  Error error = kErrorNone;
  MacFrame *receive_frame;
  uint16_t dstpan;
  MacAddress dstaddr;
  int length;

  length = recvfrom(sockfd_, receive_packet_->GetPsdu(), MacFrame::kMTU, 0, NULL, NULL);
  receive_frame = reinterpret_cast<MacFrame*>(receive_packet_);

  receive_frame->GetDstAddr(&dstaddr);
  switch (dstaddr.length) {
    case 0:
      break;
    case 2:
      receive_frame->GetDstPanId(&dstpan);
      VerifyOrExit((dstpan == MacFrame::kShortAddrBroadcast || dstpan == panid_) &&
                   (dstaddr.address16 == MacFrame::kShortAddrBroadcast || dstaddr.address16 == short_address_),
                   error = kErrorAbort);
  break;
    case 8:
      receive_frame->GetDstPanId(&dstpan);
      VerifyOrExit((dstpan == MacFrame::kShortAddrBroadcast || dstpan == panid_) &&
                   memcmp(&dstaddr.address64, extended_address_, sizeof(dstaddr.address64)) == 0,
                   error = kErrorAbort);
      break;
    default:
      ExitNow(error = kErrorAbort);
  }

  receive_packet_->SetPsduLength(length);
  receive_packet_->SetPower(-20);

  // generate acknowledgment
  if (reinterpret_cast<MacFrame*>(receive_packet_)->GetAckRequest()) {
    uint8_t sequence;
    reinterpret_cast<MacFrame*>(receive_packet_)->GetSequence(&sequence);

    MacFrame *ack_frame = reinterpret_cast<MacFrame*>(&ack_packet_);
    ack_frame->InitMacHeader(MacFrame::kFcfFrameAck, MacFrame::kSecNone);
    ack_frame->SetSequence(sequence);

    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr);

    for (int i = 1; i < 34; i++) {
      if (args_info.eui64_arg == i)
        continue;
      sockaddr.sin_port = htons(9000 + i);
      sendto(sockfd_, ack_packet_.GetPsdu(), ack_packet_.GetPsduLength(), 0,
             (struct sockaddr*)&sockaddr, sizeof(sockaddr));
    }

    dprintf("Sent ack %d\n", sequence);
  }

exit:
  State state;
  pthread_mutex_lock(&mutex_);
  state = state_;
  if (state_ != kStateDisabled)
    state_ = kStateIdle;
  pthread_cond_signal(&condition_variable_);
  pthread_mutex_unlock(&mutex_);

  if (state != kStateDisabled) {
    assert(state == kStateReceive);
    callbacks_->HandleReceiveDone(receive_packet_, error);
  }
}

}  // namespace Thread
