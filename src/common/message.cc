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

#include <common/code_utils.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6.h>

namespace Thread {

static int num_free_buffers_;
static Buffer buffers_[kNumBuffers];
static Buffer *free_buffers_;

static Buffer *NewBuffer();
static ThreadError FreeBuffers(Buffer *buffers);
static ThreadError ReclaimBuffers(int num_buffers);

Buffer *NewBuffer() {
  Buffer *buffer = NULL;

  VerifyOrExit(free_buffers_ != NULL, ;);

  buffer = free_buffers_;
  free_buffers_ = free_buffers_->header.next;
  buffer->header.next = NULL;
  num_free_buffers_--;

exit:
  return buffer;
}

ThreadError FreeBuffers(Buffer *buffers) {
  while (buffers != NULL) {
    Buffer *tmp_buffer = buffers->header.next;
    buffers->header.next = free_buffers_;
    free_buffers_ = buffers;
    num_free_buffers_++;
    buffers = tmp_buffer;
  }
  return kThreadError_None;
}

ThreadError ReclaimBuffers(int num_buffers) {
  return (num_buffers <= num_free_buffers_) ? kThreadError_None : kThreadError_NoBufs;
}

ThreadError Message::Init() {
  free_buffers_ = buffers_;

  for (int i = 0; i < kNumBuffers - 1; i++)
    buffers_[i].header.next = &buffers_[i+1];
  buffers_[kNumBuffers-1].header.next = NULL;
  num_free_buffers_ = kNumBuffers;

  return kThreadError_None;
}

Message *Message::New(uint8_t type, uint16_t reserved) {
  Message *message = NULL;

  VerifyOrExit((message = reinterpret_cast<Message*>(NewBuffer())) != NULL, ;);

  memset(message, 0, sizeof(*message));

  VerifyOrExit(message->SetTotalLength(reserved) == kThreadError_None,
               Message::Free(message));

  message->msg_type = type;
  message->msg_reserved = reserved;
  message->msg_length = reserved;

exit:
  return message;
}

ThreadError Message::Free(Message *message) {
  assert(message->msg_list[MessageInfo::kListAll].list == NULL &&
         message->msg_list[MessageInfo::kListInterface].list == NULL);
  return FreeBuffers(reinterpret_cast<Buffer*>(message));
}

ThreadError Message::ResizeMessage(uint16_t length) {
  // add buffers
  Buffer *cur_buffer = this;
  uint16_t cur_length = kFirstBufferDataSize;
  while (cur_length < length) {
    if (cur_buffer->header.next == NULL)
      cur_buffer->header.next = NewBuffer();
    cur_buffer = cur_buffer->header.next;
    cur_length += kBufferDataSize;
  }

  // remove buffers
  Buffer *last_buffer = cur_buffer;
  cur_buffer = cur_buffer->header.next;
  last_buffer->header.next = NULL;

  FreeBuffers(cur_buffer);

  return kThreadError_None;
}

uint16_t Message::GetLength() const {
  return msg_length - msg_reserved;
}

ThreadError Message::SetLength(uint16_t length) {
  return SetTotalLength(msg_reserved + length);
}

ThreadError Message::SetTotalLength(uint16_t length) {
  ThreadError error = kThreadError_None;
  int bufs = 0;

  if (length > kFirstBufferDataSize)
    bufs = (((length - kFirstBufferDataSize) - 1) / kBufferDataSize) + 1;

  if (msg_length > kFirstBufferDataSize)
    bufs -= (((msg_length - kFirstBufferDataSize) - 1) / kBufferDataSize) + 1;

  SuccessOrExit(error = ReclaimBuffers(bufs));

  ResizeMessage(length);
  msg_length = length;

exit:
  return error;
}

uint16_t Message::GetOffset() const {
  return msg_offset;
}

ThreadError Message::MoveOffset(int delta) {
  msg_offset += delta;
  return kThreadError_None;
}

ThreadError Message::SetOffset(uint16_t offset) {
  msg_offset = offset;
  return kThreadError_None;
}

ThreadError Message::Append(const void *buf, uint16_t length) {
  ThreadError error = kThreadError_None;
  uint16_t old_length = GetLength();

  SuccessOrExit(error = SetLength(GetLength() + length));
  Write(old_length, length, buf);

exit:
  return error;
}

ThreadError Message::Prepend(const void *buf, uint16_t length) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(length <= msg_reserved, error = kThreadError_NoBufs);
  msg_reserved -= length;
  msg_offset += length;

  Write(0, length, buf);

exit:
  return error;
}

int Message::Read(uint16_t offset, uint16_t length, void *buf) const {
  uint16_t bytes_copied = 0;
  uint16_t bytes_to_copy;

  offset += msg_reserved;

  if (offset >= msg_length)
    return 0;

  if (offset + length >= msg_length)
    length = msg_length - offset;

  // special case first buffer
  if (offset < kFirstBufferDataSize) {
    bytes_to_copy = kFirstBufferDataSize - offset;
    if (bytes_to_copy > length)
      bytes_to_copy = length;
    memcpy(buf, head.data + offset, bytes_to_copy);

    length -= bytes_to_copy;
    bytes_copied += bytes_to_copy;
    buf = reinterpret_cast<uint8_t*>(buf) + bytes_to_copy;

    offset = 0;
  } else {
    offset -= kFirstBufferDataSize;
  }

  // advance to offset
  Buffer *cur_buffer = header.next;
  while (offset >= kBufferDataSize) {
    if (cur_buffer == NULL)
      break;
    cur_buffer = cur_buffer->header.next;
    offset -= kBufferDataSize;
  }

  // begin copy
  while (length > 0) {
    if (cur_buffer == NULL)
      break;

    bytes_to_copy = kBufferDataSize - offset;
    if (bytes_to_copy > length)
      bytes_to_copy = length;
    memcpy(buf, cur_buffer->data + offset, bytes_to_copy);

    length -= bytes_to_copy;
    bytes_copied += bytes_to_copy;
    buf = reinterpret_cast<uint8_t*>(buf) + bytes_to_copy;

    cur_buffer = cur_buffer->header.next;
    offset = 0;
  }

  return bytes_copied;
}

int Message::Write(uint16_t offset, uint16_t length, const void *buf) {
  uint16_t bytes_copied = 0;
  uint16_t bytes_to_copy;

  offset += msg_reserved;

  if (offset >= msg_length)
    return 0;

  if (offset + length >= msg_length)
    length = msg_length - offset;

  // special case first buffer
  if (offset < kFirstBufferDataSize) {
    bytes_to_copy = kFirstBufferDataSize - offset;
    if (bytes_to_copy > length)
      bytes_to_copy = length;
    memcpy(head.data + offset, buf, bytes_to_copy);

    length -= bytes_to_copy;
    bytes_copied += bytes_to_copy;
    buf = reinterpret_cast<const uint8_t*>(buf) + bytes_to_copy;

    offset = 0;
  } else {
    offset -= kFirstBufferDataSize;
  }

  // advance to offset
  Buffer *cur_buffer = header.next;
  while (offset >= kBufferDataSize) {
    if (cur_buffer == NULL)
      break;
    cur_buffer = cur_buffer->header.next;
    offset -= kBufferDataSize;
  }

  // begin copy
  while (length) {
    if (cur_buffer == NULL)
      break;

    bytes_to_copy = kBufferDataSize - offset;
    if (bytes_to_copy > length)
      bytes_to_copy = length;
    memcpy(cur_buffer->data + offset, buf, bytes_to_copy);

    length -= bytes_to_copy;
    bytes_copied += bytes_to_copy;
    buf = reinterpret_cast<const uint8_t*>(buf) + bytes_to_copy;

    cur_buffer = cur_buffer->header.next;
    offset = 0;
  }

  return bytes_copied;
}

int Message::CopyTo(uint16_t src_offset, uint16_t dst_offset, uint16_t length, Message *message) {
  uint16_t bytes_copied = 0;

  // XXX: The current implementation requires two copies.  We can
  // optimize to perform a single copy if needed.
  while (length > 0) {
    uint8_t buf[16];
    uint16_t bytes_to_copy = (length < sizeof(buf)) ? length : sizeof(buf);

    Read(src_offset, bytes_to_copy, buf);
    message->Write(dst_offset, bytes_to_copy, buf);

    src_offset += bytes_to_copy;
    dst_offset += bytes_to_copy;
    length -= bytes_to_copy;
    bytes_copied += bytes_to_copy;
  }

  return bytes_copied;
}

uint8_t Message::GetType() const {
  return msg_type;
}

Message *Message::GetNext() const {
  return msg_list[MessageInfo::kListInterface].next;
}

uint16_t Message::GetDatagramTag() const {
  return msg_dgram_tag;
}

ThreadError Message::SetDatagramTag(uint16_t tag) {
  msg_dgram_tag = tag;
  return kThreadError_None;
}

uint8_t Message::GetTimeout() const {
  return msg_timeout;
}

ThreadError Message::SetTimeout(uint8_t timeout) {
  msg_timeout = timeout;
  return kThreadError_None;
}

uint16_t Message::UpdateChecksum(uint16_t checksum, uint16_t offset, uint16_t length) const {
  uint16_t bytes_covered = 0;
  uint16_t bytes_to_cover;

  offset += msg_reserved;

  if (static_cast<uint32_t>(offset) + length > msg_length)
    return kThreadError_Error;

  // special case first buffer
  if (offset < kFirstBufferDataSize) {
    bytes_to_cover = kFirstBufferDataSize - offset;
    if (bytes_to_cover > length)
      bytes_to_cover = length;
    checksum = Ip6::UpdateChecksum(checksum, head.data + offset, bytes_to_cover);

    length -= bytes_to_cover;
    bytes_covered += bytes_to_cover;

    offset = 0;
  } else {
    offset -= kFirstBufferDataSize;
  }

  // advance to offset
  Buffer *cur_buffer = header.next;
  while (offset >= kBufferDataSize) {
    if (cur_buffer == NULL)
      break;
    cur_buffer = cur_buffer->header.next;
    offset -= kBufferDataSize;
  }

  // begin copy
  while (length > 0) {
    if (cur_buffer == NULL)
      break;

    bytes_to_cover = kBufferDataSize - offset;
    if (bytes_to_cover > length)
      bytes_to_cover = length;
    checksum = Ip6::UpdateChecksum(checksum, cur_buffer->data + offset, bytes_to_cover);

    length -= bytes_to_cover;
    bytes_covered += bytes_to_cover;

    cur_buffer = cur_buffer->header.next;
    offset = 0;
  }

  return checksum;
}

ThreadError Message::GetMacDestination(MacAddress *address) const {
  if (msg_mac_dst_short) {
    address->length = 2;
    address->address16 = msg_mac_dst_16;
  } else {
    address->length = 8;
    memcpy(&address->address64, msg_mac_dst_64, sizeof(address->address64));
  }
  return kThreadError_None;
}

ThreadError Message::SetMacDestination(const MacAddress *address) {
  switch (address->length) {
    case 2:
      SetMacDestination(address->address16);
      break;
    case 8:
      msg_mac_dst_short = false;
      memcpy(msg_mac_dst_64, &address->address64, sizeof(msg_mac_dst_64));
      break;
    default:
      assert(false);
  }
  return kThreadError_None;
}

ThreadError Message::SetMacDestination(uint16_t address) {
  msg_mac_dst_short = true;
  msg_mac_dst_16 = address;
  return kThreadError_None;
}

ThreadError Message::GetMacSource(MacAddress *address) const {
  if (msg_mac_src_short) {
    address->length = 2;
    address->address16 = msg_mac_src_16;
  } else {
    address->length = 8;
    memcpy(&address->address64, msg_mac_src_64, sizeof(address->address64));
  }
  return kThreadError_None;
}

ThreadError Message::SetMacSource(const MacAddress *address) {
  switch (address->length) {
    case 2:
      SetMacSource(address->address16);
      break;
    case 8:
      msg_mac_src_short = false;
      memcpy(msg_mac_src_64, &address->address64, sizeof(msg_mac_src_64));
      break;
    default:
      assert(false);
  }
  return kThreadError_None;
}

ThreadError Message::SetMacSource(uint16_t address) {
  msg_mac_src_short = true;
  msg_mac_src_16 = address;
  return kThreadError_None;
}

bool Message::IsMeshHeaderEnabled() const {
  return msg_mesh_header;
}

ThreadError Message::SetMeshHeaderEnable(bool enable) {
  msg_mesh_header = enable;
  return kThreadError_None;
}

MacAddr16 Message::GetMeshDestination() const {
  return msg_mesh_dst;
}

ThreadError Message::SetMeshDestination(MacAddr16 address) {
  msg_mesh_dst = address;
  return kThreadError_None;
}

MacAddr16 Message::GetMeshSource() const {
  return msg_mesh_src;
}

ThreadError Message::SetMeshSource(MacAddr16 address) {
  msg_mesh_src = address;
  return kThreadError_None;
}

bool Message::GetChildMask(uint8_t child_index) const {
  return (msg_child_mask[child_index/8] & (0x80 >> (child_index % 8))) != 0;
}

ThreadError Message::ClearChildMask(uint8_t child_index) {
  msg_child_mask[child_index / 8] &= ~(0x80 >> (child_index % 8));
  return kThreadError_None;
}

ThreadError Message::SetChildMask(uint8_t child_index) {
  msg_child_mask[child_index / 8] |= 0x80 >> (child_index % 8);
  return kThreadError_None;
}

bool Message::IsChildPending() const {
  for (int i = 0; i < sizeof(msg_child_mask); i++) {
    if (msg_child_mask[i] != 0)
      return true;
  }
  return false;
}

bool Message::GetDirectTransmission() const {
  return msg_direct_tx;
}

ThreadError Message::ClearDirectTransmission() {
  msg_direct_tx = false;
  return kThreadError_None;
}

ThreadError Message::SetDirectTransmission() {
  msg_direct_tx = true;
  return kThreadError_None;
}

void Message::Dump() const {
  uint8_t buf[1500];
  Read(0, sizeof(buf), buf);
  dump("message-dump", buf, GetLength());
}

MessageList  MessageQueue::all_;

MessageQueue::MessageQueue() {
  interface_.head = NULL;
  interface_.tail = NULL;
}

ThreadError MessageQueue::AddToList(int list_id, Message *message) {
  assert(message->msg_list[list_id].next == NULL && message->msg_list[list_id].prev == NULL &&
         message->msg_list[list_id].list != NULL);

  MessageList *list = message->msg_list[list_id].list;

  if (list->head == NULL) {
    list->head = message;
    list->tail = message;
  } else {
    list->tail->msg_list[list_id].next = message;
    message->msg_list[list_id].prev = list->tail;
    list->tail = message;
  }

  return kThreadError_None;
}

ThreadError MessageQueue::RemoveFromList(int list_id, Message *message) {
  assert(message->msg_list[list_id].list != NULL);

  MessageList *list = message->msg_list[list_id].list;

  assert(list->head == message || message->msg_list[list_id].next != NULL || message->msg_list[list_id].prev != NULL);

  if (message->msg_list[list_id].prev)
    message->msg_list[list_id].prev->msg_list[list_id].next = message->msg_list[list_id].next;
  else
    list->head = message->msg_list[list_id].next;

  if (message->msg_list[list_id].next)
    message->msg_list[list_id].next->msg_list[list_id].prev = message->msg_list[list_id].prev;
  else
    list->tail = message->msg_list[list_id].prev;

  message->msg_list[list_id].prev = NULL;
  message->msg_list[list_id].next = NULL;

  return kThreadError_None;
}

Message *MessageQueue::GetHead() const {
  return interface_.head;
}

Message *MessageQueue::GetNext(Message *message) const {
  return message->msg_list[MessageInfo::kListInterface].next;
}

ThreadError MessageQueue::Enqueue(Message *message) {
  ThreadError error = kThreadError_None;

  message->msg_list[MessageInfo::kListAll].list = &all_;
  SuccessOrExit(error = AddToList(MessageInfo::kListAll, message));
  message->msg_list[MessageInfo::kListInterface].list = &interface_;
  SuccessOrExit(error = AddToList(MessageInfo::kListInterface, message));

exit:
  return error;
}

ThreadError MessageQueue::Dequeue(Message *message) {
  ThreadError error = kThreadError_None;

  SuccessOrExit(error = RemoveFromList(MessageInfo::kListAll, message));;
  SuccessOrExit(error = RemoveFromList(MessageInfo::kListInterface, message));
  message->msg_list[MessageInfo::kListAll].list = NULL;
  message->msg_list[MessageInfo::kListInterface].list = NULL;

exit:
  return error;
}

}  // namespace Thread
