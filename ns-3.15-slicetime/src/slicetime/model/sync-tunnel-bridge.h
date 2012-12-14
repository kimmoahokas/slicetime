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
 *
 */

// this file is adapted from devices/tap-bridge/tap-bridge.h

#ifndef SYNC_TUNNEL_BRIDGE_H
#define SYNC_TUNNEL_BRIDGE_H

#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/mac48-address.h"
#include "ns3/system-thread.h"

#include <arpa/inet.h>
#include <string.h>

namespace ns3 {

  // the data structure used to send ethernet frames through the tunnel
  namespace SyncBridgeCom {
    struct TunPacket {
      int32_t flowid;
      int32_t len;
      char data[];
    };

  } // namespace SyncBridgeCom


class Node;


/**
 * \ingroup devices
 * \defgroup SyncTunnelBridgeModel Sync Tunnel Bridge Model
 * @see SyncTunnelBridge
 */

/**
 * \ingroup SyncTunnelBridgeModel
 *
 * \brief A bridge to connect an ns-3 net device to a simple udp tunnel
 * which can be connected to another simulation or virtual machine on the 
 * other side.
 * 
 * From a ns-3 perspective the SyncTunnelBridge is a bridge similar to TapBridge, 
 * which means that one cannot use applications in the node where the SyncTunnelBridge 
 * is installed. Such a "ghost node" just bridges to an usual net device which is connected 
 * to some channel.
 *
 * It's use is to connect ns-3 with different simulators or virtual machines and it was 
 * developed in conjunction with SyncSimulatorImpl.
 *
 * It would have also been possible to use a design similar to EmuNetDevice, which allows 
 * to use applications in the same node. In this case it is however more difficult to 
 * connect to a normal ns-3 channel, for which reason the bridge approach was chosen.
 * 
 * The udp tunnel uses the following data format:
 * \code
 * struct TunPacket {
 *   int32_t flowid;
 *   int32_t len;
 *   char data[];
 * };
 * \endcode
 * - \em data contains an ethernet frame and \em len is it's length
 * - \em flowid is used to distinguish the traffic of different nodes which share the same udp tunnel
 *
 * Because of flowid it is sufficient to open one listening socket to receive the traffic for all
 * SyncTunnelBridges in the ns-3 simulation. However there has to be one SyncTunnelBridge for each 
 * ghost node which bridges to some net device. In order to solve this, SyncTunnelBridge uses the helper
 * SyncTunnelComm of which only one instance is created in the whole simulation. This helper spins up 
 * an extra thread which waits for packets from the tunnel endpoint. Because of this extra thread, SyncTunnelBridge
 * has to be used with either SyncSimulatorImpl or RealtimeSimulatorImpl. The socket which SyncTunnelComm creates 
 * is also used by all SyncTunnelBridges to send the data, but with different recipient addresses if needed.
 *
 * SyncTunnelBridge has 3 configuration attributes:
 * - \em TunnelDestinationAddress and \em TunnelDestinationPort determine the tunnel end point to which all data 
 *   is sent
 * - \em TunnelFlowId sets the flow id which is used inside the TunPackets which are sent to the other side and 
 *   therefore determines the node on the other side of the tunnel
 *
 * Since the port and address to receive traffic on is used in SyncTunnelComm of which only one instance exists,  
 * they are set with the global values SyncTunnelReceivePort and SyncTunnelReceiveAddress.
 */


class SyncTunnelBridge : public NetDevice
{
public:
  static TypeId GetTypeId (void);

  SyncTunnelBridge ();
  virtual ~SyncTunnelBridge ();

  /**
   *  \brief Get the bridged net device.
   *
   * The bridged net device is the ns-3 device to which this bridge is connected,
   *
   * \returns the bridged net device.
   */
  Ptr<NetDevice> GetBridgedNetDevice (void);

  /**
   *  \brief Set the ns-3 net device to bridge.
   *
   * This method tells the bridge which ns-3 net device it should use to connect
   * the simulation side of the bridge.  
   *
   * \param bridgedDevice device to set
   */
  void SetBridgedNetDevice (Ptr<NetDevice> bridgedDevice);

  /*
   * Forward a packet received from the tunnel to the bridged ns-3 device
   *
   * \param buf A character buffer containing the actaul packet bits that were
   *            received from the host.
   * \param buf The length of the buffer.
   */
  void ForwardToBridgedDevice (uint8_t *buf, uint32_t len);

  //
  // The following methods are inherited from NetDevice base class
  //
  virtual void SetIfIndex(const uint32_t index);
  virtual uint32_t GetIfIndex(void) const;
  virtual Ptr<Channel> GetChannel (void) const;
  virtual void SetAddress (Address address);
  virtual Address GetAddress (void) const;
  virtual bool SetMtu (const uint16_t mtu);
  virtual uint16_t GetMtu (void) const;
  virtual bool IsLinkUp (void) const;
  virtual void AddLinkChangeCallback (Callback<void> callback);
  virtual bool IsBroadcast (void) const;
  virtual Address GetBroadcast (void) const;
  virtual bool IsMulticast (void) const;
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;
  virtual bool IsPointToPoint (void) const;
  virtual bool IsBridge (void) const;
  virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
  virtual Ptr<Node> GetNode (void) const;
  virtual void SetNode (Ptr<Node> node);
  virtual bool NeedsArp (void) const;
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
  virtual void SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb);
  virtual bool SupportsSendFrom () const;
  virtual Address GetMulticast (Ipv6Address addr) const;

protected:
  virtual void DoDispose (void);

  // takes a packet from the bridged ns-3 device and sends it into the tunnel
  bool ReceiveFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol,
                                 Address const &src, Address const &dst, PacketType packetType);
  bool DiscardFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, Address const &src);

private:

  // setups the tunnel
  void StartTunnel (void);
 
  // shuts down the tunnel
  void StopTunnel (void);

  // checks whether packet is suited for ns-3
  Ptr<Packet> Filter (Ptr<Packet> packet, Address *src, Address *dst, uint16_t *type);

  // receive callbacks
  // (just for compatiblity to NetDevice; are never called)
  NetDevice::ReceiveCallback m_rxCallback;
  NetDevice::PromiscReceiveCallback m_promiscRxCallback;

  // node we are connected to
  Ptr<Node> m_node;
  
  // index of this device
  uint32_t m_ifIndex;

  // MTU for net devices
  uint16_t m_mtu;

  // used to send data into the tunnel
  struct sockaddr_in m_destSockAddr;

  Mac48Address m_address;

  // configuration parameters of this tunnel
  Ipv4Address m_destAddress;
  uint16_t m_destPort;
  int32_t m_flowId;

  // mac address of this device
  // (again for compatibility; is not used)
  Ptr<NetDevice> m_bridgedDevice;

  // stores whether this bridge has been started
  bool m_isStarted;

};

} // namespace ns3

#endif /* SYNC_TUNNEL_BRIDGE_H */
