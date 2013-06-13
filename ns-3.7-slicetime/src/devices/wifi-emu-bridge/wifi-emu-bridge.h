/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 * Copyright (c) 2010 RWTH Aachen University
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
 * Author of WiFi part: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */
// this is an adpated version of tap-bridge.h

#ifndef WIFI_EMU_BRIDGE_H
#define WIFI_EMU_BRIDGE_H

#include "wifi-emu-msg.h"

#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/mac48-address.h"
#include "ns3/realtime-simulator-impl.h"
#include "ns3/sync-simulator-impl.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-mac-header.h"

#include <set>

namespace ns3 {

// list of channels to scan (USA)
static const uint16_t SCAN_CHANNELS_A[] = {36,40,44,48,52,56,60,64,147,151,155,167};
static const uint16_t SCAN_CHANNELS_B[] = {1,2,3,4,5,6,7,8,9,10,11};


class Node;

/**
 * \ingroup WifiEmuBridgeModule
 * 
 * \brief A bridge used to connect a special WiFi driver of a real system to a ns-3 WifiNetDevice
 *
 * This special driver appears to the operating system it is running in like a real WiFi device. 
 * However this device is not real hardware but the bridged WifiNetDevice of ns-3.

 * This bridge is therefore similar to the TapBridge, but adds extra functionality: A TapBridge only
 * supports the exchange of Ethernet frames, whereas a WiFi device in a real system provides more 
 * functionality. Besides the exchange of data frames meta-information as the network associated to, 
 * signal strengths, etc have to be provided.  Also likewise the TapBridge, this Bridge is connected 
 * to a regular WifiNetDevice. 
 *
 * The communication with the network driver residing in a real system, takes place via WifiEmuComm. 
 * See it's documentation for details how to configure the communication specific parameters. 
 *
 * The matching between this bridge and the remote driver takes place via a client id, which can be set 
 * through the ClientId attribute.
 *
 * The BufferSize attribute determines the size of the virtual buffer the device has. If this buffer gets full
 * and too many packets wait to be sent over the wireless channel, the driver is signalled to stop sending packets.
 * (This shall mimick the behavior of a hardware buffer each real WiFi device has)
 */
class WifiEmuBridge : public NetDevice
{
public:
  static TypeId GetTypeId (void);

  WifiEmuBridge ();
  virtual ~WifiEmuBridge ();

  /**
   * \brief Get the bridged net device.
   *
   * The bridged net device is the ns-3 device to which this bridge is connected,
   *
   * \returns the bridged net device.
   */
  Ptr<NetDevice> GetBridgedNetDevice (void);

  /**
   * \brief Set the ns-3 net device to bridge.
   *
   * This method tells the bridge which ns-3 net device it should use to connect
   * the simulation side of the bridge.
   *
   * \param bridgedDevice the device to set
   */
  void SetBridgedNetDevice (Ptr<NetDevice> bridgedDevice);

  /**
   * \brief Set a start time for the device.
   *
   * The bridge bridge consumes a non-trivial amount of time to start.  It starts
   * up in the context of a scheduled event to ensure that all configuration
   * has been completed before extracting the configuration.
   *
   * If this method is not called, the device starts at time 0.
   *
   * \param tStart the start time
   */
  void Start (Time tStart);

  /**
   * Set a stop time for the device.
   *
   * If not used, the device won't be shut down while the simulation runs.
   *
   * \param tStop the stop time
   *
   * \see WifiEmuBridge::Start
   */
  void Stop (Time tStop);

  /**
   * Accepts the messages received through WifiEmuComm
   *  and schedules them as an event in order to have them
   *  being processed in the main thread.
   *
   * \param type the message type
   * \param msg the raw message received
   * \param len length of the message
   */
  void RecvMsg (uint8_t type, uint8_t *msg, int len);

  //
  // The following methods are inherited from NetDevice base class and are
  // documented there.
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

private:
  /**
   * \internal
   *
   * Spin up the device
   */
  void StartBridge (void);

  /**
   * \internal
   *
   * Tear down the device
   */
  void StopBridge (void);

  /**
   * Handles the messages received
   *
   * (RecvMsg schedules an event for this method)
   *
   * \param type the message type
   * \param msg the raw message received
   * \param len length of the message
   */
  void RecvMsgHandle (uint8_t type, uint8_t *msg, int len);

  /**
   * \internal
   *
   * Performs checks and removes headers from received packets.
   *
   * \param packet The packet we received from the host, and which we need 
   *               to check.
   * \param src    A pointer to the data structure that will get the source
   *               MAC address of the packet (extracted from the packet Ethernet
   *               header).
   * \param dst    A pointer to the data structure that will get the destination
   *               MAC address of the packet (extracted from the packet Ethernet 
   *               header).
   * \param type   A pointer to the variable that will get the packet type from 
   *               either the Ethernet header in the case of type interpretation
   *               (DIX framing) or from the 802.2 LLC header in the case of 
   *               length interpretation (802.3 framing).
   */
  Ptr<Packet> Filter (Ptr<Packet> packet, Address *src, Address *dst, uint16_t *type);


  // handlers for callbacks set in the WiFi stack
  bool ReceiveFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol,
                                 Address const &src, Address const &dst, PacketType packetType);
  bool ReceivePromiscFromBridgedDevice (Ptr<WifiPhy> phy, Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate,
                    bool isShortPreamble, double signalDbm, double noiseDbm);
  bool DiscardFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, Address const &src);
  void LinkStateChanged (void);
  void PacketSent(Ptr<const Packet> packet, const WifiMacHeader *hdr);

  // handlers for received messages
  void RecvMsgReqRegister(struct wifi_emu_msg_req_register *msg, int len);
  void RecvMsgUnregister(struct wifi_emu_msg_unregister *msg, int len);
  void RecvMsgReqSetIfUp(struct wifi_emu_msg_req_setifup *msg, int len);
  void RecvMsgFrame(struct wifi_emu_msg_frame *msg, int len);
  void RecvMsgReqSetChannel(struct wifi_emu_msg_req_setchannel *msg, int len);
  void RecvMsgReqSetMode(struct wifi_emu_msg_req_setmode *msg, int len);
  void RecvMsgReqSetWap(struct wifi_emu_msg_req_setwap *msg, int len);
  void RecvMsgReqSetEssid(struct wifi_emu_msg_req_setessid *msg, int len);
  void RecvMsgReqSetRate(struct wifi_emu_msg_req_setrate *msg, int len);
  void RecvMsgReqScan(struct wifi_emu_msg_req_scan *msg, int len);
  void RecvMsgReqFer(struct wifi_emu_msg_req_fer *msg, int len);
  void RecvMsgReqSetSpy(struct wifi_emu_msg_req_setspy *msg, int len);

  /**
   * \internal
   *
   * sends stats to driver
   */
  void SendMsgStats();

  /**
   * \internal
   *
   * send setter response
   *
   * \param ok ok-value to include in the response
   */
  void SendMsgRespSetter(uint8_t ok);

  /**
   * \internal
   *
   * send spy update
   *
   * \param addr the mac address of which we have new data
   * \param signalDbm the signal value
   * \param noiseDbm the noise value
   */
  void SendSpyUpdate(Mac48Address addr, double signalDbm, double noiseDbm);

  /**
   * \internal
   *
   * Sets the level and noise field of a wifi_emu_qual struct and calculates the quality.
   *
   * \param qual the struct to write to
   * \param signalDbm the signal value
   * \param noiseDbm the noise value
   */
  void SetQual(struct wifi_emu_qual *qual, double signalDbm, double noiseDbm);

  /**
   * \internal
   *
   * Converts a float to a signed int
   *
   * \param value the flaot to convert
   */
  int8_t RoundToInt8 (double value);

  /**
   * \internal
   *
   * Gets called when a scan request was completed
   *
   * \param results the list with scan results
   */
  void ScanCompleted(std::vector<WifiMac::ScanningEntry> const &results);

  /**
   * \internal
   *
   * List of channels which are scanned upon scan request
   * (depends on used PHY standard)
   */
  std::vector<uint16_t> m_scanChannels;

  /**
   * \internal
   *
   * Callback used to hook the standard packet receive callback of the WifiEmuBridge
   * ns-3 net device.  This is never called, and therefore no packets will ever
   * be received forwarded up the IP stack on the ghost node through this device.
   */
  NetDevice::ReceiveCallback m_rxCallback;

  /**
   * \internal
   *
   * Callback used to hook the promiscuous packet receive callback of the WifiEmuBridge
   * ns-3 net device.  This is never called, and therefore no packets will ever
   * be received forwarded up the IP stack on the ghost node through this device.
   *
   * Note that we intercept the similar callback in the bridged device in order to
   * do the actual bridging between the bridged ns-3 net device and the Bridge device
   * on the host.
   */
  NetDevice::PromiscReceiveCallback m_promiscRxCallback;

  /**
   * \internal
   *
   * Pointer to the (ghost) Node to which we are connected.
   */
  Ptr<Node> m_node;


  /**
   * \internal
   *
   * The ns-3 interface index of this WifiEmuBridge net device.
   */
  uint32_t m_ifIndex;

  /**
   * \internal
   *
   * The common mtu to use for the net devices
   */
  uint16_t m_mtu;

  /**
   * \internal
   *
   * The ID of the ns-3 event used to schedule the start up of the underlying
   * host Bridge device and ns-3 read thread.
   */
  EventId m_startEvent;

  /**
   * \internal
   *
   * The ID of the ns-3 event used to schedule the tear down of the underlying
   * host Bridge device and ns-3 read thread.
   */
  EventId m_stopEvent;

  /**
   * \internal
   *
   * The (unused) MAC address of the WifiEmuBridge net device.  Since the WifiEmuBridge
   * is implemented as a ns-3 net device, it is required to implement certain
   * functionality.  In this case, the WifiEmuBridge is automatically assigned a
   * MAC address, but it is not used.
   */
  Mac48Address m_address;

  /**
   * \internal
   *
   * Time to start spinning up the device
   */
  Time m_tStart;

  /**
   * \internal
   *
   * Time to start tearing down the device
   */
  Time m_tStop;

  /**
   * \internal
   *
   * clientid of this bridge
   */
  uint16_t m_clientid;

  /**
   * \internal
   *
   * The ns-3 net device to which we are bridging.
   */
  Ptr<WifiNetDevice> m_bridgedDevice;

  /**
   * \internal
   *
   * A copy of a raw pointer to the required real-time simulator implementation.
   * Never free this pointer!
   */
  RealtimeSimulatorImpl *m_rtImpl;

  /**
   * \internal
   *
   * A copy of a raw pointer to the required sync simulator implementation.
   * Never free this pointer!
   */
  SyncSimulatorImpl *m_syncImpl;

  /**
   * \internal
   *
   * Indicates whether the bridge has been started
   */
  bool m_isStarted;

  /**
   * \internal
   *
   * Indicates whether the driver is connected
   */
  bool m_isConnected;

  /**
   * \internal
   *
   * Indicates whether the interface is up
   */
  bool m_isUp;

  /**
   * \internal
   *
   * Buffers statistics which are sent to driver.
   * (so data doesn't has to be gathered again and again)
   */
  struct wifi_emu_stats m_wstats;

  /**
   * \internal
   *
   * Mode of the device we are bridged to
   * (the mode stored in the statistics is just what we currently show the driver)
   */
  uint8_t m_devMode;

  /**
   * \internal
   *
   * List of mac addresses for which we have to sent spy updates
   */
  std::set<Mac48Address> m_spyAddr;

  /**
   * \internal
   *
   * Virtual send buffer size
   */
  uint32_t m_bufferSize;

  /**
   * \internal
   *
   * number of free bytes in the virtual send buffer
   * (is negative when "out of buffer")
   */
  int64_t m_bufferFree;

  /**
   * \internal
   *
   * stores whether the virtual buffer is used
   */
  int64_t m_useVirtualBuf;




};

} // namespace ns3

#endif /* WIFI_EMU_BRIDGE_H */
