/*
 *  Copyright (c) 2021, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#include <pw_protobuf/decoder.h>
#include <pw_protobuf/wire_format.h>
#include <pw_rpc/internal/packet.h>

#include "common/logging.hpp"
#include "rpc/rpc_decoder.hpp"

void PrintProtoBuf(std::span<const std::byte> aProto)
{
    pw::Status            status;
    pw::protobuf::Decoder decoder(std::as_bytes(aProto));

    while ((status = decoder.Next()).ok())
    {
        pw::protobuf::WireType wireType = decoder.ReadWireType();
        uint32_t               field    = decoder.FieldNumber();

        switch (wireType)
        {
        case pw::protobuf::WireType::kVarint:
        {
            uint64_t value;
            decoder.ReadUint64(&value);
            otLogCritMac("%d (kVarint)   : 0x%02x", field, value);
            break;
        }

        case pw::protobuf::WireType::kFixed64:
        {
            uint64_t value;
            decoder.ReadFixed64(&value);
            otLogCritMac("%d (kFixed64)  : 0x%02x", field, value);
        }
        break;

        case pw::protobuf::WireType::kDelimited:
        {
            char                       buf[500] = {0};
            char *                     start    = buf;
            char *                     end      = buf + sizeof(buf);
            std::span<const std::byte> value;

            decoder.ReadBytes(&value);

            (void)start;
            (void)end;
            for (uint16_t i = 0; i < value.size(); i++)
            {
                start += snprintf(start, end - start, "%02x ", static_cast<uint8_t>(value[i]));
            }

            otLogCritMac("%d (kDelimited): %s", field, buf);
        }
        break;

        case pw::protobuf::WireType::kFixed32:
        {
            uint32_t value;
            decoder.ReadFixed32(&value);
            otLogCritMac("%d (kFixed32)  : 0x%02x", field, value);
        }
        break;
        default:
            otLogCritMac("Default: %d : %d", (uint16_t)wireType, field);
            break;
        }
    }
}

void PrintProtoBuf(const uint8_t *aBuffer, uint16_t aLength)
{
    PrintProtoBuf(std::span<const std::byte>(reinterpret_cast<const std::byte *>(aBuffer), aLength));
}

void PrintRpcFrame(const uint8_t *aBuffer, uint16_t aLength)
{
    PrintProtoBuf(std::span<const std::byte>(reinterpret_cast<const std::byte *>(aBuffer), aLength));
}

static const char *PacketTypeToString(pw::rpc::internal::PacketType aType)
{
    static const char typeStrings[][25] = {"REQUEST",      "RESPONSE",     "CLIENT_STREAM_END",   "SERVER_STREAM_END",
                                           "CLIENT_ERROR", "SERVER_ERROR", "CANCEL_SERVER_STREAM"};

    return (aType <= pw::rpc::internal::PacketType::CANCEL_SERVER_STREAM) ? typeStrings[static_cast<uint8_t>(aType)]
                                                                          : "Unknown";
}

void PrintRpcPayload(const uint8_t *aBuffer, uint16_t aLength)
{
    auto result = pw::rpc::internal::Packet::FromBuffer(
        std::span<const std::byte>(reinterpret_cast<const std::byte *>(aBuffer), aLength));
    if (!result.ok())
    {
        otLogCritMac("Parse RPC packet failed");
    }
    else
    {
        auto &packet = result.value();
        otLogCritMac("RPC Header: Type: %s, ChannelId:0x%x, ServiceId:0x%x, MethodId:0x%x",
                     PacketTypeToString(packet.type()), packet.channel_id(), packet.service_id(), packet.method_id());
        PrintProtoBuf(packet.payload());
    }
}

void Test(void)
{
    uint8_t frame[] = {0x2a, 0x0a, 0x08, 0x88, 0xef, 0x99, 0xab, 0xc5, 0xe8, 0x8c, 0x91, 0x11, 0x08, 0x01,
                       0x10, 0x63, 0x1d, 0xe9, 0x62, 0x04, 0x88, 0x25, 0x69, 0xbc, 0xa9, 0xd6, 0x30, 0x00};

    otLogCritMac("ParseFrame:");
    PrintRpcFrame(frame, sizeof(frame));
    otLogCritMac("ParsePayload:");
    PrintRpcPayload(frame, sizeof(frame));
}
