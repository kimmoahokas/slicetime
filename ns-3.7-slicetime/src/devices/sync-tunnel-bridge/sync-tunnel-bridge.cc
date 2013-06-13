/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 * Copyright (c) 2009 RWTH Aachen University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * adaptions by: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

// This file is adapted from devices/tap-bridge/tap-bridge.cc

#include "sync-tunnel-bridge.h"
#include "sync-tunnel-comm.h"

#include "ns3/node.h"
#include "ns3/channel.h"
#include "ns3/packet.h"
#include "ns3/ethernet-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4.h"
#include "ns3/simulator.h"

#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>

NS_LOG_COMPONENT_DEFINE ("SyncTunnelBridge");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SyncTunnelBridge);

TypeId
SyncTunnelBridge::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SyncTunnelBridge")
    .SetParent<NetDevice> ()
    .AddConstructor<SyncTunnelBridge> ()
    .AddAttribute ("TunnelDestinationAddress",
                   "The tunnel's remote address to send to",
                   Ipv4AddressValue ("127.0.0.1"),
                   MakeIpv4AddressAccessor (&SyncTunnelBridge::m_destAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("TunnelDestinationPort",
                   "The tunnel's remote port to send to",
                   UintegerValue (7543),
                   MakeUintegerAccessor (&SyncTunnelBridge::m_destPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TunnelFlowId",
                   "The tunnel's flow id",
                   IntegerValue (17),
                   MakeIntegerAccessor (&SyncTunnelBridge::m_flowId),
                   MakeIntegerChecker<int32_t> ())
    ;
  return tid;
}

SyncTunnelBridge::SyncTunnelBridge ()
: m_node (0),
  m_ifIndex (0),
  m_mtu (0),
  m_isStarted (false)
{
  NS_LOG_FUNCTION_NOARGS ();
  Simulator::Schedule (Seconds(0), &SyncTunnelBridge::StartTunnel, this);
}

SyncTunnelBridge::~SyncTunnelBridge()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_bridgedDevice = 0;
}

  void 
SyncTunnelBridge::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();
  NetDevice::DoDispose ();
}


  void
SyncTunnelBridge::StartTunnel (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ABORT_MSG_IF (m_isStarted == true, "SyncTunnelBridge::StartTunnel(): Tunnel is already started");

  // register at comm class
  NS_LOG_LOGIC("Registering at SyncTunnelComm with flowId " << m_flowId);
  SyncTunnelComm::Register(m_flowId, this);

  // create data structures needed for sending
  m_destSockAddr.sin_family = AF_INET;
  m_destSockAddr.sin_port = htons(m_destPort);
  m_destSockAddr.sin_addr.s_addr = htonl(m_destAddress.Get());
  
  m_isStarted = true;
}

void
SyncTunnelBridge::StopTunnel (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  m_isStarted = false;

  // unregister at comm class
  NS_LOG_LOGIC("Unregistering at SyncTunnelComm with flowId " << m_flowId);
  SyncTunnelComm::Unregister(m_flowId);
}

void
SyncTunnelBridge::ForwardToBridgedDevice (uint8_t *buf, uint32_t len)
{
  NS_LOG_FUNCTION (buf << len);

  // create Packet out of the buffer which has been received and free that buffer
  Ptr<Packet> packet = Create<Packet> (reinterpret_cast<const uint8_t *> (buf), len);
  free (buf);
  buf = 0;

  Address src, dst;
  uint16_t type;

  NS_LOG_LOGIC ("Received packet from tunnel");

  // check if packet is suited for ns-3
  Ptr<Packet> p = Filter (packet, &src, &dst, &type);
  if (p == 0)
    {
      NS_LOG_LOGIC ("Discarding packet as unfit for ns-3 consumption");
      return;
    }

  NS_LOG_LOGIC ("Pkt source is " << src);
  NS_LOG_LOGIC ("Pkt destination is " << dst);
  NS_LOG_LOGIC ("Pkt LengthType is " << type);

  // send the packet from the bridged device
  // (SendFrom has to be used since on the other side of the tunnel is another bridge
  //   for which reason the source mac addresses of the packets we receive can vary)
  NS_LOG_LOGIC ("Forwarding packet");
  m_bridgedDevice->SendFrom (packet, src, dst, type);
}

Ptr<Packet>
SyncTunnelBridge::Filter (Ptr<Packet> p, Address *src, Address *dst, uint16_t *type)
{
  NS_LOG_FUNCTION (p);
  uint32_t pktSize;

  //
  // We have a candidate packet for injection into ns-3.  We expect that since
  // it came over a socket that provides Ethernet packets, it should be big 
  // enough to hold an EthernetHeader.  If it can't, we signify the packet 
  // should be filtered out by returning 0.
  //
  pktSize = p->GetSize ();
  EthernetHeader header (false);
  if (pktSize < header.GetSerializedSize ())
    {
      return 0;
    }

  p->RemoveHeader (header);

  NS_LOG_LOGIC ("Pkt source is " << header.GetSource ());
  NS_LOG_LOGIC ("Pkt destination is " << header.GetDestination ());
  NS_LOG_LOGIC ("Pkt LengthType is " << header.GetLengthType ());

  //
  // If the length/type is less than 1500, it corresponds to a length 
  // interpretation packet.  In this case, it is an 802.3 packet and 
  // will also have an 802.2 LLC header.  If greater than 1500, we
  // find the protocol number (Ethernet type) directly.
  //
  if (header.GetLengthType () <= 1500)
    {
      *src = header.GetSource ();
      *dst = header.GetDestination ();

      pktSize = p->GetSize ();
      LlcSnapHeader llc;
      if (pktSize < llc.GetSerializedSize ())
        {
          return 0;
        }

        p->RemoveHeader (llc);
        *type = llc.GetType ();
    }
  else
    {
      *src = header.GetSource ();
      *dst = header.GetDestination ();
      *type = header.GetLengthType ();
    }

  //
  // What we give back is a packet without the Ethernet header (nor the 
  // possible llc/snap header) on it.  We think it is ready to send on
  // out the bridged net device.
  //
  return p;
}

Ptr<NetDevice>
SyncTunnelBridge::GetBridgedNetDevice (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_bridgedDevice;
}

void 
SyncTunnelBridge::SetBridgedNetDevice (Ptr<NetDevice> bridgedDevice)
{
  NS_LOG_FUNCTION (bridgedDevice);

  NS_ASSERT_MSG (m_node != 0, "SyncTunnelBridge::SetBridgedDevice():  Bridge not installed in a node");
  NS_ASSERT_MSG (bridgedDevice != this, "SyncTunnelBridge::SetBridgedDevice(): Cannot bridge to self");
  NS_ASSERT_MSG (m_bridgedDevice == 0, "SyncTunnelBridge::SetBridgedDevice(): Already bridged");
  
  if (!Mac48Address::IsMatchingType (bridgedDevice->GetAddress ()))
    {
      NS_FATAL_ERROR ("SyncTunnelBridge::SetBridgedDevice: Device does not support eui 48 addresses: cannot be added to bridge.");
    }
  
  if (!bridgedDevice->SupportsSendFrom ())
    {
      NS_FATAL_ERROR ("SyncTunnelBridge::SetBridgedDevice: Device does not support SendFrom: cannot be added to bridge.");
    }

  //
  // We need to disconnect the bridged device from the internet stack on our
  // node to ensure that only one stack responds to packets inbound over the
  // bridged device.  That one stack lives outside ns-3 so we just blatantly
  // steal the device callbacks.
  //
  // N.B This can be undone if someone does a RegisterProtocolHandler later 
  // on this node.
  //
  bridgedDevice->SetReceiveCallback (MakeCallback (&SyncTunnelBridge::DiscardFromBridgedDevice, this));
  bridgedDevice->SetPromiscReceiveCallback (MakeCallback (&SyncTunnelBridge::ReceiveFromBridgedDevice, this));

  m_bridgedDevice = bridgedDevice;
}

bool
SyncTunnelBridge::DiscardFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &src)
{
  NS_LOG_FUNCTION (device << packet << protocol << src);
  NS_LOG_LOGIC ("Discarding packet stolen from bridged device " << device);
  return true;
}

bool
SyncTunnelBridge::ReceiveFromBridgedDevice (
  Ptr<NetDevice> device, 
  Ptr<const Packet> packet, 
  uint16_t protocol,
  const Address &src, 
  const Address &dst, 
  PacketType packetType)
{
  NS_LOG_FUNCTION (device << packet << protocol << src << dst << packetType);
  NS_ASSERT_MSG (device == m_bridgedDevice, "SyncTunnelBridge::SetBridgedDevice(): Received packet from unexpected device");
  NS_LOG_DEBUG ("Packet UID is " << packet->GetUid ());

  // add ethernet header to the packet which we want to send
  Mac48Address from = Mac48Address::ConvertFrom (src);
  Mac48Address to = Mac48Address::ConvertFrom (dst);

  Ptr<Packet> p = packet->Copy ();
  EthernetHeader header = EthernetHeader (false);
  header.SetSource (from);
  header.SetDestination (to);

  header.SetLengthType (protocol);
  p->AddHeader (header);

  NS_LOG_LOGIC ("Sending packet to tunnel");
  NS_LOG_LOGIC ("Pkt source is " << header.GetSource ());
  NS_LOG_LOGIC ("Pkt destination is " << header.GetDestination ());
  NS_LOG_LOGIC ("Pkt LengthType is " << header.GetLengthType ());
  NS_LOG_LOGIC ("Pkt size is " << p->GetSize ());

  NS_ABORT_MSG_IF(p->GetSize() < 0, "SyncTunnelBridge::ReceiveFromBridgedDevice(): Packet has negative length");
  NS_ABORT_MSG_IF(m_flowId < 0, "SyncTunnelBridge::ReceiveFromBridgedDevice(): Negative flow id");

  // copy the packet into the TunPacket data structure
  struct SyncBridgeCom::TunPacket *tpacket = (struct SyncBridgeCom::TunPacket*) malloc (sizeof (struct SyncBridgeCom::TunPacket) + p->GetSize());
  NS_ABORT_MSG_IF(tpacket == NULL, "SyncTunnelBridge::ReceiveFromBridgedDevice(): malloc failed");
  tpacket->len = htons((uint16_t)(p->GetSize())); // use htons because len and flowid are signed int32
  tpacket->flowid = htons((uint16_t)m_flowId);
  p->CopyData ((uint8_t *)tpacket->data, p->GetSize());

  // send the packet through the tunnel
  int bytes_sent = SyncTunnelComm::SendTo((unsigned char*) tpacket, sizeof (struct SyncBridgeCom::TunPacket) + p->GetSize() , 0, (struct sockaddr*) &m_destSockAddr, sizeof(struct sockaddr_in));
  NS_ABORT_MSG_IF (bytes_sent == -1, "SyncTunnelBridge::ReceiveFromBridgedDevice(): Send error, errno = " << strerror (errno));

  free(tpacket);

  NS_LOG_LOGIC("Sent packet to tunnel");

  return true;
}

void 
SyncTunnelBridge::SetIfIndex(const uint32_t index)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_ifIndex = index;
}

uint32_t 
SyncTunnelBridge::GetIfIndex(void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_ifIndex;
}

Ptr<Channel> 
SyncTunnelBridge::GetChannel (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

void
SyncTunnelBridge::SetAddress (Address address)
{
  NS_LOG_FUNCTION (address);
  m_address = Mac48Address::ConvertFrom (address);
}

Address 
SyncTunnelBridge::GetAddress (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_address;
}

bool 
SyncTunnelBridge::SetMtu (const uint16_t mtu)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_mtu = mtu;
  return true;
}

uint16_t 
SyncTunnelBridge::GetMtu (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_mtu;
}


bool 
SyncTunnelBridge::IsLinkUp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

void 
SyncTunnelBridge::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION_NOARGS ();
}

bool 
SyncTunnelBridge::IsBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
SyncTunnelBridge::GetBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
SyncTunnelBridge::IsMulticast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
SyncTunnelBridge::GetMulticast (Ipv4Address multicastGroup) const
{
 NS_LOG_FUNCTION (this << multicastGroup);
 Mac48Address multicast = Mac48Address::GetMulticast (multicastGroup);
 return multicast;
}

bool 
SyncTunnelBridge::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

bool 
SyncTunnelBridge::IsBridge (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  //
  // Returning false from IsBridge in a device called SyncTunnelBridge may seem odd
  // at first glance, but this test is for a device that bridges ns-3 devices
  // together.  The Tap bridge doesn't do that -- it bridges an ns-3 device to
  // a Linux device.  This is a completely different story.
  // 
  return false;
}

bool 
SyncTunnelBridge::Send (Ptr<Packet> packet, const Address& dst, uint16_t protocol)
{
  NS_LOG_FUNCTION (packet << dst << protocol);
  NS_FATAL_ERROR ("SyncTunnelBridge::Send: You may not call Send on a SyncTunnelBridge directly");
  return false;
}

bool 
SyncTunnelBridge::SendFrom (Ptr<Packet> packet, const Address& src, const Address& dst, uint16_t protocol)
{
  NS_LOG_FUNCTION (packet << src << dst << protocol);
  NS_FATAL_ERROR ("SyncTunnelBridge::Send: You may not call SendFrom on a SyncTunnelBridge directly");
  return false;
}

Ptr<Node> 
SyncTunnelBridge::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}

void 
SyncTunnelBridge::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_node = node;
}

bool 
SyncTunnelBridge::NeedsArp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

void 
SyncTunnelBridge::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_rxCallback = cb;
}

void 
SyncTunnelBridge::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_promiscRxCallback = cb;
}

bool
SyncTunnelBridge::SupportsSendFrom () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address SyncTunnelBridge::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address::GetMulticast (addr);
}

} // namespace ns3
