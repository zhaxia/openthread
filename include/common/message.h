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

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <common/thread_error.h>
#include <mac/mac_frame.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    Message *head;
    Message *tail;
};

struct MessageListEntry
{
    struct MessageList *list;
    Message *next;
    Message *prev;
};

struct BufferHeader
{
    struct Buffer *next;
};

struct MessageInfo
{
    enum
    {
        kListAll = 0,
        kListInterface = 1,
    };
    MessageListEntry list[2];
    uint16_t header_reserved;
    uint16_t length;
    uint16_t offset;
    uint16_t datagram_tag;
    uint8_t timeout;

    uint8_t child_mask[8];

    uint8_t type : 2;
    bool direct_tx : 1;
};

struct Buffer
{
    enum
    {
        kBufferDataSize = kBufferSize - sizeof(struct BufferHeader),
        kFirstBufferDataSize = kBufferDataSize - sizeof(struct MessageInfo),
    };

    struct BufferHeader header;
    union
    {
        struct
        {
            MessageInfo info;
            uint8_t data[kFirstBufferDataSize];
        } head;
        uint8_t data[kBufferDataSize];
    };
};

#define msg_list           head.info.list
#define msg_queue          head.info.queue
#define msg_reserved       head.info.header_reserved
#define msg_length         head.info.length
#define msg_offset         head.info.offset
#define msg_dgram_tag      head.info.datagram_tag
#define msg_type           head.info.type
#define msg_timeout        head.info.timeout
#define msg_child_mask     head.info.child_mask
#define msg_direct_tx      head.info.direct_tx

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
    int CopyTo(uint16_t src_offset, uint16_t dst_offset, uint16_t length, Message &message);

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
    static ThreadError AddToList(int list_id, Message &message);
    static ThreadError RemoveFromList(int list_id, Message &message);

    MessageList m_interface;
};

}  // namespace Thread

#endif  // MESSAGE_H_
