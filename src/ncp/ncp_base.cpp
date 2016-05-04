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

/**
 * @file
 *   This file implements a protobuf interface to the OpenThread stack.
 */

#include <stdio.h>

#include <common/code_utils.hpp>
#include <ncp/ncp.hpp>

namespace Thread {

NcpBase::NcpBase():
    mNetifHandler(&HandleUnicastAddressesChanged, this),
    mUpdateAddressesTask(&RunUpdateAddressesTask, this)
{
}

ThreadError NcpBase::Start()
{
    mNetif.RegisterHandler(mNetifHandler);
    Ip6::Ip6::SetNcpReceivedHandler(&HandleReceivedDatagram, this);
    return kThreadError_None;
}

ThreadError NcpBase::Stop()
{
    return kThreadError_None;
}

void NcpBase::HandleReceivedDatagram(void *context, Message &message)
{
    NcpBase *obj = reinterpret_cast<NcpBase *>(context);
    obj->HandleReceivedDatagram(message);
}

void NcpBase::HandleReceivedDatagram(Message &message)
{
    SuccessOrExit(mSendQueue.Enqueue(message));

    if (mSending == false)
    {
        mSending = true;
        SendMessage(kNcpChannel_ThreadData, message);
    }

exit:
    {}
}

ThreadError NcpBase::ProcessThreadControl(uint8_t *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_None;
    ThreadControl thread_control;

    VerifyOrExit(thread_control__unpack(bufLength, buf, &thread_control) != NULL,
                 printf("protobuf unpack error\n"); error = kThreadError_Parse);

    switch (thread_control.message_case)
    {
    case THREAD_CONTROL__MESSAGE_PRIMITIVE:
        ProcessPrimitive(thread_control);
        bufLength = thread_control__pack(&thread_control, buf);
        Send(kNcpChannel_ThreadControl, buf, bufLength);
        mSending = true;
        break;

    case THREAD_CONTROL__MESSAGE_STATE:
        ProcessState(thread_control);
        bufLength = thread_control__pack(&thread_control, buf);
        Send(kNcpChannel_ThreadControl, buf, bufLength);
        mSending = true;
        break;

    case THREAD_CONTROL__MESSAGE_WHITELIST:
        ProcessWhitelist(thread_control);
        bufLength = thread_control__pack(&thread_control, buf);
        Send(kNcpChannel_ThreadControl, buf, bufLength);
        mSending = true;
        break;

    case THREAD_CONTROL__MESSAGE_SCAN_REQUEST:
        ProcessScanRequest(thread_control);
        bufLength = thread_control__pack(&thread_control, buf);
        Send(kNcpChannel_ThreadControl, buf, bufLength);
        mSending = true;
        break;

    default:
        break;
    }

exit:
    return error;
}

ThreadError NcpBase::ProcessPrimitive(ThreadControl &message)
{
    switch (message.primitive.type)
    {
    case THREAD_PRIMITIVE__TYPE__THREAD_KEY:
        ProcessPrimitiveKey(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_KEY_SEQUENCE:
        ProcessPrimitiveKeySequence(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_MESH_LOCAL_PREFIX:
        ProcessPrimitiveMeshLocalPrefix(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_MODE:
        ProcessPrimitiveMode(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_STATUS:
        ProcessPrimitiveStatus(message);
        break;

    case THREAD_PRIMITIVE__TYPE__THREAD_TIMEOUT:
        ProcessPrimitiveTimeout(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_CHANNEL:
        ProcessPrimitiveChannel(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_PANID:
        ProcessPrimitivePanId(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_EXTENDED_PANID:
        ProcessPrimitiveExtendedPanId(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_NETWORK_NAME:
        ProcessPrimitiveNetworkName(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_SHORT_ADDR:
        ProcessPrimitiveShortAddr(message);
        break;

    case THREAD_PRIMITIVE__TYPE__IEEE802154_EXT_ADDR:
        ProcessPrimitiveExtAddr(message);
        break;

    case _THREAD_PRIMITIVE__TYPE_IS_INT_SIZE:
        break;
    }

    return kThreadError_None;
}

ThreadError NcpBase::ProcessPrimitiveKey(ThreadControl &message)
{
    KeyManager &key_manager = mNetif.GetKeyManager();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
        key_manager.SetMasterKey(message.primitive.bytes.data, message.primitive.bytes.len);
        break;

    default:
        break;
    }

    message.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;

    uint8_t keyLength;
    const uint8_t *key = key_manager.GetMasterKey(&keyLength);
    memcpy(message.primitive.bytes.data, key, keyLength);
    message.primitive.bytes.len = keyLength;

    return kThreadError_None;
}

ThreadError NcpBase::ProcessPrimitiveKeySequence(ThreadControl &message)
{
    KeyManager &key_manager = mNetif.GetKeyManager();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        key_manager.SetCurrentKeySequence(message.primitive.uint32);
        break;

    default:
        break;
    }

    message.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
    message.primitive.uint32 = key_manager.GetCurrentKeySequence();

    return kThreadError_None;
}

ThreadError NcpBase::ProcessPrimitiveMeshLocalPrefix(ThreadControl &message)
{
    Mle::MleRouter &mle = mNetif.GetMle();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
        mle.SetMeshLocalPrefix(message.primitive.bytes.data);

    // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
        message.primitive.bytes.len = 8;
        memcpy(message.primitive.bytes.data, mle.GetMeshLocalPrefix(), 8);
        break;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessPrimitiveMode(ThreadControl &message)
{
    Mle::MleRouter &mle = mNetif.GetMle();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        mle.SetDeviceMode(message.primitive.uint32);

    // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
        message.primitive.uint32 = mle.GetDeviceMode();
        break;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessPrimitiveStatus(ThreadControl &message)
{
    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BOOL:
        if (message.primitive.bool_)
        {
            mNetif.Up();
        }
        else
        {
            mNetif.Down();
        }

    // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_BOOL;
        message.primitive.bool_ = mNetif.IsUp();
        break;

    default:
        break;
    }

    return kThreadError_None;
}

ThreadError NcpBase::ProcessPrimitiveTimeout(ThreadControl &message)
{
    Mle::MleRouter &mle = mNetif.GetMle();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        mle.SetTimeout(message.primitive.uint32);
        break;

    default:
        break;
    }

    message.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
    message.primitive.uint32 = mle.GetTimeout();

    return kThreadError_None;
}

ThreadError NcpBase::ProcessPrimitiveChannel(ThreadControl &message)
{
    Mac::Mac &mac = mNetif.GetMac();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        mac.SetChannel(message.primitive.uint32);

    // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
        message.primitive.uint32 = mac.GetChannel();
        return kThreadError_None;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessPrimitivePanId(ThreadControl &message)
{
    Mac::Mac &mac = mNetif.GetMac();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_UINT32:
        mac.SetPanId(message.primitive.uint32);

    // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
        message.primitive.uint32 = mac.GetPanId();
        return kThreadError_None;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessPrimitiveExtendedPanId(ThreadControl &message)
{
    Mac::Mac &mac = mNetif.GetMac();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
        mac.SetExtendedPanId(message.primitive.bytes.data);

    // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
        message.primitive.bytes.len = 8;
        memcpy(message.primitive.bytes.data, mac.GetExtendedPanId(), 8);
        return kThreadError_None;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessPrimitiveNetworkName(ThreadControl &message)
{
    Mac::Mac &mac = mNetif.GetMac();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE_BYTES:
        mac.SetNetworkName(reinterpret_cast<const char *>(message.primitive.bytes.data));

    // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
        message.primitive.bytes.len = 16;
        memcpy(message.primitive.bytes.data, mac.GetNetworkName(), 16);
        return kThreadError_None;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessPrimitiveShortAddr(ThreadControl &message)
{
    Mac::Mac &mac = mNetif.GetMac();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
        message.primitive.uint32 = mac.GetShortAddress();
        return kThreadError_None;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessPrimitiveExtAddr(ThreadControl &message)
{
    Mac::Mac &mac = mNetif.GetMac();

    switch (message.primitive.value_case)
    {
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
        message.primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
        message.primitive.bytes.len = 8;
        memcpy(message.primitive.bytes.data, mac.GetExtAddress(), 8);
        return kThreadError_None;

    default:
        break;
    }

    return kThreadError_Parse;
}

ThreadError NcpBase::ProcessState(ThreadControl &message)
{
    Mle::MleRouter &mle = mNetif.GetMle();

    if (message.state.has_state)
    {
        switch (message.state.state)
        {
        case THREAD_STATE__STATE__DETACHED:
            mle.BecomeDetached();
            break;

        case THREAD_STATE__STATE__CHILD:
            mle.BecomeChild(kMleAttachSamePartition);
            break;

        case THREAD_STATE__STATE__ROUTER:
            mle.BecomeRouter();
            break;

        case THREAD_STATE__STATE__LEADER:
            mle.BecomeLeader();
            break;

        case _THREAD_STATE__STATE_IS_INT_SIZE:
            break;
        }
    }

    message.state.has_state = true;

    switch (mle.GetDeviceState())
    {
    case Mle::kDeviceStateDisabled:
    case Mle::kDeviceStateDetached:
        message.state.state = THREAD_STATE__STATE__DETACHED;
        break;

    case Mle::kDeviceStateChild:
        message.state.state = THREAD_STATE__STATE__CHILD;
        break;

    case Mle::kDeviceStateRouter:
        message.state.state = THREAD_STATE__STATE__ROUTER;
        break;

    case Mle::kDeviceStateLeader:
        message.state.state = THREAD_STATE__STATE__LEADER;
        break;
    }

    return kThreadError_None;
}

ThreadError NcpBase::ProcessWhitelist(ThreadControl &message)
{
    Mac::Whitelist &whitelist = mNetif.GetMac().GetWhitelist();

    switch (message.whitelist.type)
    {
    case THREAD_WHITELIST__TYPE__STATUS:
    {
        if (message.whitelist.has_status)
        {
            switch (message.whitelist.status)
            {
            case THREAD_WHITELIST__STATUS__DISABLE:
                whitelist.Disable();
                break;

            case THREAD_WHITELIST__STATUS__ENABLE:
                whitelist.Enable();
                break;

            case _THREAD_WHITELIST__STATUS_IS_INT_SIZE:
                break;
            }
        }

        message.whitelist.has_status = true;
        message.whitelist.status = whitelist.IsEnabled() ?
                                   THREAD_WHITELIST__STATUS__ENABLE : THREAD_WHITELIST__STATUS__DISABLE;
        break;
    }

    case THREAD_WHITELIST__TYPE__LIST:
    {
        message.whitelist.n_address = whitelist.GetMaxEntries();

        for (unsigned i = 0; i < message.whitelist.n_address; i++)
        {
            message.whitelist.address[i].len = sizeof(message.whitelist.address[i].data);
            memcpy(message.whitelist.address[i].data, whitelist.GetEntries()[i].mExtAddress.mBytes,
                   sizeof(message.whitelist.address[i].data));
        }

        break;
    }

    case THREAD_WHITELIST__TYPE__ADD:
        whitelist.Add(*reinterpret_cast<Mac::ExtAddress *>(&message.whitelist.address[0]));
        break;

    case THREAD_WHITELIST__TYPE__CLEAR:
        whitelist.Clear();
        break;

    case THREAD_WHITELIST__TYPE__DELETE:
        whitelist.Remove(*reinterpret_cast<Mac::ExtAddress *>(&message.whitelist.address[0]));
        break;

    case _THREAD_WHITELIST__TYPE_IS_INT_SIZE:
        break;
    }

    return kThreadError_None;
}

ThreadError NcpBase::ProcessScanRequest(ThreadControl &message)
{
    uint16_t channel_mask = 0;
    uint16_t scan_interval = 0;

    if (message.scan_request.has_channel_mask)
    {
        channel_mask = static_cast<uint16_t>(message.scan_request.channel_mask);
    }

    if (message.scan_request.has_scan_interval_per_channel)
    {
        scan_interval = static_cast<uint16_t>(message.scan_request.scan_interval_per_channel);
    }

    return mNetif.GetMac().ActiveScan(scan_interval, channel_mask, &HandleActiveScanResult, this);
}

void NcpBase::HandleActiveScanResult(void *context, Mac::ActiveScanResult *result)
{
    NcpBase *obj = reinterpret_cast<NcpBase *>(context);
    obj->HandleActiveScanResult(result);
}

void NcpBase::HandleActiveScanResult(Mac::ActiveScanResult *result)
{
    ThreadControl message;
    size_t len;

    VerifyOrExit(mSending == false, ;);

    thread_control__init(&message);

    message.message_case = THREAD_CONTROL__MESSAGE_SCAN_RESULT;
    thread_scan_result__init(&message.scan_result);

    len = sizeof(result->mNetworkName);
    memcpy(message.scan_result.network_name.data,  result->mNetworkName, len);
    message.scan_result.network_name.len = len;

    len = sizeof(result->mExtPanid);
    memcpy(message.scan_result.ext_panid.data,  result->mExtPanid, len);
    message.scan_result.ext_panid.len = len;

    len = sizeof(result->mExtAddr);
    memcpy(message.scan_result.ext_addr.data,  result->mExtAddr, len);
    message.scan_result.ext_addr.len = len;

    message.scan_result.panid = static_cast<uint32_t>(result->mPanId);
    message.scan_result.channel = static_cast<uint32_t>(result->mChannel);
    message.scan_result.rssi = static_cast<int32_t>(result->mRssi);

    uint8_t buf[512];
    int bufLength;

    bufLength = thread_control__pack(&message, buf);
    Send(kNcpChannel_ThreadControl, buf, bufLength);
    mSending = true;

exit:
    return;
}

void NcpBase::HandleUnicastAddressesChanged(void *context)
{
    NcpBase *obj = reinterpret_cast<NcpBase *>(context);
    obj->mUpdateAddressesTask.Post();
}

void NcpBase::RunUpdateAddressesTask(void *context)
{
    NcpBase *obj = reinterpret_cast<NcpBase *>(context);
    obj->RunUpdateAddressesTask();
}

void NcpBase::RunUpdateAddressesTask()
{
    ThreadControl message;
    thread_control__init(&message);
    message.message_case = THREAD_CONTROL__MESSAGE_ADDRESSES;

    thread_ip6_addresses__init(&message.addresses);

    for (const Ip6::NetifUnicastAddress *address = mNetif.GetUnicastAddresses(); address; address = address->GetNext())
    {
        unsigned n = message.addresses.n_address;
        message.addresses.address[n].len = sizeof(message.addresses.address[n].data);
        memcpy(message.addresses.address[n].data, &address->mAddress, sizeof(message.addresses.address[n].data));
        n++;

        message.addresses.n_address = n;

        if (n >= sizeof(message.addresses.address) / sizeof(message.addresses.address[0]))
        {
            break;
        }
    }

    uint8_t buf[1024];
    int bufLength;

    bufLength = thread_control__pack(&message, buf);

    Send(kNcpChannel_ThreadInterface, buf, bufLength);
    mSending = true;
}

// ============================================================
//     Serial channel message callbacks
// ============================================================

void NcpBase::HandleReceive(void *context, uint8_t protocol,
                            uint8_t *buf, uint16_t bufLength)
{
    NcpBase *obj = reinterpret_cast<NcpBase *>(context);
    obj->HandleReceive(protocol, buf, bufLength);
}

void NcpBase::HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t bufLength)
{
    switch (protocol)
    {
    case kNcpChannel_ThreadControl:
        ProcessThreadControl(buf, bufLength);
        break;

    case kNcpChannel_ThreadData:
        Message *message;
        VerifyOrExit((message = Ip6::Ip6::NewMessage(0)) != NULL, ;);
        SuccessOrExit(message->Append(buf, bufLength));
        Ip6::Ip6::HandleDatagram(*message, NULL, mNetif.GetInterfaceId(), NULL, true);
        break;
    }

exit:
    {}
}

void NcpBase::HandleSendDone(void *context)
{
    NcpBase *obj = reinterpret_cast<Ncp *>(context);
    obj->HandleSendDone();
}

void NcpBase::HandleSendDone()
{
    mSending = false;

    if (mSendQueue.GetHead() != NULL)
    {
        SendMessage(kNcpChannel_ThreadData, *mSendQueue.GetHead());
        mSending = true;
    }
}

void NcpBase::HandleSendMessageDone(void *context)
{
    Ncp *obj = reinterpret_cast<Ncp *>(context);
    obj->HandleSendMessageDone();
}

void NcpBase::HandleSendMessageDone()
{
    Message *message = mSendQueue.GetHead();
    mSendQueue.Dequeue(*message);
    Message::Free(*message);
    HandleSendDone();
}

}  // namespace Thread
