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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#include <coap/coap_header.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <mac/mac_frame.h>
#include <thread/thread_netif.h>
#include <thread/thread_tlvs.h>
#include <meshcop/meshcop_tlvs.h>
#include <meshcop/joiner_router.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Meshcop {

JoinerRouter::JoinerRouter(ThreadNetif &netif) :
    m_coap_joiner_entrust(kMeshcopUrl_JoinEnt, &HandleJoinerEntrust, this),
    m_coap_relay_tx(kMeshcopUrl_RelayTx, &HandleRelayTx, this),
    m_socket(&HandleUdpReceive, this),
    m_timer(&HandleTimer, this)
{
    m_netif = &netif;

    m_coap_server = netif.GetCoapServer();
    m_coap_server->AddResource(m_coap_joiner_entrust);
    m_coap_server->AddResource(m_coap_relay_tx);
    m_coap_message_id = Random::Get();
}

/**
 * RLY_RX.ntf
 *
 * NON POST coap://<next_relay_addr>:MM/c/rx
 *
 * Joiner DTLS Encapsulation TLV 
 * Joiner UDP Port TLV
 * Joiner IID TLV
 * Joiner Router Locator TLV
 */
ThreadError JoinerRouter::SendRelayRx(Message &joiner_msg, 
				      const Ip6MessageInfo &joiner_msg_info)
{
    //const Ip6Address &joiner_addr;
    //uint16_t port;

    ThreadError error;
    struct sockaddr_in6 sockaddr;
    Message *message;
    Coap::Header header;
    Ip6MessageInfo message_info;
 
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin6_port = kCoapUdpPort;
    m_socket.Bind(&sockaddr);

    for (int i = 0; i < sizeof(m_coap_token); i++)
    {
        m_coap_token[i] = Random::Get();
    }

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeNonConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++m_coap_message_id);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions(kMeshcopUrl_RelayRx);
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    ThreadJoinerIidTlv joiner_iid_tlv;
    joiner_iid_tlv.Init();
    //joiner_iid_tlv.SetAddress(joiner_msg_info.peer_addr);
    SuccessOrExit(error = message->Append(&joiner_iid_tlv, 
					  sizeof(joiner_iid_tlv)));

    ThreadJoinerUdpPortTlv joiner_port_tlv;
    joiner_port_tlv.Init();
    joiner_port_tlv.port = joiner_msg_info.peer_port;
    SuccessOrExit(error = message->Append(&joiner_port_tlv, 
					  sizeof(joiner_port_tlv)));

    ThreadJoinerRlocTlv joiner_rloc_tlv;
    joiner_rloc_tlv.Init();
    //joiner_rloc_tlv.address = netif_->GetMle()->GetRloc16();
    SuccessOrExit(error = message->Append(&joiner_rloc_tlv, 
					  sizeof(joiner_rloc_tlv)));

    // TODO: Get Border Router RLOC from network data
    // netif_->GetNetworkData()->GetCommissioningDataset();
    // Pull out BorderRloc...
    memset(&message_info, 0, sizeof(message_info));
    message_info.peer_addr.s6_addr16[0] = HostSwap16(0xff03);
    message_info.peer_addr.s6_addr16[7] = HostSwap16(0x0002);
    message_info.peer_port = kCoapUdpPort;

    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent RLY_RX\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void JoinerRouter::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    JoinerRouter *obj = reinterpret_cast<JoinerRouter *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void JoinerRouter::HandleUdpReceive(Message &message, 
				    const Ip6MessageInfo &message_info) 
{
    // if commissioner, send all unsecured traffic to RelayRx
    SendRelayRx(message, message_info);
}

static void HandleRelayTx(void *context, Coap::Header &header, 
			  Message &message, const Ip6MessageInfo &message_info)
{
    JoinerRouter *self = reinterpret_cast<JoinerRouter *>(context);
    self->HandleRelayTx(header, message, message_info);
}
  
/**
 * RLY_TX.ntf
 *
 * NON POST coap://<next_relay_addr>:MM/c/tx
 *
 * Joiner DTLS Encapsulation TLV
 * Joiner UDP Port TLV
 * Joiner IID TLV
 * Joiner Router Locator TLV
 * Joiner Router KEK TLV (optional when Commissioner triggers entrust)
 */
void JoinerRouter::HandleRelayTx(Coap::Header &header, Message &message, 
				 const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    uint8_t tlvs_length;
    uint8_t tlvs[256];
    uint16_t rloc16;

    dprintf("Received RLY_TX.nfy\n");

    tlvs_length = message.GetLength() - message.GetOffset();
    message.Read(message.GetOffset(), tlvs_length, tlvs);

    MeshcopTlv *cur = reinterpret_cast<MeshcopTlv *>(tlvs);
    MeshcopTlv *end = reinterpret_cast<MeshcopTlv *>(tlvs + tlvs_length);

    //rloc16 = HostSwap16(message_info.peer_addr.s6_addr16[7]);

    //JoinerDtlsTx(header, message_info, tlvs, tlvs_length);

    Message *joiner_msg;
    VerifyOrExit((joiner_msg = Udp6::NewMessage(0)) != NULL, 
		 error = kThreadError_NoBufs);

    //message_info.peer_port = kUdpPort;
    //message_info.interface_id = m_netif->GetInterfaceId();
    //message_info.hop_limit = 255;

    // Parse TLVs and build UDP packet to forward to Joiner.
    while (cur < end)
    {
        switch (cur->GetType())
        {
	case MeshcopTlv::kTypeJoinerRouterKek:
	    // if (KEK) Timer.start --> send JOIN_ENT.req
            break;

        case MeshcopTlv::kTypeJoinerDtls:
	  // Copy entire TLV value into joiner_msg UDP paylaod
	  break;

        case MeshcopTlv::kTypeJoinerUdpPort:
	  // Set UDP destination port of joiner_msg.
	  //message_info.peer_port = kUdpPort;
	  break;

        case MeshcopTlv::kTypeJoinerIid:
	  // Set UDP destination address of joiner_msg.
	  break;

        case MeshcopTlv::kTypeJoinerRloc:
	  // Set UDP source address of joiner_msg.
	  // Assert this is equal to own node address.
	  break;

	default:
	    break;
        }

        cur = cur->GetNext();
    }

    // If all TLVs present, send the decapsulated UDP packet.

exit:

    if (error != kThreadError_None && joiner_msg != NULL)
    {
        Message::Free(*joiner_msg);
    }

    //return error;

}


ThreadError JoinerRouter::SendRelayTxDecapsulated
    (const Coap::Header &request_header, 
     const Ip6MessageInfo &message_info,
     const uint8_t *tlvs, 
     uint8_t tlvs_length)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, 
		 error = kThreadError_NoBufs);


exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

static void HandleJoinerEntrust(void *context, Coap::Header &header,
				Message &message, 
				const Ip6MessageInfo &message_info)
{
    JoinerRouter *self = reinterpret_cast<JoinerRouter *>(context);
    self->HandleJoinerEntrust(header, message, message_info);
}

void JoinerRouter::HandleJoinerEntrust(Coap::Header &header, 
				       Message &message, 
				       const Ip6MessageInfo &message_info)
{
}


void JoinerRouter::HandleTimer(void *context)
{
    JoinerRouter *obj = reinterpret_cast<JoinerRouter *>(context);
    obj->HandleTimer();
}

void JoinerRouter::HandleTimer()
{
    bool continue_timer = false;

    if (continue_timer)
    {
        m_timer.Start(1000);
    }
}



}  // namespace Meshcop
}  // namespace Thread
