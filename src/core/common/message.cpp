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

#include <common/code_utils.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6.h>

namespace Thread {

static int m_num_free_buffers;
static Buffer m_buffers[kNumBuffers];
static Buffer *m_free_buffers;

static Buffer *NewBuffer();
static ThreadError FreeBuffers(Buffer *buffers);
static ThreadError ReclaimBuffers(int num_buffers);

static MessageList m_all;

Buffer *NewBuffer()
{
    Buffer *buffer = NULL;

    VerifyOrExit(m_free_buffers != NULL, ;);

    buffer = m_free_buffers;
    m_free_buffers = m_free_buffers->header.next;
    buffer->header.next = NULL;
    m_num_free_buffers--;

exit:
    return buffer;
}

ThreadError FreeBuffers(Buffer *buffers)
{
    Buffer *tmp_buffer;

    while (buffers != NULL)
    {
        tmp_buffer = buffers->header.next;
        buffers->header.next = m_free_buffers;
        m_free_buffers = buffers;
        m_num_free_buffers++;
        buffers = tmp_buffer;
    }

    return kThreadError_None;
}

ThreadError ReclaimBuffers(int num_buffers)
{
    return (num_buffers <= m_num_free_buffers) ? kThreadError_None : kThreadError_NoBufs;
}

ThreadError Message::Init()
{
    m_free_buffers = m_buffers;

    for (int i = 0; i < kNumBuffers - 1; i++)
    {
        m_buffers[i].header.next = &m_buffers[i + 1];
    }

    m_buffers[kNumBuffers - 1].header.next = NULL;
    m_num_free_buffers = kNumBuffers;

    return kThreadError_None;
}

Message *Message::New(uint8_t type, uint16_t reserved)
{
    Message *message = NULL;

    VerifyOrExit((message = reinterpret_cast<Message *>(NewBuffer())) != NULL, ;);

    memset(message, 0, sizeof(*message));

    VerifyOrExit(message->SetTotalLength(reserved) == kThreadError_None,
                 Message::Free(*message));

    message->msg_type = type;
    message->msg_reserved = reserved;
    message->msg_length = reserved;

exit:
    return message;
}

ThreadError Message::Free(Message &message)
{
    assert(message.msg_list[MessageInfo::kListAll].list == NULL &&
           message.msg_list[MessageInfo::kListInterface].list == NULL);
    return FreeBuffers(reinterpret_cast<Buffer *>(&message));
}

ThreadError Message::ResizeMessage(uint16_t length)
{
    // add buffers
    Buffer *cur_buffer = this;
    Buffer *last_buffer;
    uint16_t cur_length = kFirstBufferDataSize;

    while (cur_length < length)
    {
        if (cur_buffer->header.next == NULL)
        {
            cur_buffer->header.next = NewBuffer();
        }

        cur_buffer = cur_buffer->header.next;
        cur_length += kBufferDataSize;
    }

    // remove buffers
    last_buffer = cur_buffer;
    cur_buffer = cur_buffer->header.next;
    last_buffer->header.next = NULL;

    FreeBuffers(cur_buffer);

    return kThreadError_None;
}

uint16_t Message::GetLength() const
{
    return msg_length - msg_reserved;
}

ThreadError Message::SetLength(uint16_t length)
{
    return SetTotalLength(msg_reserved + length);
}

ThreadError Message::SetTotalLength(uint16_t length)
{
    ThreadError error = kThreadError_None;
    int bufs = 0;

    if (length > kFirstBufferDataSize)
    {
        bufs = (((length - kFirstBufferDataSize) - 1) / kBufferDataSize) + 1;
    }

    if (msg_length > kFirstBufferDataSize)
    {
        bufs -= (((msg_length - kFirstBufferDataSize) - 1) / kBufferDataSize) + 1;
    }

    SuccessOrExit(error = ReclaimBuffers(bufs));

    ResizeMessage(length);
    msg_length = length;

exit:
    return error;
}

uint16_t Message::GetOffset() const
{
    return msg_offset;
}

ThreadError Message::MoveOffset(int delta)
{
    msg_offset += delta;
    assert(msg_offset <= GetLength());
    return kThreadError_None;
}

ThreadError Message::SetOffset(uint16_t offset)
{
    msg_offset = offset;
    assert(msg_offset <= GetLength());
    return kThreadError_None;
}

ThreadError Message::Append(const void *buf, uint16_t length)
{
    ThreadError error = kThreadError_None;
    uint16_t old_length = GetLength();

    SuccessOrExit(error = SetLength(GetLength() + length));
    Write(old_length, length, buf);

exit:
    return error;
}

ThreadError Message::Prepend(const void *buf, uint16_t length)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(length <= msg_reserved, error = kThreadError_NoBufs);
    msg_reserved -= length;
    msg_offset += length;

    Write(0, length, buf);

exit:
    return error;
}

int Message::Read(uint16_t offset, uint16_t length, void *buf) const
{
    Buffer *cur_buffer;
    uint16_t bytes_copied = 0;
    uint16_t bytes_to_copy;

    offset += msg_reserved;

    if (offset >= msg_length)
    {
        ExitNow();
    }

    if (offset + length >= msg_length)
    {
        length = msg_length - offset;
    }

    // special case first buffer
    if (offset < kFirstBufferDataSize)
    {
        bytes_to_copy = kFirstBufferDataSize - offset;

        if (bytes_to_copy > length)
        {
            bytes_to_copy = length;
        }

        memcpy(buf, head.data + offset, bytes_to_copy);

        length -= bytes_to_copy;
        bytes_copied += bytes_to_copy;
        buf = reinterpret_cast<uint8_t *>(buf) + bytes_to_copy;

        offset = 0;
    }
    else
    {
        offset -= kFirstBufferDataSize;
    }

    // advance to offset
    cur_buffer = header.next;

    while (offset >= kBufferDataSize)
    {
        assert(cur_buffer != NULL);

        cur_buffer = cur_buffer->header.next;
        offset -= kBufferDataSize;
    }

    // begin copy
    while (length > 0)
    {
        assert(cur_buffer != NULL);

        bytes_to_copy = kBufferDataSize - offset;

        if (bytes_to_copy > length)
        {
            bytes_to_copy = length;
        }

        memcpy(buf, cur_buffer->data + offset, bytes_to_copy);

        length -= bytes_to_copy;
        bytes_copied += bytes_to_copy;
        buf = reinterpret_cast<uint8_t *>(buf) + bytes_to_copy;

        cur_buffer = cur_buffer->header.next;
        offset = 0;
    }

exit:
    return bytes_copied;
}

int Message::Write(uint16_t offset, uint16_t length, const void *buf)
{
    Buffer *cur_buffer;
    uint16_t bytes_copied = 0;
    uint16_t bytes_to_copy;

    offset += msg_reserved;

    assert(offset + length <= msg_length);

    if (offset + length >= msg_length)
    {
        length = msg_length - offset;
    }

    // special case first buffer
    if (offset < kFirstBufferDataSize)
    {
        bytes_to_copy = kFirstBufferDataSize - offset;

        if (bytes_to_copy > length)
        {
            bytes_to_copy = length;
        }

        memcpy(head.data + offset, buf, bytes_to_copy);

        length -= bytes_to_copy;
        bytes_copied += bytes_to_copy;
        buf = reinterpret_cast<const uint8_t *>(buf) + bytes_to_copy;

        offset = 0;
    }
    else
    {
        offset -= kFirstBufferDataSize;
    }

    // advance to offset
    cur_buffer = header.next;

    while (offset >= kBufferDataSize)
    {
        assert(cur_buffer != NULL);

        cur_buffer = cur_buffer->header.next;
        offset -= kBufferDataSize;
    }

    // begin copy
    while (length > 0)
    {
        assert(cur_buffer != NULL);

        bytes_to_copy = kBufferDataSize - offset;

        if (bytes_to_copy > length)
        {
            bytes_to_copy = length;
        }

        memcpy(cur_buffer->data + offset, buf, bytes_to_copy);

        length -= bytes_to_copy;
        bytes_copied += bytes_to_copy;
        buf = reinterpret_cast<const uint8_t *>(buf) + bytes_to_copy;

        cur_buffer = cur_buffer->header.next;
        offset = 0;
    }

    return bytes_copied;
}

int Message::CopyTo(uint16_t src_offset, uint16_t dst_offset, uint16_t length, Message &message)
{
    uint16_t bytes_copied = 0;
    uint16_t bytes_to_copy;
    uint8_t buf[16];

    while (length > 0)
    {
        bytes_to_copy = (length < sizeof(buf)) ? length : sizeof(buf);

        Read(src_offset, bytes_to_copy, buf);
        message.Write(dst_offset, bytes_to_copy, buf);

        src_offset += bytes_to_copy;
        dst_offset += bytes_to_copy;
        length -= bytes_to_copy;
        bytes_copied += bytes_to_copy;
    }

    return bytes_copied;
}

uint8_t Message::GetType() const
{
    return msg_type;
}

Message *Message::GetNext() const
{
    return msg_list[MessageInfo::kListInterface].next;
}

uint16_t Message::GetDatagramTag() const
{
    return msg_dgram_tag;
}

void Message::SetDatagramTag(uint16_t tag)
{
    msg_dgram_tag = tag;
}

uint8_t Message::GetTimeout() const
{
    return msg_timeout;
}

void Message::SetTimeout(uint8_t timeout)
{
    msg_timeout = timeout;
}

uint16_t Message::UpdateChecksum(uint16_t checksum, uint16_t offset, uint16_t length) const
{
    Buffer *cur_buffer;
    uint16_t bytes_covered = 0;
    uint16_t bytes_to_cover;

    offset += msg_reserved;

    assert(static_cast<uint32_t>(offset) + length <= msg_length);

    // special case first buffer
    if (offset < kFirstBufferDataSize)
    {
        bytes_to_cover = kFirstBufferDataSize - offset;

        if (bytes_to_cover > length)
        {
            bytes_to_cover = length;
        }

        checksum = Ip6::UpdateChecksum(checksum, head.data + offset, bytes_to_cover);

        length -= bytes_to_cover;
        bytes_covered += bytes_to_cover;

        offset = 0;
    }
    else
    {
        offset -= kFirstBufferDataSize;
    }

    // advance to offset
    cur_buffer = header.next;

    while (offset >= kBufferDataSize)
    {
        assert(cur_buffer != NULL);

        cur_buffer = cur_buffer->header.next;
        offset -= kBufferDataSize;
    }

    // begin copy
    while (length > 0)
    {
        assert(cur_buffer != NULL);

        bytes_to_cover = kBufferDataSize - offset;

        if (bytes_to_cover > length)
        {
            bytes_to_cover = length;
        }

        checksum = Ip6::UpdateChecksum(checksum, cur_buffer->data + offset, bytes_to_cover);

        length -= bytes_to_cover;
        bytes_covered += bytes_to_cover;

        cur_buffer = cur_buffer->header.next;
        offset = 0;
    }

    return checksum;
}

bool Message::GetChildMask(uint8_t child_index) const
{
    return (msg_child_mask[child_index / 8] & (0x80 >> (child_index % 8))) != 0;
}

ThreadError Message::ClearChildMask(uint8_t child_index)
{
    msg_child_mask[child_index / 8] &= ~(0x80 >> (child_index % 8));
    return kThreadError_None;
}

ThreadError Message::SetChildMask(uint8_t child_index)
{
    msg_child_mask[child_index / 8] |= 0x80 >> (child_index % 8);
    return kThreadError_None;
}

bool Message::IsChildPending() const
{
    bool rval = false;

    for (size_t i = 0; i < sizeof(msg_child_mask); i++)
    {
        if (msg_child_mask[i] != 0)
        {
            ExitNow(rval = true);
        }
    }

exit:
    return rval;
}

bool Message::GetDirectTransmission() const
{
    return msg_direct_tx;
}

void Message::ClearDirectTransmission()
{
    msg_direct_tx = false;
}

void Message::SetDirectTransmission()
{
    msg_direct_tx = true;
}

MessageQueue::MessageQueue()
{
    m_interface.head = NULL;
    m_interface.tail = NULL;
}

ThreadError MessageQueue::AddToList(int list_id, Message &message)
{
    MessageList *list;

    assert(message.msg_list[list_id].next == NULL && message.msg_list[list_id].prev == NULL &&
           message.msg_list[list_id].list != NULL);

    list = message.msg_list[list_id].list;

    if (list->head == NULL)
    {
        list->head = &message;
        list->tail = &message;
    }
    else
    {
        list->tail->msg_list[list_id].next = &message;
        message.msg_list[list_id].prev = list->tail;
        list->tail = &message;
    }

    return kThreadError_None;
}

ThreadError MessageQueue::RemoveFromList(int list_id, Message &message)
{
    MessageList *list;

    assert(message.msg_list[list_id].list != NULL);

    list = message.msg_list[list_id].list;

    assert(list->head == &message || message.msg_list[list_id].next != NULL || message.msg_list[list_id].prev != NULL);

    if (message.msg_list[list_id].prev)
    {
        message.msg_list[list_id].prev->msg_list[list_id].next = message.msg_list[list_id].next;
    }
    else
    {
        list->head = message.msg_list[list_id].next;
    }

    if (message.msg_list[list_id].next)
    {
        message.msg_list[list_id].next->msg_list[list_id].prev = message.msg_list[list_id].prev;
    }
    else
    {
        list->tail = message.msg_list[list_id].prev;
    }

    message.msg_list[list_id].prev = NULL;
    message.msg_list[list_id].next = NULL;

    return kThreadError_None;
}

Message *MessageQueue::GetHead() const
{
    return m_interface.head;
}

ThreadError MessageQueue::Enqueue(Message &message)
{
    message.msg_list[MessageInfo::kListAll].list = &m_all;
    message.msg_list[MessageInfo::kListInterface].list = &m_interface;
    AddToList(MessageInfo::kListAll, message);
    AddToList(MessageInfo::kListInterface, message);
    return kThreadError_None;
}

ThreadError MessageQueue::Dequeue(Message &message)
{
    RemoveFromList(MessageInfo::kListAll, message);
    RemoveFromList(MessageInfo::kListInterface, message);
    message.msg_list[MessageInfo::kListAll].list = NULL;
    message.msg_list[MessageInfo::kListInterface].list = NULL;
    return kThreadError_None;
}

}  // namespace Thread
