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
#include <common/random.h>
#include <common/thread_error.h>
#include <crypto/aes_ccm.h>
#include <mac/mac.h>
#include <mac/mac_frame.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>

namespace Thread {
namespace Mac {

static const uint8_t extended_panid_init_[] = {0xde, 0xad, 0x00, 0xbe, 0xef, 0x00, 0xca, 0xfe};
static const char network_name_init_[] = "JonathanHui";
static Mac *mac_;

Mac::Mac(ThreadNetif *netif):
    m_ack_timer(&HandleAckTimer, this),
    m_backoff_timer(&HandleBackoffTimer, this),
    m_receive_timer(&HandleReceiveTimer, this)
{
    m_key_manager = netif->GetKeyManager();
    m_mle = netif->GetMle();

    memcpy(m_extended_panid, extended_panid_init_, sizeof(m_extended_panid));
    memcpy(m_network_name, network_name_init_, sizeof(m_network_name));

    mac_ = this;
}

ThreadError Mac::Init()
{
    for (size_t i = 0; i < sizeof(m_address64); i++)
    {
        m_address64.bytes[i] = Random::Get();
    }

    m_beacon_sequence = Random::Get();
    m_data_sequence = Random::Get();

    phy_init();

    return kThreadError_None;
}

ThreadError Mac::Start()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_state == kStateDisabled, ;);

    SuccessOrExit(error = phy_start());

    SetExtendedPanId(m_extended_panid);
    phy_set_pan_id(m_panid);
    phy_set_short_address(m_address16);
    {
        uint8_t buf[8];

        for (size_t i = 0; i < sizeof(buf); i++)
        {
            buf[i] = m_address64.bytes[7 - i];
        }

        phy_set_extended_address(buf);
    }
    m_state = kStateIdle;
    NextOperation();

exit:
    return error;
}

ThreadError Mac::Stop()
{
    ThreadError error = kThreadError_None;

    SuccessOrExit(error = phy_stop());
    m_ack_timer.Stop();
    m_backoff_timer.Stop();
    m_state = kStateDisabled;

    while (m_send_head != NULL)
    {
        Sender *cur;
        cur = m_send_head;
        m_send_head = m_send_head->m_next;
        cur->m_next = NULL;
    }

    m_send_tail = NULL;

    while (m_receive_head != NULL)
    {
        Receiver *cur;
        cur = m_receive_head;
        m_receive_head = m_receive_head->m_next;
        cur->m_next = NULL;
    }

    m_receive_tail = NULL;

exit:
    return error;
}

ThreadError Mac::ActiveScan(uint16_t interval_per_channel, uint16_t channel_mask,
                            ActiveScanHandler handler, void *context)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(m_state != kStateDisabled && m_state != kStateActiveScan && m_active_scan_request == false,
                 error = kThreadError_Busy);

    m_active_scan_handler = handler;
    m_active_scan_context = context;
    m_scan_channel_mask = (channel_mask == 0) ? kMacScanChannelMaskAllChannels : channel_mask;
    m_scan_interval_per_channel = (interval_per_channel == 0) ? kMacScanDefaultInterval : interval_per_channel;

    m_scan_channel = 11;

    while ((m_scan_channel_mask & 1) == 0)
    {
        m_scan_channel_mask >>= 1;
        m_scan_channel++;
    }

    if (m_state == kStateIdle)
    {
        m_state = kStateActiveScan;
        m_backoff_timer.Start(16);
    }
    else
    {
        m_active_scan_request = true;
    }

exit:
    return error;
}

ThreadError Mac::RegisterReceiver(Receiver &receiver)
{
    assert(m_receive_tail != &receiver && receiver.m_next == NULL);

    if (m_receive_tail == NULL)
    {
        m_receive_head = &receiver;
        m_receive_tail = &receiver;
    }
    else
    {
        m_receive_tail->m_next = &receiver;
        m_receive_tail = &receiver;
    }

    return kThreadError_None;
}

bool Mac::GetRxOnWhenIdle() const
{
    return m_rx_on_when_idle;
}

ThreadError Mac::SetRxOnWhenIdle(bool rx_on_when_idle)
{
    m_rx_on_when_idle = rx_on_when_idle;
    return kThreadError_None;
}

const Address64 *Mac::GetAddress64() const
{
    return &m_address64;
}

Address16 Mac::GetAddress16() const
{
    return m_address16;
}

ThreadError Mac::SetAddress16(Address16 address16)
{
    m_address16 = address16;
    return phy_set_short_address(address16);
}

uint8_t Mac::GetChannel() const
{
    return m_channel;
}

ThreadError Mac::SetChannel(uint8_t channel)
{
    m_channel = channel;
    return kThreadError_None;
}

const char *Mac::GetNetworkName() const
{
    return m_network_name;
}

ThreadError Mac::SetNetworkName(const char *name)
{
    strncpy(m_network_name, name, sizeof(m_network_name));
    return kThreadError_None;
}

uint16_t Mac::GetPanId() const
{
    return m_panid;
}

ThreadError Mac::SetPanId(uint16_t panid)
{
    m_panid = panid;
    return phy_set_pan_id(m_panid);
}

const uint8_t *Mac::GetExtendedPanId() const
{
    return m_extended_panid;
}

ThreadError Mac::SetExtendedPanId(const uint8_t *xpanid)
{
    memcpy(m_extended_panid, xpanid, sizeof(m_extended_panid));
    m_mle->SetMeshLocalPrefix(m_extended_panid);
    return kThreadError_None;
}

ThreadError Mac::SendFrameRequest(Sender &sender)
{
    ThreadError error = kThreadError_None;
    uint32_t backoff;

    VerifyOrExit(m_state != kStateDisabled &&
                 m_send_tail != &sender && sender.m_next == NULL,
                 error = kThreadError_Busy);

    if (m_send_head == NULL)
    {
        m_send_head = &sender;
        m_send_tail = &sender;
    }
    else
    {
        m_send_tail->m_next = &sender;
        m_send_tail = &sender;
    }

    if (m_state == kStateIdle)
    {
        m_state = kStateTransmitData;
        backoff = (Random::Get() % 32) + 1;
        m_backoff_timer.Start(backoff);
    }

exit:
    return error;
}

void Mac::NextOperation()
{
    switch (m_state)
    {
    case kStateActiveScan:
        m_receive_frame.SetChannel(m_scan_channel);
        phy_receive(&m_receive_frame);
        break;

    default:
        if (m_rx_on_when_idle || m_receive_timer.IsRunning())
        {
            m_receive_frame.SetChannel(m_channel);
            phy_receive(&m_receive_frame);
        }
        else
        {
            phy_sleep();
        }

        break;
    }
}

void Mac::ScheduleNextTransmission()
{
    if (m_active_scan_request)
    {
        m_active_scan_request = false;
        m_state = kStateActiveScan;
        m_backoff_timer.Start(16);
    }
    else if (m_transmit_beacon)
    {
        m_transmit_beacon = false;
        m_state = kStateTransmitBeacon;
        m_backoff_timer.Start(16);
    }
    else if (m_send_head != NULL)
    {
        m_state = kStateTransmitData;
        m_backoff_timer.Start(16);
    }
    else
    {
        m_state = kStateIdle;
    }
}

void Mac::GenerateNonce(const Address64 &address, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce)
{
    // source address
    for (int i = 0; i < 8; i++)
    {
        nonce[i] = address.bytes[i];
    }

    nonce += 8;

    // frame counter
    nonce[0] = frame_counter >> 24;
    nonce[1] = frame_counter >> 16;
    nonce[2] = frame_counter >> 8;
    nonce[3] = frame_counter >> 0;
    nonce += 4;

    // security level
    nonce[0] = security_level;
}

void Mac::SendBeaconRequest(Frame *frame)
{
    // initialize MAC header
    uint16_t fcf = Frame::kFcfFrameMacCmd | Frame::kFcfDstAddrShort | Frame::kFcfSrcAddrNone;
    frame->InitMacHeader(fcf, Frame::kSecNone);
    frame->SetDstPanId(kShortAddrBroadcast);
    frame->SetDstAddr(kShortAddrBroadcast);
    frame->SetCommandId(Frame::kMacCmdBeaconRequest);

    dprintf("Sent Beacon Request\n");
}

void Mac::SendBeacon(Frame *frame)
{
    uint8_t *payload;
    uint16_t fcf;

    // initialize MAC header
    fcf = Frame::kFcfFrameBeacon | Frame::kFcfDstAddrNone | Frame::kFcfSrcAddrExt;
    frame->InitMacHeader(fcf, Frame::kSecNone);
    frame->SetSrcPanId(m_panid);
    frame->SetSrcAddr(m_address64);

    // write payload
    payload = frame->GetPayload();

    // Superframe Specification
    payload[0] = 0xff;
    payload[1] = 0x0f;
    payload += 2;

    // GTS Fields
    payload[0] = 0x00;
    payload++;

    // Pending Address Fields
    payload[0] = 0x00;
    payload++;

    // Protocol ID
    payload[0] = 0x03;
    payload++;

    // Version and Flags
    payload[0] = 0x1 << 4 | 0x1;
    payload++;

    // Network Name
    memcpy(payload, m_network_name, sizeof(m_network_name));
    payload += sizeof(m_network_name);

    // Extended PAN
    memcpy(payload, m_extended_panid, sizeof(m_extended_panid));
    payload += sizeof(m_extended_panid);

    frame->SetPayloadLength(payload - frame->GetPayload());

    dprintf("Sent Beacon\n");
}

void Mac::HandleBackoffTimer(void *context)
{
    Mac *obj = reinterpret_cast<Mac *>(context);
    obj->HandleBackoffTimer();
}

void Mac::ProcessTransmitSecurity()
{
    uint8_t security_level;
    uint8_t nonce[13];
    uint8_t tag_length;
    Crypto::AesEcb aes_ecb;
    Crypto::AesCcm aes_ccm;

    if (m_send_frame.GetSecurityEnabled() == false)
    {
        ExitNow();
    }

    m_send_frame.GetSecurityLevel(security_level);
    m_send_frame.SetFrameCounter(m_key_manager->GetMacFrameCounter());

    m_send_frame.SetKeyId((m_key_manager->GetCurrentKeySequence() & 0x7f) + 1);

    GenerateNonce(m_address64, m_key_manager->GetMacFrameCounter(), security_level, nonce);

    aes_ecb.SetKey(m_key_manager->GetCurrentMacKey(), 16);
    tag_length = m_send_frame.GetFooterLength() - 2;

    aes_ccm.Init(aes_ecb, m_send_frame.GetHeaderLength(), m_send_frame.GetPayloadLength(), tag_length,
                 nonce, sizeof(nonce));
    aes_ccm.Header(m_send_frame.GetHeader(), m_send_frame.GetHeaderLength());
    aes_ccm.Payload(m_send_frame.GetPayload(), m_send_frame.GetPayload(), m_send_frame.GetPayloadLength(), true);
    aes_ccm.Finalize(m_send_frame.GetFooter(), &tag_length);

    m_key_manager->IncrementMacFrameCounter();

exit:
    {}
}

void Mac::HandleBackoffTimer()
{
    ThreadError error = kThreadError_None;

    if (phy_idle() != kThreadError_None)
    {
        m_backoff_timer.Start(16);
        ExitNow();
    }

    switch (m_state)
    {
    case kStateActiveScan:
        m_send_frame.SetChannel(m_scan_channel);
        SendBeaconRequest(&m_send_frame);
        m_send_frame.SetSequence(0);
        break;

    case kStateTransmitBeacon:
        m_send_frame.SetChannel(m_channel);
        SendBeacon(&m_send_frame);
        m_send_frame.SetSequence(m_beacon_sequence++);
        break;

    case kStateTransmitData:
        m_send_frame.SetChannel(m_channel);
        SuccessOrExit(error = m_send_head->HandleFrameRequest(m_send_frame));
        m_send_frame.SetSequence(m_data_sequence);
        break;

    default:
        assert(false);
        break;
    }

    // Security Processing
    ProcessTransmitSecurity();

    SuccessOrExit(error = phy_transmit(&m_send_frame));

    if (m_send_frame.GetAckRequest())
    {
#ifdef CPU_KW2X
        m_ack_timer.Start(kMacAckTimeout);
#endif
        dprintf("ack timer start\n");
    }

exit:

    if (error != kThreadError_None)
    {
        assert(false);
    }
}

extern "C" void phy_handle_transmit_done(PhyPacket *packet, bool rx_pending, ThreadError error)
{
    mac_->HandleTransmitDone(packet, rx_pending, error);
}

void Mac::HandleTransmitDone(PhyPacket *packet, bool rx_pending, ThreadError error)
{
    m_ack_timer.Stop();

    if (error != kThreadError_None)
    {
        m_backoff_timer.Start(16);
        ExitNow();
    }

    switch (m_state)
    {
    case kStateActiveScan:
        m_ack_timer.Start(m_scan_interval_per_channel);
        break;

    case kStateTransmitBeacon:
        SentFrame(true);
        break;

    case kStateTransmitData:
        if (rx_pending)
        {
            m_receive_timer.Start(kDataTimeout);
        }
        else
        {
            m_receive_timer.Stop();
        }

        SentFrame(true);
        break;

    default:
        assert(false);
        break;
    }

exit:
    NextOperation();
}

void Mac::HandleAckTimer(void *context)
{
    Mac *obj = reinterpret_cast<Mac *>(context);
    obj->HandleAckTimer();
}

void Mac::HandleAckTimer()
{
    phy_idle();

    switch (m_state)
    {
    case kStateActiveScan:
        do
        {
            m_scan_channel_mask >>= 1;
            m_scan_channel++;

            if (m_scan_channel_mask == 0 || m_scan_channel > 26)
            {
                m_active_scan_handler(m_active_scan_context, NULL);
                ScheduleNextTransmission();
                ExitNow();
            }
        }
        while ((m_scan_channel_mask & 1) == 0);

        m_backoff_timer.Start(16);
        break;

    case kStateTransmitData:
        dprintf("ack timer fired\n");
        SentFrame(false);
        break;

    default:
        assert(false);
        break;
    }

exit:
    NextOperation();
}

void Mac::HandleReceiveTimer(void *context)
{
    Mac *obj = reinterpret_cast<Mac *>(context);
    obj->HandleReceiveTimer();
}

void Mac::HandleReceiveTimer()
{
    dprintf("data poll timeout!\n");
    NextOperation();
}

void Mac::SentFrame(bool acked)
{
    Address destination;
    Neighbor *neighbor;
    uint32_t backoff;
    Sender *sender;

    switch (m_state)
    {
    case kStateActiveScan:
        m_ack_timer.Start(m_scan_interval_per_channel);
        break;

    case kStateTransmitBeacon:
        ScheduleNextTransmission();
        break;

    case kStateTransmitData:
        if (m_send_frame.GetAckRequest() && !acked)
        {
            dump("NO ACK", m_send_frame.GetHeader(), 16);

            if (m_attempts < 12)
            {
                m_attempts++;
                backoff = (Random::Get() % 32) + 1;
                m_backoff_timer.Start(backoff);
                ExitNow();
            }

            m_send_frame.GetDstAddr(destination);

            if ((neighbor = m_mle->GetNeighbor(destination)) != NULL)
            {
                neighbor->state = Neighbor::kStateInvalid;
            }
        }

        m_attempts = 0;

        sender = m_send_head;
        m_send_head = m_send_head->m_next;

        if (m_send_head == NULL)
        {
            m_send_tail = NULL;
        }

        m_data_sequence++;
        sender->HandleSentFrame(m_send_frame);

        ScheduleNextTransmission();
        break;

    default:
        assert(false);
        break;
    }

exit:
    {}
}

ThreadError Mac::ProcessReceiveSecurity(const Address &srcaddr, Neighbor &neighbor)
{
    ThreadError error = kThreadError_None;
    uint8_t security_level;
    uint32_t frame_counter;
    uint8_t nonce[13];
    uint8_t tag[16];
    uint8_t tag_length;
    uint8_t keyid;
    uint32_t key_sequence;
    const uint8_t *mac_key;
    Crypto::AesEcb aes_ecb;
    Crypto::AesCcm aes_ccm;

    if (m_receive_frame.GetSecurityEnabled() == false)
    {
        ExitNow();
    }

    VerifyOrExit(m_key_manager != NULL, error = kThreadError_Security);

    m_receive_frame.GetSecurityLevel(security_level);
    m_receive_frame.GetFrameCounter(frame_counter);

    GenerateNonce(srcaddr.address64, frame_counter, security_level, nonce);

    tag_length = m_receive_frame.GetFooterLength() - 2;

    m_receive_frame.GetKeyId(keyid);
    keyid--;

    if (keyid == (m_key_manager->GetCurrentKeySequence() & 0x7f))
    {
        // same key index
        key_sequence = m_key_manager->GetCurrentKeySequence();
        mac_key = m_key_manager->GetCurrentMacKey();
        VerifyOrExit(neighbor.previous_key == true || frame_counter >= neighbor.valid.link_frame_counter,
                     error = kThreadError_Security);
    }
    else if (neighbor.previous_key &&
             m_key_manager->IsPreviousKeyValid() &&
             keyid == (m_key_manager->GetPreviousKeySequence() & 0x7f))
    {
        // previous key index
        key_sequence = m_key_manager->GetPreviousKeySequence();
        mac_key = m_key_manager->GetPreviousMacKey();
        VerifyOrExit(frame_counter >= neighbor.valid.link_frame_counter, error = kThreadError_Security);
    }
    else if (keyid == ((m_key_manager->GetCurrentKeySequence() + 1) & 0x7f))
    {
        // next key index
        key_sequence = m_key_manager->GetCurrentKeySequence() + 1;
        mac_key = m_key_manager->GetTemporaryMacKey(key_sequence);
    }
    else
    {
        for (Receiver *receiver = m_receive_head; receiver; receiver = receiver->m_next)
        {
            receiver->HandleReceivedFrame(m_receive_frame, kThreadError_Security);
        }

        ExitNow(error = kThreadError_Security);
    }

    aes_ecb.SetKey(mac_key, 16);
    aes_ccm.Init(aes_ecb, m_receive_frame.GetHeaderLength(), m_receive_frame.GetPayloadLength(),
                 tag_length, nonce, sizeof(nonce));
    aes_ccm.Header(m_receive_frame.GetHeader(), m_receive_frame.GetHeaderLength());
    aes_ccm.Payload(m_receive_frame.GetPayload(), m_receive_frame.GetPayload(), m_receive_frame.GetPayloadLength(),
                    false);
    aes_ccm.Finalize(tag, &tag_length);

    VerifyOrExit(memcmp(tag, m_receive_frame.GetFooter(), tag_length) == 0, error = kThreadError_Security);

    if (key_sequence > m_key_manager->GetCurrentKeySequence())
    {
        m_key_manager->SetCurrentKeySequence(key_sequence);
    }

    if (key_sequence == m_key_manager->GetCurrentKeySequence())
    {
        neighbor.previous_key = false;
    }

    neighbor.valid.link_frame_counter = frame_counter + 1;

exit:
    return error;
}

extern "C" void phy_handle_receive_done(PhyPacket *packet, ThreadError error)
{
    mac_->HandleReceiveDone(packet, error);
}

void Mac::HandleReceiveDone(PhyPacket *packet, ThreadError error)
{
    Address srcaddr;
    Address dstaddr;
    PanId panid;
    Neighbor *neighbor;
    int entry;
    int8_t rssi;

    assert(packet == &m_receive_frame);

    VerifyOrExit(error == kThreadError_None, ;);

    m_receive_frame.GetSrcAddr(srcaddr);
    neighbor = m_mle->GetNeighbor(srcaddr);

    switch (srcaddr.length)
    {
    case 0:
        break;

    case 2:
        VerifyOrExit(neighbor != NULL, dprintf("drop not neighbor\n"));
        srcaddr.length = 8;
        memcpy(&srcaddr.address64, &neighbor->mac_addr, sizeof(srcaddr.address64));
        break;

    case 8:
        break;

    default:
        ExitNow();
    }

    // Source Whitelist Processing
    if (srcaddr.length != 0 && m_whitelist.IsEnabled())
    {
        VerifyOrExit((entry = m_whitelist.Find(srcaddr.address64)) >= 0, ;);

        if (m_whitelist.GetRssi(entry, rssi) == kThreadError_None)
        {
            packet->m_power = rssi;
        }
    }

    // Destination Address Filtering
    m_receive_frame.GetDstAddr(dstaddr);

    switch (dstaddr.length)
    {
    case 0:
        break;

    case 2:
        m_receive_frame.GetDstPanId(panid);
        VerifyOrExit((panid == kShortAddrBroadcast || panid == m_panid) &&
                     ((m_rx_on_when_idle && dstaddr.address16 == kShortAddrBroadcast) ||
                      dstaddr.address16 == m_address16), ;);
        break;

    case 8:
        m_receive_frame.GetDstPanId(panid);
        VerifyOrExit(panid == m_panid &&
                     memcmp(&dstaddr.address64, &m_address64, sizeof(dstaddr.address64)) == 0, ;);
        break;
    }

    // Security Processing
    SuccessOrExit(ProcessReceiveSecurity(srcaddr, *neighbor));

    switch (m_state)
    {
    case kStateActiveScan:
        HandleBeaconFrame();
        break;

    default:
        if (dstaddr.length != 0)
        {
            m_receive_timer.Stop();
        }

        if (m_receive_frame.GetType() == Frame::kFcfFrameMacCmd)
        {
            SuccessOrExit(HandleMacCommand());
        }

        for (Receiver *receiver = m_receive_head; receiver; receiver = receiver->m_next)
        {
            receiver->HandleReceivedFrame(m_receive_frame, kThreadError_None);
        }

        break;
    }

exit:
    NextOperation();
}

void Mac::HandleBeaconFrame()
{
    uint8_t *payload = m_receive_frame.GetPayload();
    uint8_t payload_length = m_receive_frame.GetPayloadLength();
    ActiveScanResult result;
    Address address;

    if (m_receive_frame.GetType() != Frame::kFcfFrameBeacon)
    {
        ExitNow();
    }

#if 0

    // Superframe Specification, GTS fields, and Pending Address fields
    if (payload_length < 4 || payload[0] != 0xff || payload[1] != 0x0f || payload[2] != 0x00 || payload[3] != 0x00)
    {
        ExitNow();
    }

#endif
    payload += 4;
    payload_length -= 4;

#if 0

    // Protocol ID
    if (payload[0] != 3)
    {
        ExitNow();
    }

#endif
    payload++;

    // skip Version and Flags
    payload++;

    // network name
    memcpy(result.network_name, payload, sizeof(result.network_name));
    payload += 16;

    // extended panid
    memcpy(result.ext_panid, payload, sizeof(result.ext_panid));
    payload += 8;

    // extended address
    m_receive_frame.GetSrcAddr(address);
    memcpy(result.ext_addr, &address.address64, sizeof(result.ext_addr));

    // panid
    m_receive_frame.GetSrcPanId(result.panid);

    // channel
    result.channel = m_receive_frame.GetChannel();

    // rssi
    result.rssi = m_receive_frame.GetPower();

    m_active_scan_handler(m_active_scan_context, &result);

exit:
    {}
}

ThreadError Mac::HandleMacCommand()
{
    ThreadError error = kThreadError_None;
    uint8_t command_id;
    m_receive_frame.GetCommandId(command_id);

    if (command_id == Frame::kMacCmdBeaconRequest)
    {
        dprintf("Received Beacon Request\n");
        m_transmit_beacon = true;

        if (m_state == kStateIdle)
        {
            m_state = kStateTransmitBeacon;
            m_transmit_beacon = false;
            m_backoff_timer.Start(16);
        }

        ExitNow(error = kThreadError_Drop);
    }

exit:
    return error;
}

Whitelist *Mac::GetWhitelist()
{
    return &m_whitelist;
}

}  // namespace Mac
}  // namespace Thread
