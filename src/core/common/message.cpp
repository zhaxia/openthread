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

#include <common/code_utils.hpp>
#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/ip6.hpp>

namespace Thread {

static int sNumFreeBuffers;
static Buffer sBuffers[kNumBuffers];
static Buffer *sFreeBuffers;

static Buffer *NewBuffer();
static ThreadError FreeBuffers(Buffer *buffers);
static ThreadError ReclaimBuffers(int numBuffers);

static MessageList sAll;

Buffer *NewBuffer()
{
    Buffer *buffer = NULL;

    VerifyOrExit(sFreeBuffers != NULL, ;);

    buffer = sFreeBuffers;
    sFreeBuffers = sFreeBuffers->mHeader.mNext;
    buffer->mHeader.mNext = NULL;
    sNumFreeBuffers--;

exit:
    return buffer;
}

ThreadError FreeBuffers(Buffer *buffers)
{
    Buffer *tmpBuffer;

    while (buffers != NULL)
    {
        tmpBuffer = buffers->mHeader.mNext;
        buffers->mHeader.mNext = sFreeBuffers;
        sFreeBuffers = buffers;
        sNumFreeBuffers++;
        buffers = tmpBuffer;
    }

    return kThreadError_None;
}

ThreadError ReclaimBuffers(int numBuffers)
{
    return (numBuffers <= sNumFreeBuffers) ? kThreadError_None : kThreadError_NoBufs;
}

ThreadError Message::Init()
{
    sFreeBuffers = sBuffers;

    for (int i = 0; i < kNumBuffers - 1; i++)
    {
        sBuffers[i].mHeader.mNext = &sBuffers[i + 1];
    }

    sBuffers[kNumBuffers - 1].mHeader.mNext = NULL;
    sNumFreeBuffers = kNumBuffers;

    return kThreadError_None;
}

Message *Message::New(uint8_t type, uint16_t reserved)
{
    Message *message = NULL;

    VerifyOrExit((message = reinterpret_cast<Message *>(NewBuffer())) != NULL, ;);

    memset(message, 0, sizeof(*message));

    VerifyOrExit(message->SetTotalLength(reserved) == kThreadError_None,
                 Message::Free(*message));

    message->mMsgType = type;
    message->mMsgReserved = reserved;
    message->mMsgLength = reserved;

exit:
    return message;
}

ThreadError Message::Free(Message &message)
{
    assert(message.mMsgList[MessageInfo::kListAll].mList == NULL &&
           message.mMsgList[MessageInfo::kListInterface].mList == NULL);
    return FreeBuffers(reinterpret_cast<Buffer *>(&message));
}

ThreadError Message::ResizeMessage(uint16_t length)
{
    // add buffers
    Buffer *curBuffer = this;
    Buffer *lastBuffer;
    uint16_t curLength = kFirstBufferDataSize;

    while (curLength < length)
    {
        if (curBuffer->mHeader.mNext == NULL)
        {
            curBuffer->mHeader.mNext = NewBuffer();
        }

        curBuffer = curBuffer->mHeader.mNext;
        curLength += kBufferDataSize;
    }

    // remove buffers
    lastBuffer = curBuffer;
    curBuffer = curBuffer->mHeader.mNext;
    lastBuffer->mHeader.mNext = NULL;

    FreeBuffers(curBuffer);

    return kThreadError_None;
}

uint16_t Message::GetLength() const
{
    return mMsgLength - mMsgReserved;
}

ThreadError Message::SetLength(uint16_t length)
{
    return SetTotalLength(mMsgReserved + length);
}

ThreadError Message::SetTotalLength(uint16_t length)
{
    ThreadError error = kThreadError_None;
    int bufs = 0;

    if (length > kFirstBufferDataSize)
    {
        bufs = (((length - kFirstBufferDataSize) - 1) / kBufferDataSize) + 1;
    }

    if (mMsgLength > kFirstBufferDataSize)
    {
        bufs -= (((mMsgLength - kFirstBufferDataSize) - 1) / kBufferDataSize) + 1;
    }

    SuccessOrExit(error = ReclaimBuffers(bufs));

    ResizeMessage(length);
    mMsgLength = length;

exit:
    return error;
}

uint16_t Message::GetOffset() const
{
    return mMsgOffset;
}

ThreadError Message::MoveOffset(int delta)
{
    mMsgOffset += delta;
    assert(mMsgOffset <= GetLength());
    return kThreadError_None;
}

ThreadError Message::SetOffset(uint16_t offset)
{
    mMsgOffset = offset;
    assert(mMsgOffset <= GetLength());
    return kThreadError_None;
}

ThreadError Message::Append(const void *buf, uint16_t length)
{
    ThreadError error = kThreadError_None;
    uint16_t oldLength = GetLength();

    SuccessOrExit(error = SetLength(GetLength() + length));
    Write(oldLength, length, buf);

exit:
    return error;
}

ThreadError Message::Prepend(const void *buf, uint16_t length)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(length <= mMsgReserved, error = kThreadError_NoBufs);
    mMsgReserved -= length;
    mMsgOffset += length;

    Write(0, length, buf);

exit:
    return error;
}

int Message::Read(uint16_t offset, uint16_t length, void *buf) const
{
    Buffer *curBuffer;
    uint16_t bytesCopied = 0;
    uint16_t bytesToCopy;

    offset += mMsgReserved;

    if (offset >= mMsgLength)
    {
        ExitNow();
    }

    if (offset + length >= mMsgLength)
    {
        length = mMsgLength - offset;
    }

    // special case first buffer
    if (offset < kFirstBufferDataSize)
    {
        bytesToCopy = kFirstBufferDataSize - offset;

        if (bytesToCopy > length)
        {
            bytesToCopy = length;
        }

        memcpy(buf, mHead.mData + offset, bytesToCopy);

        length -= bytesToCopy;
        bytesCopied += bytesToCopy;
        buf = reinterpret_cast<uint8_t *>(buf) + bytesToCopy;

        offset = 0;
    }
    else
    {
        offset -= kFirstBufferDataSize;
    }

    // advance to offset
    curBuffer = mHeader.mNext;

    while (offset >= kBufferDataSize)
    {
        assert(curBuffer != NULL);

        curBuffer = curBuffer->mHeader.mNext;
        offset -= kBufferDataSize;
    }

    // begin copy
    while (length > 0)
    {
        assert(curBuffer != NULL);

        bytesToCopy = kBufferDataSize - offset;

        if (bytesToCopy > length)
        {
            bytesToCopy = length;
        }

        memcpy(buf, curBuffer->mData + offset, bytesToCopy);

        length -= bytesToCopy;
        bytesCopied += bytesToCopy;
        buf = reinterpret_cast<uint8_t *>(buf) + bytesToCopy;

        curBuffer = curBuffer->mHeader.mNext;
        offset = 0;
    }

exit:
    return bytesCopied;
}

int Message::Write(uint16_t offset, uint16_t length, const void *buf)
{
    Buffer *curBuffer;
    uint16_t bytesCopied = 0;
    uint16_t bytesToCopy;

    offset += mMsgReserved;

    assert(offset + length <= mMsgLength);

    if (offset + length >= mMsgLength)
    {
        length = mMsgLength - offset;
    }

    // special case first buffer
    if (offset < kFirstBufferDataSize)
    {
        bytesToCopy = kFirstBufferDataSize - offset;

        if (bytesToCopy > length)
        {
            bytesToCopy = length;
        }

        memcpy(mHead.mData + offset, buf, bytesToCopy);

        length -= bytesToCopy;
        bytesCopied += bytesToCopy;
        buf = reinterpret_cast<const uint8_t *>(buf) + bytesToCopy;

        offset = 0;
    }
    else
    {
        offset -= kFirstBufferDataSize;
    }

    // advance to offset
    curBuffer = mHeader.mNext;

    while (offset >= kBufferDataSize)
    {
        assert(curBuffer != NULL);

        curBuffer = curBuffer->mHeader.mNext;
        offset -= kBufferDataSize;
    }

    // begin copy
    while (length > 0)
    {
        assert(curBuffer != NULL);

        bytesToCopy = kBufferDataSize - offset;

        if (bytesToCopy > length)
        {
            bytesToCopy = length;
        }

        memcpy(curBuffer->mData + offset, buf, bytesToCopy);

        length -= bytesToCopy;
        bytesCopied += bytesToCopy;
        buf = reinterpret_cast<const uint8_t *>(buf) + bytesToCopy;

        curBuffer = curBuffer->mHeader.mNext;
        offset = 0;
    }

    return bytesCopied;
}

int Message::CopyTo(uint16_t srcOffset, uint16_t dstOffset, uint16_t length, Message &message)
{
    uint16_t bytesCopied = 0;
    uint16_t bytesToCopy;
    uint8_t buf[16];

    while (length > 0)
    {
        bytesToCopy = (length < sizeof(buf)) ? length : sizeof(buf);

        Read(srcOffset, bytesToCopy, buf);
        message.Write(dstOffset, bytesToCopy, buf);

        srcOffset += bytesToCopy;
        dstOffset += bytesToCopy;
        length -= bytesToCopy;
        bytesCopied += bytesToCopy;
    }

    return bytesCopied;
}

uint8_t Message::GetType() const
{
    return mMsgType;
}

Message *Message::GetNext() const
{
    return mMsgList[MessageInfo::kListInterface].mNext;
}

uint16_t Message::GetDatagramTag() const
{
    return mMsgDgramTag;
}

void Message::SetDatagramTag(uint16_t tag)
{
    mMsgDgramTag = tag;
}

uint8_t Message::GetTimeout() const
{
    return mMsgTimeout;
}

void Message::SetTimeout(uint8_t timeout)
{
    mMsgTimeout = timeout;
}

uint16_t Message::UpdateChecksum(uint16_t checksum, uint16_t offset, uint16_t length) const
{
    Buffer *curBuffer;
    uint16_t bytesCovered = 0;
    uint16_t bytesToCover;

    offset += mMsgReserved;

    assert(static_cast<uint32_t>(offset) + length <= mMsgLength);

    // special case first buffer
    if (offset < kFirstBufferDataSize)
    {
        bytesToCover = kFirstBufferDataSize - offset;

        if (bytesToCover > length)
        {
            bytesToCover = length;
        }

        checksum = Ip6::UpdateChecksum(checksum, mHead.mData + offset, bytesToCover);

        length -= bytesToCover;
        bytesCovered += bytesToCover;

        offset = 0;
    }
    else
    {
        offset -= kFirstBufferDataSize;
    }

    // advance to offset
    curBuffer = mHeader.mNext;

    while (offset >= kBufferDataSize)
    {
        assert(curBuffer != NULL);

        curBuffer = curBuffer->mHeader.mNext;
        offset -= kBufferDataSize;
    }

    // begin copy
    while (length > 0)
    {
        assert(curBuffer != NULL);

        bytesToCover = kBufferDataSize - offset;

        if (bytesToCover > length)
        {
            bytesToCover = length;
        }

        checksum = Ip6::UpdateChecksum(checksum, curBuffer->mData + offset, bytesToCover);

        length -= bytesToCover;
        bytesCovered += bytesToCover;

        curBuffer = curBuffer->mHeader.mNext;
        offset = 0;
    }

    return checksum;
}

bool Message::GetChildMask(uint8_t childIndex) const
{
    return (mMsgChildMask[childIndex / 8] & (0x80 >> (childIndex % 8))) != 0;
}

ThreadError Message::ClearChildMask(uint8_t childIndex)
{
    mMsgChildMask[childIndex / 8] &= ~(0x80 >> (childIndex % 8));
    return kThreadError_None;
}

ThreadError Message::SetChildMask(uint8_t childIndex)
{
    mMsgChildMask[childIndex / 8] |= 0x80 >> (childIndex % 8);
    return kThreadError_None;
}

bool Message::IsChildPending() const
{
    bool rval = false;

    for (size_t i = 0; i < sizeof(mMsgChildMask); i++)
    {
        if (mMsgChildMask[i] != 0)
        {
            ExitNow(rval = true);
        }
    }

exit:
    return rval;
}

bool Message::GetDirectTransmission() const
{
    return mMsgDirectTx;
}

void Message::ClearDirectTransmission()
{
    mMsgDirectTx = false;
}

void Message::SetDirectTransmission()
{
    mMsgDirectTx = true;
}

MessageQueue::MessageQueue()
{
    mInterface.mHead = NULL;
    mInterface.mTail = NULL;
}

ThreadError MessageQueue::AddToList(int list_id, Message &message)
{
    MessageList *list;

    assert(message.mMsgList[list_id].mNext == NULL && message.mMsgList[list_id].mPrev == NULL &&
           message.mMsgList[list_id].mList != NULL);

    list = message.mMsgList[list_id].mList;

    if (list->mHead == NULL)
    {
        list->mHead = &message;
        list->mTail = &message;
    }
    else
    {
        list->mTail->mMsgList[list_id].mNext = &message;
        message.mMsgList[list_id].mPrev = list->mTail;
        list->mTail = &message;
    }

    return kThreadError_None;
}

ThreadError MessageQueue::RemoveFromList(int list_id, Message &message)
{
    MessageList *list;

    assert(message.mMsgList[list_id].mList != NULL);

    list = message.mMsgList[list_id].mList;

    assert(list->mHead == &message || message.mMsgList[list_id].mNext != NULL || message.mMsgList[list_id].mPrev != NULL);

    if (message.mMsgList[list_id].mPrev)
    {
        message.mMsgList[list_id].mPrev->mMsgList[list_id].mNext = message.mMsgList[list_id].mNext;
    }
    else
    {
        list->mHead = message.mMsgList[list_id].mNext;
    }

    if (message.mMsgList[list_id].mNext)
    {
        message.mMsgList[list_id].mNext->mMsgList[list_id].mPrev = message.mMsgList[list_id].mPrev;
    }
    else
    {
        list->mTail = message.mMsgList[list_id].mPrev;
    }

    message.mMsgList[list_id].mPrev = NULL;
    message.mMsgList[list_id].mNext = NULL;

    return kThreadError_None;
}

Message *MessageQueue::GetHead() const
{
    return mInterface.mHead;
}

ThreadError MessageQueue::Enqueue(Message &message)
{
    message.mMsgList[MessageInfo::kListAll].mList = &sAll;
    message.mMsgList[MessageInfo::kListInterface].mList = &mInterface;
    AddToList(MessageInfo::kListAll, message);
    AddToList(MessageInfo::kListInterface, message);
    return kThreadError_None;
}

ThreadError MessageQueue::Dequeue(Message &message)
{
    RemoveFromList(MessageInfo::kListAll, message);
    RemoveFromList(MessageInfo::kListInterface, message);
    message.mMsgList[MessageInfo::kListAll].mList = NULL;
    message.mMsgList[MessageInfo::kListInterface].mList = NULL;
    return kThreadError_None;
}

}  // namespace Thread
