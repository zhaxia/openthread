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
#include <common/encoding.hpp>
#include <common/message.hpp>
#include <common/random.hpp>
#include <common/thread_error.hpp>
#include <net/ip6.hpp>
#include <net/netif.hpp>
#include <net/udp6.hpp>
#include <thread/mesh_forwarder.hpp>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

MeshForwarder::MeshForwarder(ThreadNetif &netif):
    mMacReceiver(&HandleReceivedFrame, this),
    mMacSender(&HandleFrameRequest, &HandleSentFrame, this),
    mPollTimer(&HandlePollTimer, this),
    mReassemblyTimer(&HandleReassemblyTimer, this),
    mScheduleTransmissionTask(ScheduleTransmissionTask, this)
{
    mAddressResolver = netif.GetAddressResolver();
    mLowpan = netif.GetLowpan();
    mMac = netif.GetMac();
    mMle = netif.GetMle();
    mNetif = &netif;
    mNetworkData = netif.GetNetworkDataLeader();
    mFragTag = Random::Get();
}

ThreadError MeshForwarder::Start()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mEnabled == false, error = kThreadError_Busy);

    mMac->RegisterReceiver(mMacReceiver);
    SuccessOrExit(error = mMac->Start());

    mEnabled = true;

exit:
    return error;
}

ThreadError MeshForwarder::Stop()
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit(mEnabled == true, error = kThreadError_Busy);

    mPollTimer.Stop();
    mReassemblyTimer.Stop();

    while ((message = mSendQueue.GetHead()) != NULL)
    {
        mSendQueue.Dequeue(*message);
        Message::Free(*message);
    }

    while ((message = mReassemblyList.GetHead()) != NULL)
    {
        mReassemblyList.Dequeue(*message);
        Message::Free(*message);
    }

    mEnabled = false;

    SuccessOrExit(error = mMac->Stop());

exit:
    return error;
}

const Mac::Address64 *MeshForwarder::GetAddress64() const
{
    return mMac->GetAddress64();
}

Mac::Address16 MeshForwarder::GetAddress16() const
{
    return mMac->GetAddress16();
}

ThreadError MeshForwarder::SetAddress16(Mac::Address16 address16)
{
    mMac->SetAddress16(address16);
    return kThreadError_None;
}

void MeshForwarder::HandleResolved(const Ip6Address &eid)
{
    Message *cur, *next;
    Ip6Address ip6Dst;

    for (cur = mResolvingQueue.GetHead(); cur; cur = next)
    {
        next = cur->GetNext();

        if (cur->GetType() != Message::kTypeIp6)
        {
            continue;
        }

        cur->Read(Ip6Header::GetDestinationOffset(), sizeof(ip6Dst), &ip6Dst);

        if (memcmp(&ip6Dst, &eid, sizeof(ip6Dst)) == 0)
        {
            mResolvingQueue.Dequeue(*cur);
            mSendQueue.Enqueue(*cur);
        }
    }

    mScheduleTransmissionTask.Post();
}

void MeshForwarder::ScheduleTransmissionTask(void *context)
{
    MeshForwarder *obj = reinterpret_cast<MeshForwarder *>(context);
    obj->ScheduleTransmissionTask();
}

void MeshForwarder::ScheduleTransmissionTask()
{
    ThreadError error = kThreadError_None;
    uint8_t numChildren;
    Child *children;

    VerifyOrExit(mSendBusy == false, error = kThreadError_Busy);

    children = mMle->GetChildren(&numChildren);

    for (int i = 0; i < numChildren; i++)
    {
        if (children[i].mState == Child::kStateValid &&
            children[i].mDataRequest &&
            (mSendMessage = GetIndirectTransmission(children[i])) != NULL)
        {
            mMac->SendFrameRequest(mMacSender);
            ExitNow();
        }
    }

    if ((mSendMessage = GetDirectTransmission()) != NULL)
    {
        mMac->SendFrameRequest(mMacSender);
        ExitNow();
    }

exit:
    (void) error;
}

ThreadError MeshForwarder::SendMessage(Message &message)
{
    ThreadError error = kThreadError_None;
    Neighbor *neighbor;
    Ip6Header ip6Header;
    uint8_t numChildren;
    Child *children;
    MeshHeader meshHeader;

    switch (message.GetType())
    {
    case Message::kTypeIp6:
        message.Read(0, sizeof(ip6Header), &ip6Header);

        if (!memcmp(ip6Header.GetDestination(), mMle->GetLinkLocalAllThreadNodesAddress(),
                    sizeof(*ip6Header.GetDestination())) ||
            !memcmp(ip6Header.GetDestination(), mMle->GetRealmLocalAllThreadNodesAddress(), sizeof(*ip6Header.GetDestination())))
        {
            // schedule direct transmission
            message.SetDirectTransmission();

            // destined for all sleepy children
            children = mMle->GetChildren(&numChildren);

            for (int i = 0; i < numChildren; i++)
            {
                if (children[i].mState == Neighbor::kStateValid && (children[i].mMode & Mle::kModeRxOnWhenIdle) == 0)
                {
                    message.SetChildMask(i);
                }
            }
        }
        else if ((neighbor = mMle->GetNeighbor(*ip6Header.GetDestination())) != NULL &&
                 (neighbor->mMode & Mle::kModeRxOnWhenIdle) == 0)
        {
            // destined for a sleepy child
            message.SetChildMask(mMle->GetChildIndex(*reinterpret_cast<Child *>(neighbor)));
        }
        else
        {
            // schedule direct transmission
            message.SetDirectTransmission();
        }

        break;

    case Message::kType6lo:
        message.Read(0, sizeof(meshHeader), &meshHeader);

        if ((neighbor = mMle->GetNeighbor(meshHeader.GetDestination())) != NULL &&
            (neighbor->mMode & Mle::kModeRxOnWhenIdle) == 0)
        {
            // destined for a sleepy child
            message.SetChildMask(mMle->GetChildIndex(*reinterpret_cast<Child *>(neighbor)));
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
    SuccessOrExit(error = mSendQueue.Enqueue(message));
    mScheduleTransmissionTask.Post();

exit:
    return error;
}

void MeshForwarder::MoveToResolving(const Ip6Address &destination)
{
    Message *cur, *next;
    Ip6Address ip6Dst;

    for (cur = mSendQueue.GetHead(); cur; cur = next)
    {
        next = cur->GetNext();

        if (cur->GetType() != Message::kTypeIp6)
        {
            continue;
        }

        cur->Read(Ip6Header::GetDestinationOffset(), sizeof(ip6Dst), &ip6Dst);

        if (memcmp(&ip6Dst, &destination, sizeof(ip6Dst)) == 0)
        {
            mSendQueue.Dequeue(*cur);
            mResolvingQueue.Enqueue(*cur);
        }
    }
}

Message *MeshForwarder::GetDirectTransmission()
{
    Message *curMessage, *nextMessage;
    ThreadError error = kThreadError_None;
    Ip6Address ip6Dst;

    for (curMessage = mSendQueue.GetHead(); curMessage; curMessage = nextMessage)
    {
        nextMessage = curMessage->GetNext();

        if (curMessage->GetDirectTransmission() == false)
        {
            continue;
        }

        switch (curMessage->GetType())
        {
        case Message::kTypeIp6:
            error = UpdateIp6Route(*curMessage);
            break;

        case Message::kType6lo:
            error = UpdateMeshRoute(*curMessage);
            break;

        case Message::kTypeMac:
            ExitNow();
        }

        switch (error)
        {
        case kThreadError_None:
            ExitNow();

        case kThreadError_LeaseQuery:
            curMessage->Read(Ip6Header::GetDestinationOffset(), sizeof(ip6Dst), &ip6Dst);
            MoveToResolving(ip6Dst);
            continue;

        case kThreadError_Drop:
        case kThreadError_NoBufs:
            mSendQueue.Dequeue(*curMessage);
            Message::Free(*curMessage);
            continue;

        default:
            dprintf("error = %d\n", error);
            assert(false);
            break;
        }
    }

exit:
    return curMessage;
}

Message *MeshForwarder::GetIndirectTransmission(const Child &child)
{
    Message *message = NULL;
    int childIndex = mMle->GetChildIndex(child);
    Ip6Header ip6Header;
    MeshHeader meshHeader;

    for (message = mSendQueue.GetHead(); message; message = message->GetNext())
    {
        if (message->GetChildMask(childIndex))
        {
            break;
        }
    }

    VerifyOrExit(message != NULL, ;);

    switch (message->GetType())
    {
    case Message::kTypeIp6:
        message->Read(0, sizeof(ip6Header), &ip6Header);

        mAddMeshHeader = false;
        GetMacSourceAddress(*ip6Header.GetSource(), mMacsrc);

        if (ip6Header.GetDestination()->IsLinkLocal() || ip6Header.GetDestination()->IsMulticast())
        {
            GetMacDestinationAddress(*ip6Header.GetDestination(), mMacdst);
        }
        else
        {
            mMacdst.mLength = 2;
            mMacdst.mAddress16 = child.mValid.mRloc16;
        }

        break;

    case Message::kType6lo:
        message->Read(0, sizeof(meshHeader), &meshHeader);

        mAddMeshHeader = true;
        mMeshdst = meshHeader.GetDestination();
        mMeshsrc = meshHeader.GetSource();
        mMacsrc.mLength = 2;
        mMacsrc.mAddress16 = GetAddress16();
        mMacdst.mLength = 2;
        mMacdst.mAddress16 = meshHeader.GetDestination();
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
    MeshHeader meshHeader;
    Neighbor *neighbor;
    uint16_t nextHop;

    message.Read(0, sizeof(meshHeader), &meshHeader);

    if ((neighbor = mMle->GetNeighbor(meshHeader.GetDestination())) == NULL)
    {
        VerifyOrExit((nextHop = mMle->GetNextHop(meshHeader.GetDestination())) != Mac::kShortAddrInvalid,
                     error = kThreadError_Drop);
        VerifyOrExit((neighbor = mMle->GetNeighbor(nextHop)) != NULL, error = kThreadError_Drop);
    }

    // dprintf("MESH ROUTE = %02x %02x\n", meshHeader.GetDestination(), neighbor->mValid.mRloc16);
    mMacdst.mLength = 2;
    mMacdst.mAddress16 = neighbor->mValid.mRloc16;
    mMacsrc.mLength = 2;
    mMacsrc.mAddress16 = GetAddress16();

    mAddMeshHeader = true;
    mMeshdst = meshHeader.GetDestination();
    mMeshsrc = meshHeader.GetSource();

exit:
    return error;
}

ThreadError MeshForwarder::UpdateIp6Route(Message &message)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6Header;
    Neighbor *neighbor;

    mAddMeshHeader = false;

    message.Read(0, sizeof(ip6Header), &ip6Header);

    if (ip6Header.GetDestination()->IsLinkLocal() || ip6Header.GetDestination()->IsMulticast())
    {
        GetMacDestinationAddress(*ip6Header.GetDestination(), mMacdst);
        GetMacSourceAddress(*ip6Header.GetSource(), mMacsrc);
    }
    else if (mMle->GetDeviceState() != Mle::kDeviceStateDetached)
    {
        // non-link-local unicast
        if (mMle->GetDeviceMode() & Mle::kModeFFD)
        {
            // FFD - peform full routing
            if (mMle->IsRoutingLocator(*ip6Header.GetDestination()))
            {
                mMeshdst = HostSwap16(ip6Header.GetDestination()->m16[7]);
            }
            else if ((neighbor = mMle->GetNeighbor(*ip6Header.GetDestination())) != NULL)
            {
                mMeshdst = neighbor->mValid.mRloc16;
            }
            else if (mNetworkData->IsOnMesh(*ip6Header.GetDestination()))
            {
                SuccessOrExit(error = mAddressResolver->Resolve(*ip6Header.GetDestination(), mMeshdst));
            }
            else
            {
                mNetworkData->RouteLookup(*ip6Header.GetSource(), *ip6Header.GetDestination(), NULL, &mMeshdst);
                dprintf("found external route = %04x\n", mMeshdst);
                assert(mMeshdst != Mac::kShortAddrInvalid);
            }
        }
        else
        {
            // RFD - send to parent
            mMeshdst = mMle->GetNextHop(Mac::kShortAddrBroadcast);
        }

        if ((mMle->GetDeviceState() == Mle::kDeviceStateChild && mMeshdst == mMle->GetParent()->mValid.mRloc16) ||
            ((mMle->GetDeviceState() == Mle::kDeviceStateRouter || mMle->GetDeviceState() == Mle::kDeviceStateLeader) &&
             (neighbor = mMle->GetNeighbor(mMeshdst)) != NULL))
        {
            // destination is neighbor
            mMacdst.mLength = 2;
            mMacdst.mAddress16 = mMeshdst;

            if (mNetif->IsUnicastAddress(*ip6Header.GetSource()))
            {
                GetMacSourceAddress(*ip6Header.GetSource(), mMacsrc);
            }
            else
            {
                mMacsrc.mLength = 2;
                mMacsrc.mAddress16 = GetAddress16();
                assert(mMacsrc.mAddress16 != Mac::kShortAddrInvalid);
            }
        }
        else
        {
            // destination is not neighbor
            mMeshsrc = GetAddress16();

            SuccessOrExit(error = mMle->CheckReachability(mMeshsrc, mMeshdst, ip6Header));

            mMacdst.mLength = 2;
            mMacdst.mAddress16 = mMle->GetNextHop(mMeshdst);
            mMacsrc.mLength = 2;
            mMacsrc.mAddress16 = mMeshsrc;
            mAddMeshHeader = true;
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
    return mMac->GetRxOnWhenIdle();
}

ThreadError MeshForwarder::SetRxOnWhenIdle(bool rxOnWhenIdle)
{
    ThreadError error;

    SuccessOrExit(error = mMac->SetRxOnWhenIdle(rxOnWhenIdle));

    if (rxOnWhenIdle)
    {
        mPollTimer.Stop();
    }
    else
    {
        mPollTimer.Start(mPollPeriod);
    }

exit:
    return error;
}

ThreadError MeshForwarder::SetPollPeriod(uint32_t period)
{
    if (mMac->GetRxOnWhenIdle() == false && mPollPeriod != period)
    {
        mPollTimer.Start(period);
    }

    mPollPeriod = period;
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

    mPollTimer.Start(mPollPeriod);
}

ThreadError MeshForwarder::GetMacSourceAddress(const Ip6Address &ipaddr, Mac::Address &macaddr)
{
    assert(!ipaddr.IsMulticast());

    macaddr.mLength = 8;
    memcpy(&macaddr.mAddress64, ipaddr.m8 + 8, sizeof(macaddr.mAddress64));
    macaddr.mAddress64.mBytes[0] ^= 0x02;

    if (memcmp(&macaddr.mAddress64, mMac->GetAddress64(), sizeof(macaddr.mAddress64)) != 0)
    {
        macaddr.mLength = 2;
        macaddr.mAddress16 = GetAddress16();
    }

    return kThreadError_None;
}

ThreadError MeshForwarder::GetMacDestinationAddress(const Ip6Address &ipaddr, Mac::Address &macaddr)
{
    if (ipaddr.IsMulticast())
    {
        macaddr.mLength = 2;
        macaddr.mAddress16 = Mac::kShortAddrBroadcast;
    }
    else if (ipaddr.m16[0] == HostSwap16(0xfe80) &&
             ipaddr.m16[1] == HostSwap16(0x0000) &&
             ipaddr.m16[2] == HostSwap16(0x0000) &&
             ipaddr.m16[3] == HostSwap16(0x0000) &&
             ipaddr.m16[4] == HostSwap16(0x0000) &&
             ipaddr.m16[5] == HostSwap16(0x00ff) &&
             ipaddr.m16[6] == HostSwap16(0xfe00))
    {
        macaddr.mLength = 2;
        macaddr.mAddress16 = HostSwap16(ipaddr.m16[7]);
    }
    else if (mMle->IsRoutingLocator(ipaddr))
    {
        macaddr.mLength = 2;
        macaddr.mAddress16 = HostSwap16(ipaddr.m16[7]);
    }
    else
    {
        macaddr.mLength = 8;
        memcpy(&macaddr.mAddress64, ipaddr.m8 + 8, sizeof(macaddr.mAddress64));
        macaddr.mAddress64.mBytes[0] ^= 0x02;
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
    mSendBusy = true;
    assert(mSendMessage != NULL);

    switch (mSendMessage->GetType())
    {
    case Message::kTypeIp6:
        SendFragment(*mSendMessage, frame);
        assert(frame.GetLength() != 7);
        break;

    case Message::kType6lo:
        SendMesh(*mSendMessage, frame);
        break;

    case Message::kTypeMac:
        SendPoll(*mSendMessage, frame);
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

    macsrc.mAddress16 = GetAddress16();

    if (macsrc.mAddress16 != Mac::kShortAddrInvalid)
    {
        macsrc.mLength = 2;
    }
    else
    {
        macsrc.mLength = 8;
        memcpy(&macsrc.mAddress64, mMac->GetAddress64(), sizeof(macsrc.mAddress64));
    }

    // initialize MAC header
    fcf = Mac::Frame::kFcfFrameMacCmd | Mac::Frame::kFcfPanidCompression | Mac::Frame::kFcfFrameVersion2006;

    if (macsrc.mLength == 2)
    {
        fcf |= Mac::Frame::kFcfDstAddrShort | Mac::Frame::kFcfSrcAddrShort;
    }
    else
    {
        fcf |= Mac::Frame::kFcfDstAddrExt | Mac::Frame::kFcfSrcAddrExt;
    }

    fcf |= Mac::Frame::kFcfAckRequest | Mac::Frame::kFcfSecurityEnabled;

    frame.InitMacHeader(fcf, Mac::Frame::kKeyIdMode1 | Mac::Frame::kSecEncMic32);
    frame.SetDstPanId(mMac->GetPanId());

    neighbor = mMle->GetParent();
    assert(neighbor != NULL);

    if (macsrc.mLength == 2)
    {
        frame.SetDstAddr(neighbor->mValid.mRloc16);
        frame.SetSrcAddr(macsrc.mAddress16);
    }
    else
    {
        frame.SetDstAddr(neighbor->mMacAddr);
        frame.SetSrcAddr(macsrc.mAddress64);
    }

    frame.SetCommandId(Mac::Frame::kMacCmdDataRequest);

    mMessageNextOffset = message.GetLength();

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
    frame.SetDstPanId(mMac->GetPanId());
    frame.SetDstAddr(mMacdst.mAddress16);
    frame.SetSrcAddr(mMacsrc.mAddress16);

    // write payload
    if (message.GetLength() > frame.GetMaxPayloadLength())
    {
        fprintf(stderr, "%d %d\n", message.GetLength(), frame.GetMaxPayloadLength());
    }

    assert(message.GetLength() <= frame.GetMaxPayloadLength());
    message.Read(0, message.GetLength(), frame.GetPayload());
    frame.SetPayloadLength(message.GetLength());

    mMessageNextOffset = message.GetLength();

    return kThreadError_None;
}

ThreadError MeshForwarder::SendFragment(Message &message, Mac::Frame &frame)
{
    Mac::Address meshdst, meshsrc;
    uint16_t fcf;
    FragmentHeader *fragmentHeader;
    MeshHeader *meshHeader;
    Ip6Header ip6Header;
    UdpHeader udpHeader;
    uint8_t *payload;
    int headerLength;
    int payloadLength;
    int hcLength;
    uint16_t fragmentLength;

    if (mAddMeshHeader)
    {
        meshsrc.mLength = 2;
        meshsrc.mAddress16 = mMeshsrc;
        meshdst.mLength = 2;
        meshdst.mAddress16 = mMeshdst;
    }
    else
    {
        meshdst = mMacdst;
        meshsrc = mMacsrc;
    }

    // initialize MAC header
    fcf = Mac::Frame::kFcfFrameData | Mac::Frame::kFcfPanidCompression | Mac::Frame::kFcfFrameVersion2006;
    fcf |= (mMacdst.mLength == 2) ? Mac::Frame::kFcfDstAddrShort : Mac::Frame::kFcfDstAddrExt;
    fcf |= (mMacsrc.mLength == 2) ? Mac::Frame::kFcfSrcAddrShort : Mac::Frame::kFcfSrcAddrExt;

    // all unicast frames request ACK
    if (mMacdst.mLength == 8 || mMacdst.mAddress16 != Mac::kShortAddrBroadcast)
    {
        fcf |= Mac::Frame::kFcfAckRequest;
    }

    fcf |= Mac::Frame::kFcfSecurityEnabled;

    message.Read(0, sizeof(ip6Header), &ip6Header);

    if (ip6Header.GetNextHeader() == kProtoUdp)
    {
        message.Read(sizeof(ip6Header), sizeof(udpHeader), &udpHeader);

        if (udpHeader.GetDestinationPort() == Mle::kUdpPort)
        {
            fcf &= ~Mac::Frame::kFcfSecurityEnabled;
        }
    }

    frame.InitMacHeader(fcf, Mac::Frame::kKeyIdMode1 | Mac::Frame::kSecEncMic32);
    frame.SetDstPanId(mMac->GetPanId());

    if (mMacdst.mLength == 2)
    {
        frame.SetDstAddr(mMacdst.mAddress16);
    }
    else
    {
        frame.SetDstAddr(mMacdst.mAddress64);
    }

    if (mMacsrc.mLength == 2)
    {
        frame.SetSrcAddr(mMacsrc.mAddress16);
    }
    else
    {
        frame.SetSrcAddr(mMacsrc.mAddress64);
    }

    payload = frame.GetPayload();

    headerLength = 0;

    // initialize Mesh header
    if (mAddMeshHeader)
    {
        meshHeader = reinterpret_cast<MeshHeader *>(payload);
        meshHeader->Init();
        meshHeader->SetHopsLeft(Lowpan::kHopsLeft);
        meshHeader->SetSource(mMeshsrc);
        meshHeader->SetDestination(mMeshdst);
        payload += meshHeader->GetHeaderLength();
        headerLength += meshHeader->GetHeaderLength();
    }

    // copy IPv6 Header
    if (message.GetOffset() == 0)
    {
        hcLength = mLowpan->Compress(message, meshsrc, meshdst, payload);
        assert(hcLength > 0);
        headerLength += hcLength;

        payloadLength = message.GetLength() - message.GetOffset();

        fragmentLength = frame.GetMaxPayloadLength() - headerLength;

        if (payloadLength > fragmentLength)
        {
            // write Fragment header
            message.SetDatagramTag(mFragTag++);
            memmove(payload + 4, payload, headerLength);

            payloadLength = (frame.GetMaxPayloadLength() - headerLength - 4) & ~0x7;

            fragmentHeader = reinterpret_cast<FragmentHeader *>(payload);
            fragmentHeader->Init();
            fragmentHeader->SetSize(message.GetLength());
            fragmentHeader->SetTag(message.GetDatagramTag());
            fragmentHeader->SetOffset(0);

            payload += fragmentHeader->GetHeaderLength();
            headerLength += fragmentHeader->GetHeaderLength();
        }

        payload += hcLength;

        // copy IPv6 Payload
        message.Read(message.GetOffset(), payloadLength, payload);
        frame.SetPayloadLength(headerLength + payloadLength);

        mMessageNextOffset = message.GetOffset() + payloadLength;
        message.SetOffset(0);
    }
    else
    {
        payloadLength = message.GetLength() - message.GetOffset();

        // write Fragment header
        fragmentHeader = reinterpret_cast<FragmentHeader *>(payload);
        fragmentHeader->Init();
        fragmentHeader->SetSize(message.GetLength());
        fragmentHeader->SetTag(message.GetDatagramTag());
        fragmentHeader->SetOffset(message.GetOffset());

        payload += fragmentHeader->GetHeaderLength();
        headerLength += fragmentHeader->GetHeaderLength();

        fragmentLength = (frame.GetMaxPayloadLength() - headerLength) & ~0x7;

        if (payloadLength > fragmentLength)
        {
            payloadLength = fragmentLength;
        }

        // copy IPv6 Payload
        message.Read(message.GetOffset(), payloadLength, payload);
        frame.SetPayloadLength(headerLength + payloadLength);

        mMessageNextOffset = message.GetOffset() + payloadLength;
    }

    if (mMessageNextOffset < message.GetLength())
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

    mSendBusy = false;

    if (!mEnabled)
    {
        ExitNow();
    }

    mSendMessage->SetOffset(mMessageNextOffset);

    frame.GetDstAddr(macdst);

    dprintf("sent frame %d %d\n", mMessageNextOffset, mSendMessage->GetLength());

    if ((child = mMle->GetChild(macdst)) != NULL)
    {
        child->mDataRequest = false;

        if (mMessageNextOffset < mSendMessage->GetLength())
        {
            child->mFragmentOffset = mMessageNextOffset;
        }
        else
        {
            child->mFragmentOffset = 0;
            mSendMessage->ClearChildMask(mMle->GetChildIndex(*child));
        }
    }

    if (mSendMessage->GetDirectTransmission())
    {
        if (mMessageNextOffset < mSendMessage->GetLength())
        {
            mSendMessage->SetOffset(mMessageNextOffset);
        }
        else
        {
            mSendMessage->ClearDirectTransmission();
        }
    }

    if (mSendMessage->GetDirectTransmission() == false && mSendMessage->IsChildPending() == false)
    {
        mSendQueue.Dequeue(*mSendMessage);
        Message::Free(*mSendMessage);
    }

    mScheduleTransmissionTask.Post();

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
    ThreadMessageInfo messageInfo;
    Mac::Address macdst;
    Mac::Address macsrc;
    uint8_t *payload;
    uint8_t payloadLength;
    Ip6Address destination;
    uint8_t commandId;

#if 0
    dump("received frame", frame.GetHeader(), frame.GetLength());
#endif

    if (!mEnabled)
    {
        ExitNow();
    }

    SuccessOrExit(frame.GetSrcAddr(macsrc));

    if (error == kThreadError_Security)
    {
        memset(&destination, 0, sizeof(destination));
        destination.m16[0] = HostSwap16(0xfe80);

        switch (macsrc.mLength)
        {
        case 2:
            destination.m16[5] = HostSwap16(0x00ff);
            destination.m16[6] = HostSwap16(0xfe00);
            destination.m16[7] = HostSwap16(macsrc.mAddress16);
            break;

        case 8:
            memcpy(destination.m8 + 8, &macsrc.mAddress64, sizeof(macsrc.mAddress64));
            break;

        default:
            ExitNow();
        }

        mMle->SendLinkReject(destination);
        ExitNow();
    }

    SuccessOrExit(frame.GetDstAddr(macdst));
    messageInfo.mLinkMargin = frame.GetPower() - -100;

    payload = frame.GetPayload();
    payloadLength = frame.GetPayloadLength();

    if (mPollTimer.IsRunning() && frame.GetFramePending())
    {
        HandlePollTimer();
    }

    switch (frame.GetType())
    {
    case Mac::Frame::kFcfFrameData:
        if ((payload[0] & MeshHeader::kDispatchMask) == MeshHeader::kDispatch)
        {
            HandleMesh(payload, payloadLength, macsrc, macdst, messageInfo);
        }
        else if ((payload[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch)
        {
            HandleFragment(payload, payloadLength, macsrc, macdst, messageInfo);
        }
        else if ((payload[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8))
        {
            HandleLowpanHC(payload, payloadLength, macsrc, macdst, messageInfo);
        }

        break;

    case Mac::Frame::kFcfFrameMacCmd:
        frame.GetCommandId(commandId);

        if (commandId == Mac::Frame::kMacCmdDataRequest)
        {
            HandleDataRequest(macsrc);
        }

        break;
    }

exit:
    {}
}

void MeshForwarder::HandleMesh(uint8_t *frame, uint8_t frameLength,
                               const Mac::Address &macsrc, const Mac::Address &macdst,
                               const ThreadMessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Message *message = NULL;
    Mac::Address meshdst;
    Mac::Address meshsrc;
    MeshHeader *meshHeader = reinterpret_cast<MeshHeader *>(frame);

    VerifyOrExit(meshHeader->IsValid(), error = kThreadError_Drop);

    meshsrc.mLength = 2;
    meshsrc.mAddress16 = meshHeader->GetSource();
    meshdst.mLength = 2;
    meshdst.mAddress16 = meshHeader->GetDestination();

    if (meshdst.mAddress16 == GetAddress16())
    {
        frame += 5;
        frameLength -= 5;

        if ((frame[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch)
        {
            HandleFragment(frame, frameLength, meshsrc, meshdst, messageInfo);
        }
        else if ((frame[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8))
        {
            HandleLowpanHC(frame, frameLength, meshsrc, meshdst, messageInfo);
        }
        else
        {
            ExitNow();
        }
    }
    else if (meshHeader->GetHopsLeft() > 0)
    {
        SuccessOrExit(error = CheckReachability(frame, frameLength, meshsrc, meshdst));

        meshHeader->SetHopsLeft(meshHeader->GetHopsLeft() - 1);

        VerifyOrExit((message = Message::New(Message::kType6lo, 0)) != NULL, error = kThreadError_Drop);
        SuccessOrExit(error = message->SetLength(frameLength));
        message->Write(0, frameLength, frame);

        SendMessage(*message);
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError MeshForwarder::CheckReachability(uint8_t *frame, uint8_t frameLength,
                                             const Mac::Address &meshsrc, const Mac::Address &meshdst)
{
    ThreadError error = kThreadError_None;
    Ip6Header ip6Header;

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

    mLowpan->DecompressBaseHeader(ip6Header, meshsrc, meshdst, frame);

    error = mMle->CheckReachability(meshsrc.mAddress16, meshdst.mAddress16, ip6Header);

exit:
    return error;
}

void MeshForwarder::HandleFragment(uint8_t *frame, uint8_t frameLength,
                                   const Mac::Address &macsrc, const Mac::Address &macdst,
                                   const ThreadMessageInfo &messageInfo)
{
    FragmentHeader *fragmentHeader = reinterpret_cast<FragmentHeader *>(frame);
    uint16_t datagramLength = fragmentHeader->GetSize();
    uint16_t datagramTag = fragmentHeader->GetTag();
    Message *message;
    int headerLength;

    if (fragmentHeader->GetOffset() == 0)
    {
        frame += 4;
        frameLength -= 4;

        VerifyOrExit((message = Message::New(Message::kTypeIp6, 0)) != NULL, ;);
        headerLength = mLowpan->Decompress(*message, macsrc, macdst, frame, frameLength, datagramLength);
        VerifyOrExit(headerLength > 0, Message::Free(*message));
        frame += headerLength;
        frameLength -= headerLength;

        VerifyOrExit(message->SetLength(datagramLength) == kThreadError_None, Message::Free(*message));
        datagramLength = HostSwap16(datagramLength - sizeof(Ip6Header));
        message->Write(Ip6Header::GetPayloadLengthOffset(), sizeof(datagramLength), &datagramLength);
        message->SetDatagramTag(datagramTag);
        message->SetTimeout(kReassemblyTimeout);

        mReassemblyList.Enqueue(*message);

        if (!mReassemblyTimer.IsRunning())
        {
            mReassemblyTimer.Start(1000);
        }
    }
    else
    {
        frame += 5;
        frameLength -= 5;

        for (message = mReassemblyList.GetHead(); message; message = message->GetNext())
        {
            if (message->GetLength() == datagramLength &&
                message->GetDatagramTag() == datagramTag &&
                message->GetOffset() == fragmentHeader->GetOffset())
            {
                break;
            }
        }

        VerifyOrExit(message != NULL, ;);
    }

    assert(message != NULL);

    // copy Fragment
    message->Write(message->GetOffset(), frameLength, frame);
    message->MoveOffset(frameLength);
    VerifyOrExit(message->GetOffset() >= message->GetLength(), ;);

    mReassemblyList.Dequeue(*message);
    Ip6::HandleDatagram(*message, mNetif, mNetif->GetInterfaceId(), &messageInfo, false);

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

    for (Message *message = mReassemblyList.GetHead(); message; message = next)
    {
        next = message->GetNext();
        timeout = message->GetTimeout();

        if (timeout > 0)
        {
            message->SetTimeout(timeout - 1);
        }
        else
        {
            mReassemblyList.Dequeue(*message);
            Message::Free(*message);
        }
    }

    if (mReassemblyList.GetHead() != NULL)
    {
        mReassemblyTimer.Start(1000);
    }
}

void MeshForwarder::HandleLowpanHC(uint8_t *frame, uint8_t frameLength,
                                   const Mac::Address &macsrc, const Mac::Address &macdst,
                                   const ThreadMessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Message *message;
    int headerLength;
    uint16_t ip6PayloadLength;

    VerifyOrExit((message = Message::New(Message::kTypeIp6, 0)) != NULL, ;);

    headerLength = mLowpan->Decompress(*message, macsrc, macdst, frame, frameLength, 0);
    VerifyOrExit(headerLength > 0, ;);
    frame += headerLength;
    frameLength -= headerLength;

    SuccessOrExit(error = message->SetLength(message->GetLength() + frameLength));

    ip6PayloadLength = HostSwap16(message->GetLength() - sizeof(Ip6Header));
    message->Write(Ip6Header::GetPayloadLengthOffset(), sizeof(ip6PayloadLength), &ip6PayloadLength);

    message->Write(message->GetOffset(), frameLength, frame);
    Ip6::HandleDatagram(*message, mNetif, mNetif->GetInterfaceId(), &messageInfo, false);

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
    int childIndex;

    assert(mMle->GetDeviceState() != Mle::kDeviceStateDetached);

    VerifyOrExit((neighbor = mMle->GetNeighbor(macsrc)) != NULL, ;);
    neighbor->mLastHeard = Timer::GetNow();

    mMle->HandleMacDataRequest(*reinterpret_cast<Child *>(neighbor));
    childIndex = mMle->GetChildIndex(*reinterpret_cast<Child *>(neighbor));

    for (Message *message = mSendQueue.GetHead(); message; message = message->GetNext())
    {
        if (message->GetDirectTransmission() == false && message->GetChildMask(childIndex))
        {
            neighbor->mDataRequest = true;
            break;
        }
    }

    mScheduleTransmissionTask.Post();

exit:
    {}
}

}  // namespace Thread
