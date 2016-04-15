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

#ifndef MESSAGE_HPP_
#define MESSAGE_HPP_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <common/thread_error.hpp>
#include <mac/mac_frame.hpp>

namespace Thread {

enum
{
    kBufferPoolSize = 8192,
    kBufferSize = 128,
    kNumBuffers = kBufferPoolSize / kBufferSize,
};

class Message;

struct MessageList
{
    Message *mHead;
    Message *mTail;
};

struct MessageListEntry
{
    struct MessageList *mList;
    Message *mNext;
    Message *mPrev;
};

struct BufferHeader
{
    struct Buffer *mNext;
};

struct MessageInfo
{
    enum
    {
        kListAll = 0,
        kListInterface = 1,
    };
    MessageListEntry mList[2];
    uint16_t mHeaderReserved;
    uint16_t mLength;
    uint16_t mOffset;
    uint16_t mDatagramTag;
    uint8_t mTimeout;

    uint8_t mChildMask[8];

    uint8_t mType : 2;
    bool mDirectTx : 1;
};

struct Buffer
{
    enum
    {
        kBufferDataSize = kBufferSize - sizeof(struct BufferHeader),
        kFirstBufferDataSize = kBufferDataSize - sizeof(struct MessageInfo),
    };

    struct BufferHeader mHeader;
    union
    {
        struct
        {
            MessageInfo mInfo;
            uint8_t mData[kFirstBufferDataSize];
        } mHead;
        uint8_t mData[kBufferDataSize];
    };
};

#define mMsgList           mHead.mInfo.mList
#define mMsgQueue          mHead.mInfo.mQueue
#define mMsgReserved       mHead.mInfo.mHeaderReserved
#define mMsgLength         mHead.mInfo.mLength
#define mMsgOffset         mHead.mInfo.mOffset
#define mMsgDgramTag       mHead.mInfo.mDatagramTag
#define mMsgType           mHead.mInfo.mType
#define mMsgTimeout        mHead.mInfo.mTimeout
#define mMsgChildMask      mHead.mInfo.mChildMask
#define mMsgDirectTx       mHead.mInfo.mDirectTx

class Message: private Buffer
{
    friend class MessageQueue;

public:
    enum
    {
        kTypeIp6 = 0,   ///< A full uncompress IPv6 packet
        kType6lo = 1,   ///< A 6lo frame: mesh, fragment, or other
        kTypeMac = 2,   ///< A MAC frame: data poll, or other
        kTypeMisc = 3,  ///< A MAC frame: data poll, or other
    };

    uint16_t GetLength() const;
    ThreadError SetLength(uint16_t length);

    uint16_t GetOffset() const;
    ThreadError MoveOffset(int delta);
    ThreadError SetOffset(uint16_t offset);

    ThreadError Prepend(const void *buf, uint16_t length);
    ThreadError Append(const void *buf, uint16_t length);

    int Read(uint16_t offset, uint16_t length, void *buf) const;
    int Write(uint16_t offset, uint16_t length, const void *buf);
    int CopyTo(uint16_t srcOffset, uint16_t dstOffset, uint16_t length, Message &message);

    uint8_t GetType() const;
    Message *GetNext() const;

    uint16_t GetDatagramTag() const;
    void SetDatagramTag(uint16_t tag);

    bool GetChildMask(uint8_t child_index) const;
    ThreadError ClearChildMask(uint8_t child_index);
    ThreadError SetChildMask(uint8_t child_index);
    bool IsChildPending() const;

    bool GetDirectTransmission() const;
    void ClearDirectTransmission();
    void SetDirectTransmission();

    uint8_t GetTimeout() const;
    void SetTimeout(uint8_t timeout);

    uint16_t UpdateChecksum(uint16_t checksum, uint16_t offset, uint16_t length) const;

    static ThreadError Init();
    static Message *New(uint8_t type, uint16_t reserve_header);
    static ThreadError Free(Message &message);

private:
    ThreadError SetTotalLength(uint16_t length);
    ThreadError ResizeMessage(uint16_t length);
};

class MessageQueue
{
public:
    MessageQueue();
    Message *GetHead() const;
    ThreadError Enqueue(Message &message);
    ThreadError Dequeue(Message &message);

private:
    static ThreadError AddToList(int listId, Message &message);
    static ThreadError RemoveFromList(int listId, Message &message);

    MessageList mInterface;
};

}  // namespace Thread

#endif  // MESSAGE_HPP_
