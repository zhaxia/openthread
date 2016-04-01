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

#ifndef NCP_NCP_H_
#define NCP_NCP_H_

#include <common/message.h>
#include <common/thread_error.h>
#include <ncp/hdlc.h>
#include <ncp/ncp.pb-c.h>
#include <platform/common/uart.h>
#include <thread/thread_netif.h>

namespace Thread {

class Ncp
{
public:
    Ncp();
    ThreadError Init();
    ThreadError Start();
    ThreadError Stop();

    static void HandleReceivedDatagram(void *context, Message &message);
    void HandleReceivedDatagram(Message &message);

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
    static void HandleReceive(void *context, uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    void HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    static void HandleSendDone(void *context);
    void HandleSendDone();
    static void HandleSendMessageDone(void *context);
    void HandleSendMessageDone();
    static void HandleUnicastAddressesChanged(void *context);

    static void RunUpdateAddressesTask(void *context);

    ThreadNetif m_netif;
    NetifHandler m_netif_handler;
    Hdlc m_hdlc;
    bool m_hdlc_sending;
    uint16_t m_scan_result_index;

    void RunUpdateAddressesTask();
    Tasklet m_update_addresses_task;

    void RunSendScanResultsTask();

    MessageQueue m_send_queue;
};

}  // namespace Thread

#endif  // NCP_NCP_H_
