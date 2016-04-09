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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#ifndef NCP_NCP_BASE_H_
#define NCP_NCP_BASE_H_

#include <common/message.h>
#include <common/thread_error.h>
#include <ncp/ncp.pb-c.h>
#include <platform/common/uart.h>
#include <thread/thread_netif.h>

enum {
  kNcpChannel_ThreadControl = 0,    ///< Host initiated Thread control
  kNcpChannel_ThreadInterface = 1,  ///< Unsolicited Thread interface events
  kNcpChannel_ThreadData = 2,       ///< IPv6 data 
  kNcpChannel_ThreadBle = 3         ///< BLE HCI 
};

namespace Thread {

class NcpBase
{
public:
    NcpBase();

    virtual ThreadError Init();
    virtual ThreadError Start();
    virtual ThreadError Stop();

    virtual ThreadError SendMessage(uint8_t protocol, Message &message) = 0;
    virtual ThreadError Send(uint8_t protocol, uint8_t *frame, 
			     uint16_t frame_length) = 0;

    static void HandleReceivedDatagram(void *context, Message &message);
    void HandleReceivedDatagram(Message &message);

protected:
    static void HandleReceive(void *context, uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    void HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    static void HandleSendDone(void *context);
    void HandleSendDone();
    static void HandleSendMessageDone(void *context);
    void HandleSendMessageDone();

private:
    ThreadError ProcessThreadControl(uint8_t *buf, uint16_t buf_length);
    ThreadError ProcessPrimitive(ThreadControl &message);
    ThreadError ProcessPrimitiveKey(ThreadControl &message);
    ThreadError ProcessPrimitiveKeySequence(ThreadControl &message);
    ThreadError ProcessPrimitiveMeshLocalPrefix(ThreadControl &message);
    ThreadError ProcessPrimitiveMode(ThreadControl &message);
    ThreadError ProcessPrimitiveStatus(ThreadControl &message);
    ThreadError ProcessPrimitiveTimeout(ThreadControl &message);
    ThreadError ProcessPrimitiveChannel(ThreadControl &message);
    ThreadError ProcessPrimitivePanId(ThreadControl &message);
    ThreadError ProcessPrimitiveExtendedPanId(ThreadControl &message);
    ThreadError ProcessPrimitiveNetworkName(ThreadControl &message);
    ThreadError ProcessPrimitiveShortAddr(ThreadControl &message);
    ThreadError ProcessPrimitiveExtAddr(ThreadControl &message);
    ThreadError ProcessState(ThreadControl &message);
    ThreadError ProcessWhitelist(ThreadControl &message);
    ThreadError ProcessScanRequest(ThreadControl &message);

    static void HandleActiveScanResult(void *context, Mac::ActiveScanResult *result);
    void HandleActiveScanResult(Mac::ActiveScanResult *result);
    static void HandleUnicastAddressesChanged(void *context);

    static void RunUpdateAddressesTask(void *context);

    ThreadNetif m_netif;
    NetifHandler m_netif_handler;

    bool m_sending;

    uint16_t m_scan_result_index;

    void RunUpdateAddressesTask();
    Tasklet m_update_addresses_task;

    void RunSendScanResultsTask();

 protected:
    MessageQueue m_send_queue;
};

}  // namespace Thread

#endif  // NCP_NCP_BASE_H_
