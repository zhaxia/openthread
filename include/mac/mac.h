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

#ifndef MAC_H_
#define MAC_H_

#include <common/tasklet.h>
#include <common/timer.h>
#include <crypto/aes_ecb.h>
#include <mac/mac_frame.h>
#include <mac/mac_whitelist.h>
#include <platform/common/phy.h>
#include <thread/key_manager.h>
#include <thread/topology.h>

namespace Thread {

namespace Mle { class MleRouter; }

namespace Mac {

enum
{
    kMacAckTimeout = 16,  // milliseconds
    kDataTimeout = 100,  // milliseconds

    kMacScanChannelMaskAllChannels = 0xffff,
    kMacScanDefaultInterval = 128  // milliseconds
};

struct ActiveScanResult
{
    uint8_t network_name[16];
    uint8_t ext_panid[8];
    uint8_t ext_addr[8];
    uint16_t panid;
    uint8_t channel;
    int8_t rssi;
};

class Receiver
{
    friend class Mac;

public:
    typedef void (*ReceiveFrameHandler)(void *context, Frame &frame, ThreadError error);
    Receiver(ReceiveFrameHandler receive_frame_handler, void *context) {
        m_receive_frame_handler = receive_frame_handler;
        m_context = context;
        m_next = NULL;
    }

private:
    void HandleReceivedFrame(Frame &frame, ThreadError error) { m_receive_frame_handler(m_context, frame, error); }

    ReceiveFrameHandler m_receive_frame_handler;
    void *m_context;
    Receiver *m_next;
};

class Sender
{
    friend class Mac;

public:
    typedef ThreadError(*FrameRequestHandler)(void *context, Frame &frame);
    typedef void (*SentFrameHandler)(void *context, Frame &frame);
    Sender(FrameRequestHandler frame_request_handler, SentFrameHandler sent_frame_handler, void *context) {
        m_frame_request_handler = frame_request_handler;
        m_sent_frame_handler = sent_frame_handler;
        m_context = context;
        m_next = NULL;
    }

private:
    ThreadError HandleFrameRequest(Frame &frame) { return m_frame_request_handler(m_context, frame); }
    void HandleSentFrame(Frame &frame) { m_sent_frame_handler(m_context, frame); }

    FrameRequestHandler m_frame_request_handler;
    SentFrameHandler m_sent_frame_handler;
    void *m_context;
    Sender *m_next;
};

class Mac
{
public:
    explicit Mac(ThreadNetif *netif);
    ThreadError Init();
    ThreadError Start();
    ThreadError Stop();

    typedef void (*ActiveScanHandler)(void *context, ActiveScanResult *result);
    ThreadError ActiveScan(uint16_t interval_in_ms_per_channel, uint16_t channel_mask,
                           ActiveScanHandler handler, void *context);

    bool GetRxOnWhenIdle() const;
    ThreadError SetRxOnWhenIdle(bool rx_on_when_idle);

    ThreadError RegisterReceiver(Receiver &receiver);
    ThreadError SendFrameRequest(Sender &sender);

    const Address64 *GetAddress64() const;
    Address16 GetAddress16() const;
    ThreadError SetAddress16(Address16 address16);

    uint8_t GetChannel() const;
    ThreadError SetChannel(uint8_t channel);

    const char *GetNetworkName() const;
    ThreadError SetNetworkName(const char *name);

    uint16_t GetPanId() const;
    ThreadError SetPanId(uint16_t panid);

    const uint8_t *GetExtendedPanId() const;
    ThreadError SetExtendedPanId(const uint8_t *xpanid);

    void HandleReceiveDone(PhyPacket *packet, ThreadError error);
    void HandleTransmitDone(PhyPacket *packet, bool rx_pending, ThreadError error);

    Whitelist *GetWhitelist();

private:
    void GenerateNonce(const Address64 &address, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce);
    void NextOperation();
    void ProcessTransmitSecurity();
    ThreadError ProcessReceiveSecurity(const Address &srcaddr, Neighbor &neighbor);
    void ScheduleNextTransmission();
    void SentFrame(bool acked);
    void SendBeaconRequest(Frame *frame);
    void SendBeacon(Frame *frame);
    void StartBackoff();
    void HandleBeaconFrame();
    ThreadError HandleMacCommand();

    static void HandleAckTimer(void *context);
    void HandleAckTimer();
    static void HandleBackoffTimer(void *context);
    void HandleBackoffTimer();
    static void HandleReceiveTimer(void *context);
    void HandleReceiveTimer();

    Timer m_ack_timer;
    Timer m_backoff_timer;
    Timer m_receive_timer;

    KeyManager *m_key_manager = NULL;

    Address64 m_address64;
    Address16 m_address16 = kShortAddrInvalid;
    uint16_t m_panid = kShortAddrInvalid;
    uint8_t m_extended_panid[8];
    char m_network_name[16];
    uint8_t m_channel = 12;

    Frame m_send_frame;
    Frame m_receive_frame;
    Sender *m_send_head = NULL, *m_send_tail = NULL;
    Receiver *m_receive_head = NULL, *m_receive_tail = NULL;
    Mle::MleRouter *m_mle;

    enum
    {
        kStateDisabled = 0,
        kStateIdle,
        kStateActiveScan,
        kStateTransmitBeacon,
        kStateTransmitData,
    };
    uint8_t m_state = kStateDisabled;

    uint8_t m_beacon_sequence;
    uint8_t m_data_sequence;
    bool m_rx_on_when_idle = true;
    uint8_t m_attempts = 0;
    bool m_transmit_beacon = false;

    bool m_active_scan_request = false;
    uint8_t m_scan_channel = 11;
    uint16_t m_scan_channel_mask = 0xff;
    uint16_t m_scan_interval_per_channel = 0;
    ActiveScanHandler m_active_scan_handler = NULL;
    void *m_active_scan_context = NULL;

    Whitelist m_whitelist;
};

}  // namespace Mac
}  // namespace Thread

#endif  // MAC_MAC_H_
