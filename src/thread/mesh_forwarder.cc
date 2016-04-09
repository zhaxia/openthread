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

#include <thread/mesh_forwarder.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/message.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <net/ip6.h>
#include <net/netif.h>
#include <net/udp6.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

MeshForwarder::MeshForwarder(ThreadNetif &netif):
    m_mac_receiver(&HandleReceivedFrame, this),
    m_mac_sender(&HandleFrameRequest, &HandleSentFrame, this),
    m_poll_timer(&HandlePollTimer, this),
    m_reassembly_timer(&HandleReassemblyTimer, this),
    m_schedule_transmission_task(ScheduleTransmissionTask, this)
{
    m_address_resolver = netif.GetAddressResolver();
    m_lowpan = netif.GetLowpan();
    m_mac = netif.GetMac();
    m_mle = netif.GetMle();
    m_netif = &netif;
    m_network_data = netif.GetNetworkDataLeader();
    m_frag_tag = Random::Get();
}

ThreadError MeshForwarder::Start()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_enabled == false, error = kThreadError_Busy);

    m_mac->RegisterReceiver(m_mac_receiver);
    SuccessOrExit(error = m_mac->Start());

    m_enabled = true;

exit:
    return error;
}

ThreadError MeshForwarder::Stop()
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit(m_enabled == true, error = kThreadError_Busy);

    m_poll_timer.Stop();
    m_reassembly_timer.Stop();

    while ((message = m_send_queue.GetHead()) != NULL)
    {
        m_send_queue.Dequeue(*message);
        Message::Free(*message);
    }

    while ((message = m_reassembly_list.GetHead()) != NULL)
    {
        m_reassembly_list.Dequeue(*message);
        Message::Free(*message);
    }

    m_enabled = false;

    SuccessOrExit(error = m_mac->Stop());

exit:
    return error;
}

const Mac::Address64 *MeshForwarder::GetAddress64() const
{
    return m_mac->GetAddress64();
}

Mac::Address16 MeshForwarder::GetAddress16() const
{
    return m_mac->GetAddress16();
}

ThreadError MeshForwarder::SetAddress16(Mac::Address16 address16)
{
    m_mac->SetAddress16(address16);
    return kThreadError_None;
}

void MeshForwarder::HandleResolved(const Ip6Address &eid)
{
    Message *cur, *next;
    Ip6Address ip6_dst;

    for (cur = m_resolving_queue.GetHead(); cur; cur = next)
    {
        next = cur->GetNext();

        if (cur->GetType() != Message::kTypeIp6)
        {
            continue;
        }

        cur->Read(Ip6Header::GetDestinationOffset(), sizeof(ip6_dst), &ip6_dst);

        if (memcmp(&ip6_dst, &eid, sizeof(ip6_dst)) == 0)
        {
            m_resolving_queue.Dequeue(*cur);
            m_send_queue.Enqueue(*cur);
        }
    }

    m_schedule_transmission_task.Post();
}

void MeshForwarder::ScheduleTransmissionTask(void *context)
{
    MeshForwarder *obj = reinterpret_cast<MeshForwarder *>(context);
    obj->ScheduleTransmissionTask();
}

void MeshForwarder::ScheduleTransmissionTask()
{
    ThreadError error = kThreadError_None;
    uint8_t num_children;
    Child *children;

    VerifyOrExit(m_send_busy == false, error = kThreadError_Busy);

    children = m_mle->GetChildren(&num_children);

    for (int i = 0; i < num_children; i++)
    {
        if (children[i].state == Child::kStateValid &&
            children[i].data_request &&
            (m_send_message = GetIndirectTransmission(children[i])) != NULL)
        {
            m_mac->SendFrameRequest(m_mac_sender);
            ExitNow();
        }
    }

    if ((m_send_message = GetDirectTransmission()) != NULL)
    {
        m_mac->SendFrameRequest(m_mac_sender);
        ExitNow();
    }

exit:
    (void) error;
}

ThreadError MeshForwarder::SendMessage(Message &message)
{
    ThreadError error = kThreadError_None;
    Neighbor *neighbor;
    Ip6Header ip6_header;
    uint8_t num_children;
    Child *children;
    MeshHeader mesh_header;

    switch (message.GetType())
    {
    case Message::kTypeIp6:
        message.Read(0, sizeof(ip6_header), &ip6_header);

        if (!memcmp(ip6_header.GetDestination(), m_mle->GetLinkLocalAllThreadNodesAddress(),
                    sizeof(*ip6_header.GetDestination())) ||
            !memcmp(ip6_header.GetDestination(), m_mle->GetRealmLocalAllThreadNodesAddress(), sizeof(*ip6_header.GetDestination())))
        {
            // schedule direct transmission
            message.SetDirectTransmission();

            // destined for all sleepy children
            children = m_mle->GetChildren(&num_children);

            for (int i = 0; i < num_children; i++)
            {
                if (children[i].state == Neighbor::kStateValid && (children[i].mode & Mle::kModeRxOnWhenIdle) == 0)
                {
                    message.SetChildMask(i);
                }
            }
        }
        else if ((neighbor = m_mle->GetNeighbor(*ip6_header.GetDestination())) != NULL &&
                 (neighbor->mode & Mle::kModeRxOnWhenIdle) == 0)
        {
            // destined for a sleepy child
            message.SetChildMask(m_mle->GetChildIndex(*reinterpret_cast<Child *>(neighbor)));
        }
        else
        {
            // schedule direct transmission
            message.SetDirectTransmission();
        }

        break;

    case Message::kType6lo:
        message.Read(0, sizeof(mesh_header), &mesh_header);

        if ((neighbor = m_mle->GetNeighbor(mesh_header.GetDestination())) != NULL &&
            (neighbor->mode & Mle::kModeRxOnWhenIdle) == 0)
        {
            // destined for a sleepy child
            message.SetChildMask(m_mle->GetChildIndex(*reinterpret_cast<Child *>(neighbor)));
        }
        else
        {
            // not destined for a sleepy child
            message.SetDirectTransmission();
        }

        break;

    case Message::kTypeMac:
        message.SetDirectTransmission();
        break;
    }

    message.SetOffset(0);
    SuccessOrExit(error = m_send_queue.Enqueue(message));
    m_schedule_transmission_task.Post();

exit:
    return error;
}

void MeshForwarder::MoveToResolving(const Ip6Address &destination)
{
    Message *cur, *next;
    Ip6Address ip6_dst;

    for (cur = m_send_queue.GetHead(); cur; cur = next)
    {
        next = cur->GetNext();

        if (cur->GetType() != Message::kTypeIp6)
        {
            continue;
        }

        cur->Read(Ip6Header::GetDestinationOffset(), sizeof(ip6_dst), &ip6_dst);

        if (memcmp(&ip6_dst, &destination, sizeof(ip6_dst)) == 0)
        {
            m_send_queue.Dequeue(*cur);
            m_resolving_queue.Enqueue(*cur);
        }
    }
}

Message *MeshForwarder::GetDirectTransmission()
{
    Message *cur_message, *next_message;
    ThreadError error = kThreadError_None;
    Ip6Address ip6_dst;

    for (cur_message = m_send_queue.GetHead(); cur_message; cur_message = next_message)
    {
        next_message = cur_message->GetNext();

        if (cur_message->GetDirectTransmission() == false)
        {
            continue;
        }

        switch (cur_message->GetType())
        {
        case Message::kTypeIp6:
            error = UpdateIp6Route(*cur_message);
            break;

        case Message::kType6lo:
            error = UpdateMeshRoute(*cur_message);
            break;

        case Message::kTypeMac:
            ExitNow();
        }

        switch (error)
        {
        case kThreadError_None:
            ExitNow();

        case kThreadError_LeaseQuery:
            cur_message->Read(Ip6Header::GetDestinationOffset(), sizeof(ip6_dst), &ip6_dst);
            MoveToResolving(ip6_dst);
            continue;

        case kThreadError_Drop:
        case kThreadError_NoBufs:
            m_send_queue.Dequeue(*cur_message);
            Message::Free(*cur_message);
            continue;

        default:
            dprintf("error = %d\n", error);
            assert(false);
            break;
        }
    }

exit:
    return cur_message;
}

Message *MeshForwarder::GetIndirectTransmission(const Child &child)
{
    Message *message = NULL;
    int child_index = m_mle->GetChildIndex(child);
    Ip6Header ip6_header;
    MeshHeader mesh_header;

    for (message = m_send_queue.GetHead(); message; message = message->GetNext())
    {
        if (message->GetChildMask(child_index))
        {
            break;
        }
    }

    VerifyOrExit(message != NULL, ;);

    switch (message->GetType())
    {
    case Message::kTypeIp6:
        message->Read(0, sizeof(ip6_header), &ip6_header);

        m_add_mesh_header = false;
        GetMacSourceAddress(*ip6_header.GetSource(), m_macsrc);

        if (ip6_header.GetDestination()->IsLinkLocal() || ip6_header.GetDestination()->IsMulticast())
        {
            GetMacDestinationAddress(*ip6_header.GetDestination(), m_macdst);
        }
        else
        {
            m_macdst.length = 2;
            m_macdst.address16 = child.valid.rloc16;
        }

        break;

    case Message::kType6lo:
        message->Read(0, sizeof(mesh_header), &mesh_header);

        m_add_mesh_header = true;
        m_meshdst = mesh_header.GetDestination();
        m_meshsrc = mesh_header.GetSource();
        m_macsrc.length = 2;
        m_macsrc.address16 = GetAddress16();
        m_macdst.length = 2;
        m_macdst.address16 = mesh_header.GetDestination();
        break;

    default:
        assert(false);
        break;
    }

exit:
    return message;
}

ThreadError MeshForwarder::UpdateMeshRoute(Message &message)
{
    ThreadError error = kThreadError_None;
    MeshHeader mesh_header;
    Neighbor *neighbor;
    uint16_t next_hop;

    message.Read(0, sizeof(mesh_header), &mesh_header);

    if ((neighbor = m_mle->GetNeighbor(mesh_header.GetDestination())) == NULL)
    {
        VerifyOrExit((next_hop = m_mle->GetNextHop(mesh_header.GetDestination())) != Mac::kShortAddrInvalid,
                     error = kThreadError_Drop);
        VerifyOrExit((neighbor = m_mle->GetNeighbor(next_hop)) != NULL, error = kThreadError_Drop);
    }

    // dprintf("MESH ROUTE = %02x %02x\n", mesh_header.GetDestination(), neighbor->valid.rloc16);
    m_macdst.length = 2;
    m_macdst.address16 = neighbor->valid.rloc16;
    m_macsrc.length = 2;
    m_macsrc.address16 = GetAddress16();

    m_add_mesh_header = true;
    m_meshdst = mesh_header.GetDestination();
    m_meshsrc = mesh_header.GetSource();

exit:
    return error;
}

ThreadError MeshForwarder::UpdateIp6Route(Message &message)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6_header;
    Neighbor *neighbor;

    m_add_mesh_header = false;

    message.Read(0, sizeof(ip6_header), &ip6_header);

    if (ip6_header.GetDestination()->IsLinkLocal() || ip6_header.GetDestination()->IsMulticast())
    {
        GetMacDestinationAddress(*ip6_header.GetDestination(), m_macdst);
        GetMacSourceAddress(*ip6_header.GetSource(), m_macsrc);
    }
    else if (m_mle->GetDeviceState() != Mle::kDeviceStateDetached)
    {
        // non-link-local unicast
        if (m_mle->GetDeviceMode() & Mle::kModeFFD)
        {
            // FFD - peform full routing
            if (m_mle->IsRoutingLocator(*ip6_header.GetDestination()))
            {
                m_meshdst = HostSwap16(ip6_header.GetDestination()->s6_addr16[7]);
            }
            else if ((neighbor = m_mle->GetNeighbor(*ip6_header.GetDestination())) != NULL)
            {
                m_meshdst = neighbor->valid.rloc16;
            }
            else if (m_network_data->IsOnMesh(*ip6_header.GetDestination()))
            {
                SuccessOrExit(error = m_address_resolver->Resolve(*ip6_header.GetDestination(), m_meshdst));
            }
            else
            {
                m_network_data->RouteLookup(*ip6_header.GetSource(), *ip6_header.GetDestination(), NULL, &m_meshdst);
                dprintf("found external route = %04x\n", m_meshdst);
                assert(m_meshdst != Mac::kShortAddrInvalid);
            }
        }
        else
        {
            // RFD - send to parent
            m_meshdst = m_mle->GetNextHop(Mac::kShortAddrBroadcast);
        }

        if ((m_mle->GetDeviceState() == Mle::kDeviceStateChild && m_meshdst == m_mle->GetParent()->valid.rloc16) ||
            ((m_mle->GetDeviceState() == Mle::kDeviceStateRouter || m_mle->GetDeviceState() == Mle::kDeviceStateLeader) &&
             (neighbor = m_mle->GetNeighbor(m_meshdst)) != NULL))
        {
            // destination is neighbor
            m_macdst.length = 2;
            m_macdst.address16 = m_meshdst;

            if (m_netif->IsUnicastAddress(*ip6_header.GetSource()))
            {
                GetMacSourceAddress(*ip6_header.GetSource(), m_macsrc);
            }
            else
            {
                m_macsrc.length = 2;
                m_macsrc.address16 = GetAddress16();
                assert(m_macsrc.address16 != Mac::kShortAddrInvalid);
            }
        }
        else
        {
            // destination is not neighbor
            m_meshsrc = GetAddress16();

            SuccessOrExit(error = m_mle->CheckReachability(m_meshsrc, m_meshdst, ip6_header));

            m_macdst.length = 2;
            m_macdst.address16 = m_mle->GetNextHop(m_meshdst);
            m_macsrc.length = 2;
            m_macsrc.address16 = m_meshsrc;
            m_add_mesh_header = true;
        }
    }
    else
    {
        assert(false);
        ExitNow(error = kThreadError_Drop);
    }

exit:
    return error;
}

bool MeshForwarder::GetRxOnWhenIdle()
{
    return m_mac->GetRxOnWhenIdle();
}

ThreadError MeshForwarder::SetRxOnWhenIdle(bool rx_on_when_idle)
{
    ThreadError error;

    SuccessOrExit(error = m_mac->SetRxOnWhenIdle(rx_on_when_idle));

    if (rx_on_when_idle)
    {
        m_poll_timer.Stop();
    }
    else
    {
        m_poll_timer.Start(m_poll_period);
    }

exit:
    return error;
}

ThreadError MeshForwarder::SetPollPeriod(uint32_t period)
{
    if (m_mac->GetRxOnWhenIdle() == false && m_poll_period != period)
    {
        m_poll_timer.Start(period);
    }

    m_poll_period = period;
    return kThreadError_None;
}

void MeshForwarder::HandlePollTimer(void *context)
{
    MeshForwarder *obj = reinterpret_cast<MeshForwarder *>(context);
    obj->HandlePollTimer();
}

void MeshForwarder::HandlePollTimer()
{
    Message *message;

    if ((message = Message::New(Message::kTypeMac, 0)) != NULL)
    {
        SendMessage(*message);
        dprintf("Sent poll\n");
    }

    m_poll_timer.Start(m_poll_period);
}

ThreadError MeshForwarder::GetMacSourceAddress(const Ip6Address &ipaddr, Mac::Address &macaddr)
{
    assert(!ipaddr.IsMulticast());

    macaddr.length = 8;
    memcpy(&macaddr.address64, ipaddr.s6_addr + 8, sizeof(macaddr.address64));
    macaddr.address64.bytes[0] ^= 0x02;

    if (memcmp(&macaddr.address64, m_mac->GetAddress64(), sizeof(macaddr.address64)) != 0)
    {
        macaddr.length = 2;
        macaddr.address16 = GetAddress16();
    }

    return kThreadError_None;
}

ThreadError MeshForwarder::GetMacDestinationAddress(const Ip6Address &ipaddr, Mac::Address &macaddr)
{
    if (ipaddr.IsMulticast())
    {
        macaddr.length = 2;
        macaddr.address16 = Mac::kShortAddrBroadcast;
    }
    else if (ipaddr.s6_addr16[0] == HostSwap16(0xfe80) &&
             ipaddr.s6_addr16[1] == HostSwap16(0x0000) &&
             ipaddr.s6_addr16[2] == HostSwap16(0x0000) &&
             ipaddr.s6_addr16[3] == HostSwap16(0x0000) &&
             ipaddr.s6_addr16[4] == HostSwap16(0x0000) &&
             ipaddr.s6_addr16[5] == HostSwap16(0x00ff) &&
             ipaddr.s6_addr16[6] == HostSwap16(0xfe00))
    {
        macaddr.length = 2;
        macaddr.address16 = HostSwap16(ipaddr.s6_addr16[7]);
    }
    else if (m_mle->IsRoutingLocator(ipaddr))
    {
        macaddr.length = 2;
        macaddr.address16 = HostSwap16(ipaddr.s6_addr16[7]);
    }
    else
    {
        macaddr.length = 8;
        memcpy(&macaddr.address64, ipaddr.s6_addr + 8, sizeof(macaddr.address64));
        macaddr.address64.bytes[0] ^= 0x02;
    }

    return kThreadError_None;
}

ThreadError MeshForwarder::HandleFrameRequest(void *context, Mac::Frame &frame)
{
    MeshForwarder *obj = reinterpret_cast<MeshForwarder *>(context);
    return obj->HandleFrameRequest(frame);
}

ThreadError MeshForwarder::HandleFrameRequest(Mac::Frame &frame)
{
    m_send_busy = true;
    assert(m_send_message != NULL);

    switch (m_send_message->GetType())
    {
    case Message::kTypeIp6:
        SendFragment(*m_send_message, frame);
        assert(frame.GetLength() != 7);
        break;

    case Message::kType6lo:
        SendMesh(*m_send_message, frame);
        break;

    case Message::kTypeMac:
        SendPoll(*m_send_message, frame);
        break;
    }

#if 0
    dump("sent frame", frame.GetHeader(), frame.GetLength());
#endif

    return kThreadError_None;
}

ThreadError MeshForwarder::SendPoll(Message &message, Mac::Frame &frame)
{
    Mac::Address macsrc;
    uint16_t fcf;
    Neighbor *neighbor;

    macsrc.address16 = GetAddress16();

    if (macsrc.address16 != Mac::kShortAddrInvalid)
    {
        macsrc.length = 2;
    }
    else
    {
        macsrc.length = 8;
        memcpy(&macsrc.address64, m_mac->GetAddress64(), sizeof(macsrc.address64));
    }

    // initialize MAC header
    fcf = Mac::Frame::kFcfFrameMacCmd | Mac::Frame::kFcfPanidCompression | Mac::Frame::kFcfFrameVersion2006;

    if (macsrc.length == 2)
    {
        fcf |= Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrShort;
    }
    else
    {
        fcf |= Mac::Frame::kFcfDstAddrExt | Mac::Frame::kFcfSrcAddrExt;
    }

    fcf |= Mac::Frame::kFcfAckRequest | Mac::Frame::kFcfSecurityEnabled;

    frame.InitMacHeader(fcf, Mac::Frame::kKeyIdMode1 | Mac::Frame::kSecEncMic32);
    frame.SetDstPanId(m_mac->GetPanId());

    neighbor = m_mle->GetParent();
    assert(neighbor != NULL);

    if (macsrc.length == 2)
    {
        frame.SetDstAddr(neighbor->valid.rloc16);
        frame.SetSrcAddr(macsrc.address16);
    }
    else
    {
        frame.SetDstAddr(neighbor->mac_addr);
        frame.SetSrcAddr(macsrc.address64);
    }

    frame.SetCommandId(Mac::Frame::kMacCmdDataRequest);

    m_message_next_offset = message.GetLength();

    return kThreadError_None;
}

ThreadError MeshForwarder::SendMesh(Message &message, Mac::Frame &frame)
{
    uint16_t fcf;

    // initialize MAC header
    fcf = Mac::Frame::kFcfFrameData | Mac::Frame::kFcfPanidCompression | Mac::Frame::kFcfFrameVersion2006 |
          Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrShort |
          Mac::Frame::kFcfAckRequest | Mac::Frame::kFcfSecurityEnabled;

    frame.InitMacHeader(fcf, Mac::Frame::kKeyIdMode1 | Mac::Frame::kSecEncMic32);
    frame.SetDstPanId(m_mac->GetPanId());
    frame.SetDstAddr(m_macdst.address16);
    frame.SetSrcAddr(m_macsrc.address16);

    // write payload
    if (message.GetLength() > frame.GetMaxPayloadLength())
    {
        fprintf(stderr, "%d %d\n", message.GetLength(), frame.GetMaxPayloadLength());
    }

    assert(message.GetLength() <= frame.GetMaxPayloadLength());
    message.Read(0, message.GetLength(), frame.GetPayload());
    frame.SetPayloadLength(message.GetLength());

    m_message_next_offset = message.GetLength();

    return kThreadError_None;
}

ThreadError MeshForwarder::SendFragment(Message &message, Mac::Frame &frame)
{
    Mac::Address meshdst, meshsrc;
    uint16_t fcf;
    FragmentHeader *fragment_header;
    MeshHeader *mesh_header;
    Ip6Header ip6_header;
    UdpHeader udp_header;
    uint8_t *payload;
    int header_length;
    int payload_length;
    int hc_length;
    uint16_t fragment_length;

    if (m_add_mesh_header)
    {
        meshsrc.length = 2;
        meshsrc.address16 = m_meshsrc;
        meshdst.length = 2;
        meshdst.address16 = m_meshdst;
    }
    else
    {
        meshdst = m_macdst;
        meshsrc = m_macsrc;
    }

    // initialize MAC header
    fcf = Mac::Frame::kFcfFrameData | Mac::Frame::kFcfPanidCompression | Mac::Frame::kFcfFrameVersion2006;
    fcf |= (m_macdst.length == 2) ? Mac::Frame::kFcfDstAddrShort : Mac::Frame::kFcfDstAddrExt;
    fcf |= (m_macsrc.length == 2) ? Mac::Frame::kFcfSrcAddrShort : Mac::Frame::kFcfSrcAddrExt;

    // all unicast frames request ACK
    if (m_macdst.length == 8 || m_macdst.address16 != Mac::kShortAddrBroadcast)
    {
        fcf |= Mac::Frame::kFcfAckRequest;
    }

    fcf |= Mac::Frame::kFcfSecurityEnabled;

    message.Read(0, sizeof(ip6_header), &ip6_header);

    if (ip6_header.GetNextHeader() == kProtoUdp)
    {
        message.Read(sizeof(ip6_header), sizeof(udp_header), &udp_header);

        if (udp_header.GetDestinationPort() == Mle::kUdpPort)
        {
            fcf &= ~Mac::Frame::kFcfSecurityEnabled;
        }
    }

    frame.InitMacHeader(fcf, Mac::Frame::kKeyIdMode1 | Mac::Frame::kSecEncMic32);
    frame.SetDstPanId(m_mac->GetPanId());

    if (m_macdst.length == 2)
    {
        frame.SetDstAddr(m_macdst.address16);
    }
    else
    {
        frame.SetDstAddr(m_macdst.address64);
    }

    if (m_macsrc.length == 2)
    {
        frame.SetSrcAddr(m_macsrc.address16);
    }
    else
    {
        frame.SetSrcAddr(m_macsrc.address64);
    }

    payload = frame.GetPayload();

    header_length = 0;

    // initialize Mesh header
    if (m_add_mesh_header)
    {
        mesh_header = reinterpret_cast<MeshHeader *>(payload);
        mesh_header->Init();
        mesh_header->SetHopsLeft(Lowpan::kHopsLeft);
        mesh_header->SetSource(m_meshsrc);
        mesh_header->SetDestination(m_meshdst);
        payload += mesh_header->GetHeaderLength();
        header_length += mesh_header->GetHeaderLength();
    }

    // copy IPv6 Header
    if (message.GetOffset() == 0)
    {
        hc_length = m_lowpan->Compress(message, meshsrc, meshdst, payload);
        assert(hc_length > 0);
        header_length += hc_length;

        payload_length = message.GetLength() - message.GetOffset();

        fragment_length = frame.GetMaxPayloadLength() - header_length;

        if (payload_length > fragment_length)
        {
            // write Fragment header
            message.SetDatagramTag(m_frag_tag++);
            memmove(payload + 4, payload, header_length);

            payload_length = (frame.GetMaxPayloadLength() - header_length - 4) & ~0x7;

            fragment_header = reinterpret_cast<FragmentHeader *>(payload);
            fragment_header->Init();
            fragment_header->SetSize(message.GetLength());
            fragment_header->SetTag(message.GetDatagramTag());
            fragment_header->SetOffset(0);

            payload += fragment_header->GetHeaderLength();
            header_length += fragment_header->GetHeaderLength();
        }

        payload += hc_length;

        // copy IPv6 Payload
        message.Read(message.GetOffset(), payload_length, payload);
        frame.SetPayloadLength(header_length + payload_length);

        m_message_next_offset = message.GetOffset() + payload_length;
        message.SetOffset(0);
    }
    else
    {
        payload_length = message.GetLength() - message.GetOffset();

        // write Fragment header
        fragment_header = reinterpret_cast<FragmentHeader *>(payload);
        fragment_header->Init();
        fragment_header->SetSize(message.GetLength());
        fragment_header->SetTag(message.GetDatagramTag());
        fragment_header->SetOffset(message.GetOffset());

        payload += fragment_header->GetHeaderLength();
        header_length += fragment_header->GetHeaderLength();

        fragment_length = (frame.GetMaxPayloadLength() - header_length) & ~0x7;

        if (payload_length > fragment_length)
        {
            payload_length = fragment_length;
        }

        // copy IPv6 Payload
        message.Read(message.GetOffset(), payload_length, payload);
        frame.SetPayloadLength(header_length + payload_length);

        m_message_next_offset = message.GetOffset() + payload_length;
    }

    if (m_message_next_offset < message.GetLength())
    {
        frame.SetFramePending(true);
    }

    return kThreadError_None;
}

void MeshForwarder::HandleSentFrame(void *context, Mac::Frame &frame)
{
    MeshForwarder *obj = reinterpret_cast<MeshForwarder *>(context);
    obj->HandleSentFrame(frame);
}

void MeshForwarder::HandleSentFrame(Mac::Frame &frame)
{
    Mac::Address macdst;
    Child *child;

    m_send_busy = false;

    if (!m_enabled)
    {
        ExitNow();
    }

    m_send_message->SetOffset(m_message_next_offset);

    frame.GetDstAddr(macdst);

    dprintf("sent frame %d %d\n", m_message_next_offset, m_send_message->GetLength());

    if ((child = m_mle->GetChild(macdst)) != NULL)
    {
        child->data_request = false;

        if (m_message_next_offset < m_send_message->GetLength())
        {
            child->fragment_offset = m_message_next_offset;
        }
        else
        {
            child->fragment_offset = 0;
            m_send_message->ClearChildMask(m_mle->GetChildIndex(*child));
        }
    }

    if (m_send_message->GetDirectTransmission())
    {
        if (m_message_next_offset < m_send_message->GetLength())
        {
            m_send_message->SetOffset(m_message_next_offset);
        }
        else
        {
            m_send_message->ClearDirectTransmission();
        }
    }

    if (m_send_message->GetDirectTransmission() == false && m_send_message->IsChildPending() == false)
    {
        m_send_queue.Dequeue(*m_send_message);
        Message::Free(*m_send_message);
    }

    m_schedule_transmission_task.Post();

exit:
    {}
}

void MeshForwarder::HandleReceivedFrame(void *context, Mac::Frame &frame, ThreadError error)
{
    MeshForwarder *obj = reinterpret_cast<MeshForwarder *>(context);
    obj->HandleReceivedFrame(frame, error);
}

void MeshForwarder::HandleReceivedFrame(Mac::Frame &frame, ThreadError error)
{
    ThreadMessageInfo message_info;
    Mac::Address macdst;
    Mac::Address macsrc;
    uint8_t *payload;
    uint8_t payload_length;
    Ip6Address destination;
    uint8_t command_id;

#if 0
    dump("received frame", frame.GetHeader(), frame.GetLength());
#endif

    if (!m_enabled)
    {
        ExitNow();
    }

    SuccessOrExit(frame.GetSrcAddr(macsrc));

    if (error == kThreadError_Security)
    {
        memset(&destination, 0, sizeof(destination));
        destination.s6_addr16[0] = HostSwap16(0xfe80);

        switch (macsrc.length)
        {
        case 2:
            destination.s6_addr16[5] = HostSwap16(0x00ff);
            destination.s6_addr16[6] = HostSwap16(0xfe00);
            destination.s6_addr16[7] = HostSwap16(macsrc.address16);
            break;

        case 8:
            memcpy(destination.s6_addr + 8, &macsrc.address64, sizeof(macsrc.address64));
            break;

        default:
            ExitNow();
        }

        m_mle->SendLinkReject(destination);
        ExitNow();
    }

    SuccessOrExit(frame.GetDstAddr(macdst));
    message_info.link_margin = frame.GetPower() - -100;

    payload = frame.GetPayload();
    payload_length = frame.GetPayloadLength();

    if (m_poll_timer.IsRunning() && frame.GetFramePending())
    {
        HandlePollTimer();
    }

    switch (frame.GetType())
    {
    case Mac::Frame::kFcfFrameData:
        if ((payload[0] & MeshHeader::kDispatchMask) == MeshHeader::kDispatch)
        {
            HandleMesh(payload, payload_length, macsrc, macdst, message_info);
        }
        else if ((payload[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch)
        {
            HandleFragment(payload, payload_length, macsrc, macdst, message_info);
        }
        else if ((payload[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8))
        {
            HandleLowpanHC(payload, payload_length, macsrc, macdst, message_info);
        }

        break;

    case Mac::Frame::kFcfFrameMacCmd:
        frame.GetCommandId(command_id);

        if (command_id == Mac::Frame::kMacCmdDataRequest)
        {
            HandleDataRequest(macsrc);
        }

        break;
    }

exit:
    {}
}

void MeshForwarder::HandleMesh(uint8_t *frame, uint8_t frame_length,
                               const Mac::Address &macsrc, const Mac::Address &macdst,
                               const ThreadMessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Message *message = NULL;
    Mac::Address meshdst;
    Mac::Address meshsrc;
    MeshHeader *mesh_header = reinterpret_cast<MeshHeader *>(frame);

    VerifyOrExit(mesh_header->IsValid(), error = kThreadError_Drop);

    meshsrc.length = 2;
    meshsrc.address16 = mesh_header->GetSource();
    meshdst.length = 2;
    meshdst.address16 = mesh_header->GetDestination();

    if (meshdst.address16 == GetAddress16())
    {
        frame += 5;
        frame_length -= 5;

        if ((frame[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch)
        {
            HandleFragment(frame, frame_length, meshsrc, meshdst, message_info);
        }
        else if ((frame[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8))
        {
            HandleLowpanHC(frame, frame_length, meshsrc, meshdst, message_info);
        }
        else
        {
            ExitNow();
        }
    }
    else if (mesh_header->GetHopsLeft() > 0)
    {
        SuccessOrExit(error = CheckReachability(frame, frame_length, meshsrc, meshdst));

        mesh_header->SetHopsLeft(mesh_header->GetHopsLeft() - 1);

        VerifyOrExit((message = Message::New(Message::kType6lo, 0)) != NULL, error = kThreadError_Drop);
        SuccessOrExit(error = message->SetLength(frame_length));
        message->Write(0, frame_length, frame);

        SendMessage(*message);
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError MeshForwarder::CheckReachability(uint8_t *frame, uint8_t frame_length,
                                             const Mac::Address &meshsrc, const Mac::Address &meshdst)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6_header;

    // skip mesh header
    frame += 5;

    // skip fragment header
    if ((frame[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch)
    {
        VerifyOrExit((frame[0] & FragmentHeader::kOffset) == 0, ;);
        frame += 4;
    }

    // only process IPv6 packets
    VerifyOrExit((frame[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8), ;);

    m_lowpan->DecompressBaseHeader(ip6_header, meshsrc, meshdst, frame);

    error = m_mle->CheckReachability(meshsrc.address16, meshdst.address16, ip6_header);

exit:
    return error;
}

void MeshForwarder::HandleFragment(uint8_t *frame, uint8_t frame_length,
                                   const Mac::Address &macsrc, const Mac::Address &macdst,
                                   const ThreadMessageInfo &message_info)
{
    FragmentHeader *fragment_header = reinterpret_cast<FragmentHeader *>(frame);
    uint16_t datagram_length = fragment_header->GetSize();
    uint16_t datagram_tag = fragment_header->GetTag();
    Message *message;
    int header_length;

    if (fragment_header->GetOffset() == 0)
    {
        frame += 4;
        frame_length -= 4;

        VerifyOrExit((message = Message::New(Message::kTypeIp6, 0)) != NULL, ;);
        header_length = m_lowpan->Decompress(*message, macsrc, macdst, frame, frame_length, datagram_length);
        VerifyOrExit(header_length > 0, Message::Free(*message));
        frame += header_length;
        frame_length -= header_length;

        VerifyOrExit(message->SetLength(datagram_length) == kThreadError_None, Message::Free(*message));
        datagram_length = HostSwap16(datagram_length - sizeof(Ip6Header));
        message->Write(Ip6Header::GetPayloadLengthOffset(), sizeof(datagram_length), &datagram_length);
        message->SetDatagramTag(datagram_tag);
        message->SetTimeout(kReassemblyTimeout);

        m_reassembly_list.Enqueue(*message);

        if (!m_reassembly_timer.IsRunning())
        {
            m_reassembly_timer.Start(1000);
        }
    }
    else
    {
        frame += 5;
        frame_length -= 5;

        for (message = m_reassembly_list.GetHead(); message; message = message->GetNext())
        {
            if (message->GetLength() == datagram_length &&
                message->GetDatagramTag() == datagram_tag &&
                message->GetOffset() == fragment_header->GetOffset())
            {
                break;
            }
        }

        VerifyOrExit(message != NULL, ;);
    }

    assert(message != NULL);

    // copy Fragment
    message->Write(message->GetOffset(), frame_length, frame);
    message->MoveOffset(frame_length);
    VerifyOrExit(message->GetOffset() >= message->GetLength(), ;);

    m_reassembly_list.Dequeue(*message);
    Ip6::HandleDatagram(*message, m_netif, m_netif->GetInterfaceId(), &message_info, false);

exit:
    {}
}

void MeshForwarder::HandleReassemblyTimer(void *context)
{
    MeshForwarder *obj = reinterpret_cast<MeshForwarder *>(context);
    obj->HandleReassemblyTimer();
}

void MeshForwarder::HandleReassemblyTimer()
{
    Message *next = NULL;
    uint8_t timeout;

    for (Message *message = m_reassembly_list.GetHead(); message; message = next)
    {
        next = message->GetNext();
        timeout = message->GetTimeout();

        if (timeout > 0)
        {
            message->SetTimeout(timeout - 1);
        }
        else
        {
            m_reassembly_list.Dequeue(*message);
            Message::Free(*message);
        }
    }

    if (m_reassembly_list.GetHead() != NULL)
    {
        m_reassembly_timer.Start(1000);
    }
}

void MeshForwarder::HandleLowpanHC(uint8_t *frame, uint8_t frame_length,
                                   const Mac::Address &macsrc, const Mac::Address &macdst,
                                   const ThreadMessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Message *message;
    int header_length;
    uint16_t ip6_payload_length;

    VerifyOrExit((message = Message::New(Message::kTypeIp6, 0)) != NULL, ;);

    header_length = m_lowpan->Decompress(*message, macsrc, macdst, frame, frame_length, 0);
    VerifyOrExit(header_length > 0, ;);
    frame += header_length;
    frame_length -= header_length;

    SuccessOrExit(error = message->SetLength(message->GetLength() + frame_length));

    ip6_payload_length = HostSwap16(message->GetLength() - sizeof(Ip6Header));
    message->Write(Ip6Header::GetPayloadLengthOffset(), sizeof(ip6_payload_length), &ip6_payload_length);

    message->Write(message->GetOffset(), frame_length, frame);
    Ip6::HandleDatagram(*message, m_netif, m_netif->GetInterfaceId(), &message_info, false);

exit:

    if (error != kThreadError_None)
    {
        Message::Free(*message);
    }
}

void MeshForwarder::UpdateFramePending()
{
}

void MeshForwarder::HandleDataRequest(const Mac::Address &macsrc)
{
    Neighbor *neighbor;
    int child_index;

    assert(m_mle->GetDeviceState() != Mle::kDeviceStateDetached);

    VerifyOrExit((neighbor = m_mle->GetNeighbor(macsrc)) != NULL, ;);
    neighbor->last_heard = Timer::GetNow();

    m_mle->HandleMacDataRequest(*reinterpret_cast<Child *>(neighbor));
    child_index = m_mle->GetChildIndex(*reinterpret_cast<Child *>(neighbor));

    for (Message *message = m_send_queue.GetHead(); message; message = message->GetNext())
    {
        if (message->GetDirectTransmission() == false && message->GetChildMask(child_index))
        {
            neighbor->data_request = true;
            break;
        }
    }

    m_schedule_transmission_task.Post();

exit:
    {}
}

}  // namespace Thread
