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
// this is an adpated version of tap-bridge.cc

#include "wifi-emu-bridge.h"
#include "wifi-emu-comm.h"
#include "wifi-emu-msg.h"
#include "wifi-emu-tag.h"

#include "ns3/node.h"
#include "ns3/channel.h"
#include "ns3/packet.h"
#include "ns3/ethernet-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/simulator.h"
#include "ns3/integer.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/mac-low.h"

#include <arpa/inet.h> // htons, etc
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#define DEBUG_TIME 0

NS_LOG_COMPONENT_DEFINE ("WifiEmuBridge");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (WifiEmuBridge);

TypeId
WifiEmuBridge::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WifiEmuBridge")
    .SetParent<NetDevice> ()
    .AddConstructor<WifiEmuBridge> ()
    .AddAttribute("ClientId",
                  "The id with which the remote side will register",
                  UintegerValue(1),
                  MakeUintegerAccessor (&WifiEmuBridge::m_clientid),
                  MakeUintegerChecker<uint16_t> ())
    ;
  return tid;
}

WifiEmuBridge::WifiEmuBridge ()
: m_node (0),
  m_ifIndex (0),
  m_mtu (0),
  m_startEvent (),
  m_stopEvent (),
  m_tStart(Seconds(0)),
  m_tStop(Seconds(0)),
  m_isStarted(false),
  m_isConnected(false),
  m_useVirtualBuf(false)
{
  NS_LOG_FUNCTION_NOARGS ();
  
  // Schedule start event
  Start (m_tStart); 
}

WifiEmuBridge::~WifiEmuBridge()
{
  NS_LOG_FUNCTION_NOARGS ();

  m_bridgedDevice = 0;
}

  void 
WifiEmuBridge::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();
  NetDevice::DoDispose ();
}

void
WifiEmuBridge::Start (Time tStart)
{
  NS_LOG_FUNCTION (tStart);
  //
  // Cancel any pending start event and schedule a new one at some relative time in the future.
  //
  Simulator::Cancel (m_startEvent);
  m_startEvent = Simulator::Schedule (tStart, &WifiEmuBridge::StartBridge, this);
}

  void
WifiEmuBridge::Stop (Time tStop)
{
  NS_LOG_FUNCTION (tStop);
  //
  // Cancel any pending stop event and schedule a new one at some relative time in the future.
  //
  Simulator::Cancel (m_stopEvent);
  m_startEvent = Simulator::Schedule (tStop, &WifiEmuBridge::StopBridge, this);
}

void
WifiEmuBridge::RecvMsg (uint8_t type, uint8_t *msg, int len)
{
  NS_LOG_FUNCTION(type << msg << len);

  //
  // just schedule an event in order to have the message processing in the main thread
  // (otherwise the message processing is in the context of the receive thread and locks
  //  would be needed)
  //
  // WifiEmuBridge requires a special SimulatorImpl.
  // If this is RealTimeSimulatorImpl, the received packet has to be scheduled as RealtimeEvent.
  // In case of SyncSimulatorImpl there is another special function which is used.
  //

  EventImpl *event = MakeEvent (&WifiEmuBridge::RecvMsgHandle, this, type, msg, len);
  uint32_t node_id = GetNode ()->GetId (); 

  if (m_rtImpl != NULL)
    {
      NS_LOG_INFO ("WifiEmuBridge::RecvMsg(): Scheduling handler in RealtimeSimulatorImpl");
      m_rtImpl->ScheduleRealtimeNowWithContext (node_id, event);
    }
  else if (m_syncImpl != NULL)
    {
      NS_LOG_INFO ("WifiEmuBridge::RecvMsg(): Scheduling handler in SyncSimulatorImpl");
      m_syncImpl->ScheduleInCurrentSliceWithContext (node_id, event);
    }
  else
    {
      NS_FATAL_ERROR("WifiEmuBridge::RecvMsg(): WifiEmuBridge has to be used with RealtimeSimulatorImplementation or SyncSimulatorImplementation!");
    }
}

void
WifiEmuBridge::RecvMsgHandle (uint8_t type, uint8_t *msg, int len)
{
  NS_LOG_FUNCTION(type << msg << len);

  // The mininum length and the type have already been checked by WifiEmuCommX
  //  so we don't have to do this here.

  NS_LOG_LOGIC("Checking the type of a received message");

  // check the type, the correct size and call the corresponding handler
  switch(type)
    {
      case WIFI_EMU_MSG_REQ_REGISTER:
        if(len != sizeof(struct wifi_emu_msg_req_register)) break;
        RecvMsgReqRegister((struct wifi_emu_msg_req_register *)msg, len);
        return;
      case WIFI_EMU_MSG_UNREGISTER:
        if(len != sizeof(struct wifi_emu_msg_unregister)) break;
        RecvMsgUnregister((struct wifi_emu_msg_unregister *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SETCHANNEL:
        if(len != sizeof(struct wifi_emu_msg_req_setchannel)) break;
        RecvMsgReqSetChannel((struct wifi_emu_msg_req_setchannel *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SETMODE:
        if(len != sizeof(struct wifi_emu_msg_req_setmode)) break;
        RecvMsgReqSetMode((struct wifi_emu_msg_req_setmode *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SETWAP:
        if(len != sizeof(struct wifi_emu_msg_req_setwap)) break;
        RecvMsgReqSetWap((struct wifi_emu_msg_req_setwap *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SETESSID:
        if(len != sizeof(struct wifi_emu_msg_req_setessid)) break;
        RecvMsgReqSetEssid((struct wifi_emu_msg_req_setessid *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SETRATE:
        if(len != sizeof(struct wifi_emu_msg_req_setrate)) break;
        RecvMsgReqSetRate((struct wifi_emu_msg_req_setrate *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SETIFUP:
        if(len != sizeof(struct wifi_emu_msg_req_setifup)) break;
        RecvMsgReqSetIfUp((struct wifi_emu_msg_req_setifup *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SCAN:
        if(len != sizeof(struct wifi_emu_msg_req_scan)) break;
        RecvMsgReqScan((struct wifi_emu_msg_req_scan *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_FER:
        if(len != sizeof(struct wifi_emu_msg_req_fer)) break;
        RecvMsgReqFer((struct wifi_emu_msg_req_fer *)msg, len);
        return;
      case WIFI_EMU_MSG_REQ_SETSPY:
        // RecvMsgReqSetSpy checks length
        RecvMsgReqSetSpy((struct wifi_emu_msg_req_setspy *)msg, len);
        return;
      case WIFI_EMU_MSG_FRAME:
        // RecvMsgFrame checks length
        RecvMsgFrame((struct wifi_emu_msg_frame *)msg, len);
        return;
      default:
        NS_LOG_WARN("Received message with unknown type");
        free(msg);
        return;
    }

  // if we get here the message had an incorrect size
  NS_LOG_WARN("Received message with incorrect length");
  free(msg);
  return;
}

void
WifiEmuBridge::RecvMsgReqRegister(struct wifi_emu_msg_req_register *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  if(msg->virtual_buf)
    m_useVirtualBuf = true;
  else
    m_useVirtualBuf = false;

  free(msg);

  NS_LOG_LOGIC("Received registration request");

  // allocate message
  struct wifi_emu_msg_resp_register *resp = (struct wifi_emu_msg_resp_register *)malloc(sizeof(struct wifi_emu_msg_resp_register));
  NS_ABORT_MSG_IF (resp == 0, "WifiEmuBridge::RecvMsgRegister(): malloc failed");
  resp->h.type = WIFI_EMU_MSG_RESP_REGISTER;
  resp->h.client_id = htons(m_clientid);

  // if driver has already connected, send a negative response
  if(m_isConnected)
    {
    resp->ok = 0;
    NS_LOG_LOGIC("Sending negative response");
    }
  // otherwise set internal values and send positive response
  else
    {
    m_isConnected = true;
    m_isUp = false;
    m_spyAddr.clear();

    resp->ok = 1;
    m_bridgedDevice->GetAddress().CopyTo(resp->mac_address);
    resp->mtu = htons(m_bridgedDevice->GetMtu());
    memcpy(&(resp->wstats), &m_wstats, sizeof(struct wifi_emu_stats));

    NS_LOG_LOGIC("Sending positive response");
    }
  
  // Send message
  WifiEmuComm::Send(m_clientid, (uint8_t *)resp, sizeof(*resp));
}

void
WifiEmuBridge::RecvMsgUnregister(struct wifi_emu_msg_unregister *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  free(msg);

  NS_LOG_LOGIC("Received unregister message");

  // Just set internal values, no response needed
  m_isConnected = false;
  m_isUp = false;
}

void
WifiEmuBridge::RecvMsgReqSetIfUp(struct wifi_emu_msg_req_setifup *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  // set internal value accordingly
  if(msg->up)
    {
    m_isUp = true;
    NS_LOG_LOGIC("Received interface-up request");
    }
  else
    {
    m_isUp = false;
    NS_LOG_LOGIC("Received interface-down request");
    }

  free(msg);

  // send positive response
  SendMsgRespSetter(1);
}

void
WifiEmuBridge::RecvMsgReqSetSpy(struct wifi_emu_msg_req_setspy *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  // abort if we aren't in ad-hoc mode
  if(m_devMode != WIFI_EMU_MODE_ADHOC)
    {
    free(msg);
    NS_LOG_WARN("Spy statistics are only supported in ad-hoc mode");
    SendMsgRespSetter(0);
    return;
    }

  // check message length
  NS_ABORT_MSG_IF((int)(sizeof(struct wifi_emu_msg_req_setspy)+msg->num_addr*WIFI_EMU_MAC_LEN) > len, "WifiEmuBridge::RecvMsgSetSpy(): Received message with incorrect length field");

  NS_LOG_LOGIC("Received spy request");

  // store addresses to watch
  m_spyAddr.clear();
  for(uint8_t i=0; i<msg->num_addr; i++)
    {
    Mac48Address addr;
    addr.CopyFrom(msg->addr[i]);
    m_spyAddr.insert(addr);
    }

  free(msg);

  // send positive response
  SendMsgRespSetter(1);
}

void
WifiEmuBridge::RecvMsgFrame(struct wifi_emu_msg_frame *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  // check that interface is up (otherwise we won't send anything)
  if(!m_isUp)
    {
    NS_LOG_WARN("Discarding packet because interface is not up");
    free(msg);
    return;
    }

  // check length of message
  NS_ABORT_MSG_IF((int)(sizeof(struct wifi_emu_msg_frame)+ntohs(msg->data_len)) > len, "WifiEmuBridge::RecvMsgFrame(): Received frame with incorrect length field");

  NS_LOG_LOGIC("Received packet to send over WiFi");

  // copy data into a new Packet
  Ptr<Packet> packet = Create<Packet> (reinterpret_cast<const uint8_t *> (msg->data), ntohs(msg->data_len));
  free (msg);

  if(m_useVirtualBuf) {
    // add tag containing the packets original size (needed in PacketSent())
    WifiEmuTag tag (packet->GetSize());
    packet->AddPacketTag(tag);
  }
  
  // remove header from packet
  Address src, dst;
  uint16_t type;
  Ptr<Packet> p = Filter (packet, &src, &dst, &type);
  if (p == 0)
    {
      NS_LOG_WARN ("TapBridge::ForwardToBridgedDevice:  Discarding packet as unfit for ns-3 consumption");
      return;
    }

#if DEBUG_TIME
    {
    struct timeval t;
    gettimeofday(&t, NULL);
    printf("SIM_SendToWifi\t%lu\t%06lu\n", t.tv_sec, t.tv_usec);
    fflush(stdout);
    }
#endif

  // send packet
  m_bridgedDevice->Send(packet, dst, type);
  NS_LOG_LOGIC("Handed packet to bridged device");
}

void
WifiEmuBridge::RecvMsgReqSetChannel(struct wifi_emu_msg_req_setchannel *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);
  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  uint16_t channel = ntohs(msg->channel);
  free(msg);

  m_bridgedDevice->GetPhy()->SetChannelNumber(channel);
  m_wstats.channel = htons(channel);

  SendMsgStats();
  SendMsgRespSetter(1);

  NS_LOG_LOGIC("Set channel number to " << channel);
}

void
WifiEmuBridge::RecvMsgReqSetMode(struct wifi_emu_msg_req_setmode *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  NS_LOG_LOGIC("Received set mode request");

  // monitor mode is always possible
  if(msg->mode == WIFI_EMU_MODE_MONITOR)
    {
    m_wstats.mode = WIFI_EMU_MODE_MONITOR;
    // We don't erase the other statistics values, because the ns3 wifi-device is still connected
    // and we don't want to hide this in the statistics.
    NS_LOG_LOGIC("Set mode to monitor");
    }
  // switch to adhoc and master mode is only allowed if the ns-3 device has been configured in this mode
  else if(msg->mode == WIFI_EMU_MODE_MASTER && m_devMode == WIFI_EMU_MODE_MASTER)
    {
    m_wstats.mode = WIFI_EMU_MODE_MASTER;
    NS_LOG_LOGIC("Set mode to master");
    }
  else if(msg->mode == WIFI_EMU_MODE_ADHOC && m_devMode == WIFI_EMU_MODE_ADHOC)
    {
    m_wstats.mode = WIFI_EMU_MODE_ADHOC;
    NS_LOG_LOGIC("Set mode to ad-hoc");
    }
  // otherwise send a negative response
  else 
    {
    free(msg);
    SendMsgRespSetter(0);
    return;
    }
  
  free(msg);
  // update stats and send response
  SendMsgStats();
  SendMsgRespSetter(1);
}

void
WifiEmuBridge::RecvMsgReqSetWap(struct wifi_emu_msg_req_setwap *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  free(msg);

  // send negative response (not implemented)
  SendMsgRespSetter(0);
  NS_LOG_LOGIC("Recceived set access point request - Rejecting");
}

void
WifiEmuBridge::RecvMsgReqSetEssid(struct wifi_emu_msg_req_setessid *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);
  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  m_bridgedDevice->GetMac()->SetSsid(Ssid((char*)(msg->essid), msg->essid_len));

  // directly copy essid into stats (most wifi cards show the ssid after an association has been established)
  memcpy(m_wstats.essid, msg->essid, msg->essid_len);
  m_wstats.essid_len = msg->essid_len;

  free(msg);

  SendMsgStats();
  SendMsgRespSetter(1);

  NS_LOG_LOGIC("Set ssid to " << m_bridgedDevice->GetMac()->GetSsid());
}

void
WifiEmuBridge::RecvMsgReqSetRate(struct wifi_emu_msg_req_setrate *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);
  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  free(msg);

  // send negative response (not implemented)
  SendMsgRespSetter(0);
  NS_LOG_LOGIC("Recceived set rate request - Rejecting");
}

void
WifiEmuBridge::RecvMsgReqScan(struct wifi_emu_msg_req_scan *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  if(!m_isConnected)
    {
    free(msg);
    NS_LOG_WARN("Driver is not connected");
    return;
    }

  // the message contains a flag whether the scan shall be active and may contain a certain ssid
  // however, these are currently ignored (scans are always active and for all ssids)
  free(msg);

  NS_LOG_LOGIC("Received scan request - starting scan");

  m_bridgedDevice->StartActiveScanning(m_scanChannels, MakeCallback(&WifiEmuBridge::ScanCompleted, this));
}

void
WifiEmuBridge::RecvMsgReqFer(struct wifi_emu_msg_req_fer *msg, int len)
{
  NS_LOG_FUNCTION(msg << len);

  NS_LOG_LOGIC("Sending FER");

  // get current FER from MacLow
  double fer_double = m_bridgedDevice->GetMac()->GetMacLow()->MissedAckStatsGetFER();
  uint8_t fer = (uint8_t)(fer_double*100.0+0.5);

  // build message
  struct wifi_emu_msg_resp_fer *resp = (struct wifi_emu_msg_resp_fer *)malloc(sizeof(struct wifi_emu_msg_resp_fer));
  NS_ABORT_MSG_IF (resp == 0, "WifiEmuBridge::RecvMsgReqFer(): malloc failed");
  resp->h.type = WIFI_EMU_MSG_RESP_FER;
  resp->h.client_id = htons(m_clientid);

  // set data
  resp->fer = fer;

  // send message
  WifiEmuComm::Send(m_clientid, (uint8_t *)resp, sizeof(*resp));
}

void WifiEmuBridge::ScanCompleted(std::vector<WifiMac::ScanningEntry> const &results)
{
  NS_LOG_FUNCTION(&results);

  size_t respSize = sizeof(struct wifi_emu_msg_resp_scan) + results.size()*sizeof(wifi_emu_scan_entry);
  struct wifi_emu_msg_resp_scan *resp = (struct wifi_emu_msg_resp_scan *)malloc(respSize);
  NS_ABORT_MSG_IF (resp == 0, "WifiEmuBridge::RecvMsgReqScan(): malloc failed");
  resp->h.type = WIFI_EMU_MSG_RESP_SCAN;
  resp->h.client_id = htons(m_clientid);

  resp->num_entries = results.size();

  for(unsigned int i=0; i<results.size(); i++)
  {
    // channel
    resp->data[i].channel = htons(results[i].channel);
    // mode
    resp->data[i].mode = WIFI_EMU_MODE_MASTER; // scanning is currently only supported in master mode
    // bssid
    results[i].bssid.CopyTo(resp->data[i].remote_mac);
    // ssid
    Ssid ssid = results[i].ssid;
    NS_ABORT_MSG_IF (ssid.GetLength() > WIFI_EMU_ESSID_LEN, "WifiEmuBridge::ScanCompleted(): Scanned network with too long essid");
    memcpy(resp->data[i].essid, ssid.PeekString(), ssid.GetLength());
    resp->data[i].essid_len = ssid.GetLength();
    // qual
    SetQual(&(resp->data[i].qual), results[i].signal, results[i].noise);
  }

  WifiEmuComm::Send(m_clientid, (uint8_t *)resp, respSize);
  NS_LOG_LOGIC("Sent scan reply with " << results.size() << " entries");
}

void
WifiEmuBridge::SendMsgStats()
{
  NS_LOG_FUNCTION_NOARGS();

  NS_LOG_LOGIC("Sending statistics");

  // build message
  struct wifi_emu_msg_stats *msg = (struct wifi_emu_msg_stats *)malloc(sizeof(struct wifi_emu_msg_stats));
  NS_ABORT_MSG_IF (msg == 0, "WifiEmuBridge::SendMsgStats(): malloc failed");
  msg->h.type = WIFI_EMU_MSG_STATS;
  msg->h.client_id = htons(m_clientid);

  // copy stats
  memcpy(&(msg->wstats), &m_wstats, sizeof(struct wifi_emu_stats));
  
  // send message
  WifiEmuComm::Send(m_clientid, (uint8_t *)msg, sizeof(*msg));
}

void
WifiEmuBridge::SendMsgRespSetter(uint8_t ok)
{ 
  NS_LOG_FUNCTION(ok);

  NS_LOG_LOGIC("Sending setter response with ok=" << ok);

  // build message
  struct wifi_emu_msg_resp_setter *resp = (struct wifi_emu_msg_resp_setter *)malloc(sizeof(struct wifi_emu_msg_resp_setter));
  NS_ABORT_MSG_IF (resp == 0, "WifiEmuBridge::SendMsgRespSetter(): malloc failed");
  resp->h.type = WIFI_EMU_MSG_RESP_SETTER;
  resp->h.client_id = htons(m_clientid);
  
  // set ok field
  resp->ok = ok;

  // send message
  WifiEmuComm::Send(m_clientid, (uint8_t *)resp, sizeof(*resp));
}

void
WifiEmuBridge::SendSpyUpdate(Mac48Address addr, double signalDbm, double noiseDbm)
{
  NS_LOG_FUNCTION(addr << signalDbm << noiseDbm);

  NS_LOG_LOGIC("Sending spy update for " << addr);

  // build message
  struct wifi_emu_msg_spy_update *msg = (struct wifi_emu_msg_spy_update *)malloc(sizeof(struct wifi_emu_msg_spy_update));
  NS_ABORT_MSG_IF (msg == 0, "WifiEmuBridge::SendSpyUpdate(): malloc failed");
  msg->h.type = WIFI_EMU_MSG_SPY_UPDATE;
  msg->h.client_id = htons(m_clientid);

  // set data
  addr.CopyTo(msg->addr);
  SetQual(&(msg->qual), signalDbm, noiseDbm);

  // send message
  WifiEmuComm::Send(m_clientid, (uint8_t *)msg, sizeof(*msg));
}

void
WifiEmuBridge::SetQual(struct wifi_emu_qual *qual, double signalDbm, double noiseDbm)
{
  NS_LOG_FUNCTION(qual << signalDbm << noiseDbm);

  // calculate link quality
  qual->qual = (uint8_t)(signalDbm - noiseDbm);
  if(qual->qual > 100)
    qual->qual = 100;
  // set level and noise
  qual->level = RoundToInt8(signalDbm);
  qual->noise = RoundToInt8(noiseDbm);
}

Ptr<Packet>
WifiEmuBridge::Filter (Ptr<Packet> p, Address *src, Address *dst, uint16_t *type)
{
  NS_LOG_FUNCTION (p << src << dst << type);
  uint32_t pktSize;

  NS_LOG_LOGIC("Filtering packet");

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
    NS_LOG_LOGIC("Packet is too small to contain Ethernet-Header");
      return 0;
    }

  p->RemoveHeader (header);

  //
  // If the length/type is less than 1500, it corresponds to a length 
  // interpretation packet.  In this case, it is an 802.3 packet and 
  // will also have an 802.2 LLC header.  If greater than 1500, we
  // find the protocol number (Ethernet type) directly.
  //
  if (header.GetLengthType () <= 1500)
    {
      NS_LOG_LOGIC("Packet seems to contain LLC Header");

      *src = header.GetSource ();
      *dst = header.GetDestination ();

      pktSize = p->GetSize ();
      LlcSnapHeader llc;
      if (pktSize < llc.GetSerializedSize ())
        {
        NS_LOG_LOGIC("Packet is too small for LLC");
          return 0;
        }

        p->RemoveHeader (llc);
        *type = llc.GetType ();
    }
  else
    {
      NS_LOG_LOGIC("Packet has just Ethernet-Header");

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


  void
WifiEmuBridge::StartBridge (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT_MSG (m_isStarted == false, "WifiEmuBridge::StartBridge(): Bridge is already started");
  NS_ASSERT_MSG (m_bridgedDevice != 0, "WifiEmuBridge::StartBridge(): Not bridged to a device");

  NS_LOG_LOGIC("Starting bridge");

  // get pointers of simulator implementation (used later for scheduling)
  // (simply get both, it is later determined which one is used)
  Ptr<RealtimeSimulatorImpl> rtImpl = DynamicCast<RealtimeSimulatorImpl> (Simulator::GetImplementation ());
  m_rtImpl = GetPointer (rtImpl);
  Ptr<SyncSimulatorImpl> syncImpl = DynamicCast<SyncSimulatorImpl> (Simulator::GetImplementation ());
  m_syncImpl = GetPointer (syncImpl);

  // gather statistics
  // standard
  switch(m_bridgedDevice->GetRemoteStationManager()->GetDefaultMode().GetStandard())
  { 
    case WIFI_PHY_STANDARD_80211a:
      m_wstats.standard = WIFI_EMU_STANDARD_80211A; break;
    case WIFI_PHY_STANDARD_80211b:
      m_wstats.standard = WIFI_EMU_STANDARD_80211B; break;
    default:
      NS_FATAL_ERROR("WifiEmuBridge::StartBridge(): Associated device is configured to unsupported standard");
  }
  // channel
  m_wstats.channel = htons(m_bridgedDevice->GetPhy()->GetChannelNumber());
  // mode (master, adhoc)
  std::string macname = m_bridgedDevice->GetMac()->GetInstanceTypeId().GetName();
  if(macname.find("dhocWifiMac") != std::string::npos)
    m_devMode = WIFI_EMU_MODE_ADHOC;
  else if(macname.find("staWifiMac") != std::string::npos)
    m_devMode = WIFI_EMU_MODE_MASTER;
  else
    NS_FATAL_ERROR("WifiEmuBridge::StartBridge(): Associated device is in unsupported mode");
  m_wstats.mode = m_devMode;
  // bssid
  m_bridgedDevice->GetMac()->GetBssid().CopyTo(m_wstats.remote_mac);
  // essid
  Ssid ssid = m_bridgedDevice->GetMac()->GetSsid();
  NS_ABORT_MSG_IF (ssid.GetLength() > WIFI_EMU_ESSID_LEN, "WifiEmuBridge::StartBridge(): Associated to network with too long essid");
  memcpy(m_wstats.essid, ssid.PeekString(), ssid.GetLength());
  m_wstats.essid_len = ssid.GetLength();
  // bitrate
  m_wstats.bitrate = htonl(m_bridgedDevice->GetRemoteStationManager()->GetDefaultMode().GetDataRate());
  // quality (is udpated through received packets)
  m_wstats.qual.qual = 0;
  m_wstats.qual.level = 0;
  m_wstats.qual.noise = 0;

  // register at communications module
  WifiEmuComm::Register(m_clientid, this);

  // set internal values
  m_isStarted = true;

  NS_LOG_LOGIC("Bridge is started");
}

void
WifiEmuBridge::StopBridge (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_LOG_LOGIC("Stopping Bridge");

  // set internal values
  m_isStarted = false;
  m_isConnected = false;
  m_isUp = false;

  // unregister at communication class
  WifiEmuComm::Unregister(m_clientid);

  NS_LOG_LOGIC("Bridge is stopped");
}

Ptr<NetDevice>
WifiEmuBridge::GetBridgedNetDevice (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_bridgedDevice;
}

void 
WifiEmuBridge::SetBridgedNetDevice (Ptr<NetDevice> bridgedDevice)
{
  NS_LOG_FUNCTION (bridgedDevice);

  // perform some checks
  NS_ASSERT_MSG (m_node != 0, "WifiEmuBridge::SetBridgedDevice:  Bridge not installed in a node");
  NS_ASSERT_MSG (bridgedDevice != this, "WifiEmuBridge::SetBridgedDevice:  Cannot bridge to self");
  NS_ASSERT_MSG (m_bridgedDevice == 0, "WifiEmuBridge::SetBridgedDevice:  Already bridged");

  // try to cast to WifiNetDevice (otherwise abort)
  m_bridgedDevice = DynamicCast<WifiNetDevice>(bridgedDevice);
  NS_ASSERT_MSG (m_bridgedDevice != 0, "WifiEmuBridge::SetBridgedNetDevice(): given device is not a WifiNetDevice");
  
  NS_LOG_LOGIC("bridged netdevice set");
  
  // set callbacks
  m_bridgedDevice->SetReceiveCallback (MakeCallback (&WifiEmuBridge::DiscardFromBridgedDevice, this)); // disables upper layers in node, but is not used
  m_bridgedDevice->SetPromiscReceiveCallback (MakeCallback (&WifiEmuBridge::ReceiveFromBridgedDevice, this)); // used for regular reception of packets
  m_bridgedDevice->GetPhy()->SetPromiscSniffRxCallback (MakeCallback (&WifiEmuBridge::ReceivePromiscFromBridgedDevice, this)); // used for reception in monitor mode and statistics
  m_bridgedDevice->GetMac()->SetLinkUpCallback (MakeCallback (&WifiEmuBridge::LinkStateChanged, this)); // called when link goes up
  m_bridgedDevice->GetMac()->SetLinkDownCallback (MakeCallback (&WifiEmuBridge::LinkStateChanged, this)); // called when link goes down
  m_bridgedDevice->GetMac()->SetDcfTxStartCallback (MakeCallback (&WifiEmuBridge::PacketSent, this)); // called when packet is sent over channel

  NS_LOG_LOGIC("callbacks installed");

  // set channels which shall be scanned later on
  switch(m_bridgedDevice->GetRemoteStationManager()->GetDefaultMode().GetStandard())
  { 
    case WIFI_PHY_STANDARD_80211a:
      m_scanChannels = std::vector<uint16_t>(SCAN_CHANNELS_A, SCAN_CHANNELS_A+sizeof(SCAN_CHANNELS_A)/sizeof(SCAN_CHANNELS_A[0])); break;
    case WIFI_PHY_STANDARD_80211b:
      m_scanChannels = std::vector<uint16_t>(SCAN_CHANNELS_B, SCAN_CHANNELS_B+sizeof(SCAN_CHANNELS_B)/sizeof(SCAN_CHANNELS_B[0])); break;
    default:
      NS_FATAL_ERROR("WifiEmuBridge::SetBridgedNetDevice(): Associated device is configured to unsupported standard");
  }
}

bool
WifiEmuBridge::DiscardFromBridgedDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &src)
{
  NS_LOG_FUNCTION (device << packet << protocol << src);
  // just discard packet
  return true;
}

bool
WifiEmuBridge::ReceiveFromBridgedDevice (
  Ptr<NetDevice> device, 
  Ptr<const Packet> packet, 
  uint16_t protocol,
  const Address &src, 
  const Address &dst, 
  PacketType packetType)
{
  NS_LOG_FUNCTION (device << packet << protocol << src << dst << packetType);
  NS_ASSERT_MSG (device == m_bridgedDevice, "WifiEmuBridge::SetBridgedDevice:  Received packet from unexpected device");

#if DEBUG_TIME
    {
    struct timeval t;
    gettimeofday(&t, NULL);
    printf("SIM_GotFromWifi\t%lu\t%06lu\n", t.tv_sec, t.tv_usec);
    fflush(stdout);
    }
#endif

  NS_LOG_LOGIC("Received packet from bridged device");

  // throw away packets not intended for us
  // (promiscious sniffing is used to get destination address)
  if(packetType == PACKET_OTHERHOST)
    return true;

  // don't send anything if we aren't in master or adhoc mode
  if(!(m_wstats.mode == WIFI_EMU_MODE_MASTER || m_wstats.mode == WIFI_EMU_MODE_ADHOC))
    return true;

  // don't send if driver is not connected
  if(!m_isConnected)
    return true;

  // don't send if interface has not been marked "up" in the driver
  if(!m_isUp)
    return true;

  NS_LOG_LOGIC("Sending packet to driver");

  // assemble packet
  Mac48Address from = Mac48Address::ConvertFrom (src);
  Mac48Address to = Mac48Address::ConvertFrom (dst);

  Ptr<Packet> p = packet->Copy ();
  EthernetHeader header = EthernetHeader (false);
  header.SetSource (from);
  header.SetDestination (to);

  header.SetLengthType (protocol);
  p->AddHeader (header);

  // send packet
  NS_ASSERT_MSG (p->GetSize () <= WIFI_EMU_MSG_LEN-sizeof(struct wifi_emu_msg_frame), "WifiEmuBridge::ReceiveFromBridgedDevice: Packet too big " << p->GetSize ());
  struct wifi_emu_msg_frame *msg = (struct wifi_emu_msg_frame *)malloc(sizeof(struct wifi_emu_msg_frame) + p->GetSize());
  NS_ABORT_MSG_IF (msg == 0, "WifiEmuBridge::ReceiveFromBridgedDevice(): malloc failed");
  msg->h.type = WIFI_EMU_MSG_FRAME;
  msg->h.client_id = htons(m_clientid);
  
  msg->data_len = htons(p->GetSize());
  p->CopyData (msg->data, p->GetSize());

  WifiEmuComm::Send(m_clientid, (uint8_t *)msg, sizeof(struct wifi_emu_msg_frame)+p->GetSize());

  return true;
}

bool WifiEmuBridge::ReceivePromiscFromBridgedDevice (
Ptr<WifiPhy> phy,
Ptr<const Packet> packet,
uint16_t channelFreqMhz,
uint16_t channelNumber,
uint32_t rate,
bool isShortPreamble,
double signalDbm,
double noiseDbm)
{
  NS_LOG_FUNCTION (phy << packet << channelFreqMhz << channelNumber << rate << isShortPreamble << signalDbm << noiseDbm);

  NS_LOG_LOGIC("Received sniffed packet");

  // It is some extra work to parse the headers here again, but this handler is called directly from the 
  // PHY layer which does not know anything about MAC. The alternative would have been to pass this information
  // all the way through the usual stack which would have required lots of changes.
  // Another reason is the logical separation. The already existing stack takes care of packets we send and receive, 
  // while this "sniffing" function just hooks into the stack to obtain some information.
  
  WifiMacHeader hdr;
  packet->PeekHeader(hdr);

  // update spy stats in adhoc mode
  // (for all received data packets)
  if(m_isConnected && m_devMode == WIFI_EMU_MODE_ADHOC && hdr.IsData())
    {
    Mac48Address from = hdr.GetAddr2();

    if(m_spyAddr.count(from))
      {
        NS_LOG_LOGIC("Packet is from a node we have to spy on");
        SendSpyUpdate(from, signalDbm, noiseDbm);
      }
    }

  // update quality and bitrate in master mode
  // (for beacons received from AP)
  if(m_isConnected && m_devMode == WIFI_EMU_MODE_MASTER && hdr.IsBeacon())
    {
      Mac48Address bssid = m_bridgedDevice->GetMac()->GetBssid();

      if(hdr.GetAddr3() == bssid)
        {
        NS_LOG_LOGIC("Packet is beacon from our AP");
        SetQual(&(m_wstats.qual), signalDbm, noiseDbm);
        m_wstats.bitrate = htonl(m_bridgedDevice->GetRemoteStationManager()->Lookup(bssid)->GetCurrentMode().GetDataRate());
        SendMsgStats();
        }
    }
  
  // send packet if we are in monitor mode
  if(m_isConnected && m_wstats.mode == WIFI_EMU_MODE_MONITOR)
    {
    NS_LOG_LOGIC("Building radiotap header of packet");

    // radiotap code partially taken from pcap-writer.cc
    #define	RADIOTAP_TSFT               0x00000001
    #define	RADIOTAP_FLAGS              0x00000002
    #define	RADIOTAP_RATE               0x00000004
    #define RADIOTAP_CHANNEL            0x00000008
    #define	RADIOTAP_FHSS               0x00000010
    #define RADIOTAP_DBM_ANTSIGNAL      0x00000020
    #define RADIOTAP_DBM_ANTNOISE       0x00000040
    #define RADIOTAP_LOCK_QUALITY       0x00000080
    #define RADIOTAP_TX_ATTENUATION     0x00000100    
    #define RADIOTAP_DB_TX_ATTENUATION  0x00000200
    #define RADIOTAP_DBM_TX_POWER       0x00000200
    #define RADIOTAP_ANTENNA            0x00000400
    #define RADIOTAP_DB_ANTSIGNAL       0x00000800
    #define RADIOTAP_DB_ANTNOISE        0x00001000
    #define RADIOTAP_EXT                0x10000000

    #define	RADIOTAP_FLAG_NONE	   0x00	
    #define	RADIOTAP_FLAG_CFP	   0x01	
    #define	RADIOTAP_FLAG_SHORTPRE	   0x02	
    #define	RADIOTAP_FLAG_WEP	   0x04	
    #define	RADIOTAP_FLAG_FRAG	   0x08	
    #define	RADIOTAP_FLAG_FCS	   0x10	
    #define	RADIOTAP_FLAG_DATAPAD	   0x20	
    #define	RADIOTAP_FLAG_BADFCS	   0x40	

    #define	RADIOTAP_CHANNEL_TURBO	         0x0010
    #define	RADIOTAP_CHANNEL_CCK	         0x0020
    #define	RADIOTAP_CHANNEL_OFDM	         0x0040
    #define	RADIOTAP_CHANNEL_2GHZ	         0x0080
    #define	RADIOTAP_CHANNEL_5GHZ	         0x0100
    #define	RADIOTAP_CHANNEL_PASSIVE         0x0200
    #define	RADIOTAP_CHANNEL_DYN_CCK_OFDM    0x0400
    #define	RADIOTAP_CHANNEL_GFSK	         0x0800
    #define	RADIOTAP_CHANNEL_GSM             0x1000
    #define	RADIOTAP_CHANNEL_STATIC_TURBO    0x2000
    #define	RADIOTAP_CHANNEL_HALF_RATE       0x4000
    #define	RADIOTAP_CHANNEL_QUARTER_RATE    0x8000

    #define RADIOTAP_RX_PRESENT (RADIOTAP_TSFT | RADIOTAP_FLAGS | RADIOTAP_RATE | RADIOTAP_CHANNEL | RADIOTAP_DBM_ANTSIGNAL | RADIOTAP_DBM_ANTNOISE)
    #define RADIOTAP_RX_LENGTH (8+8+1+1+2+2+1+1)

    // allocate message
    int msg_size = packet->GetSize() + RADIOTAP_RX_LENGTH + sizeof(struct wifi_emu_msg_frame);
    NS_ASSERT_MSG (msg_size <= WIFI_EMU_MSG_LEN, "WifiEmuBridge::ReceivePromiscFromBridgedDevice: Packet too big " << packet->GetSize ());
    struct wifi_emu_msg_frame *msg = (struct wifi_emu_msg_frame *)malloc(msg_size);
    NS_ABORT_MSG_IF (msg == 0, "WifiEmuBridge::ReceivePromiscFromBridgedDevice(): malloc failed");
    msg->h.type = WIFI_EMU_MSG_FRAME;
    msg->h.client_id = htons(m_clientid);
    msg->data_len = htons(packet->GetSize() + RADIOTAP_RX_LENGTH);

    // insert radiotap header
    msg->data[0] = 0; // version
    msg->data[1] = 0; // padding
    msg->data[2] = (RADIOTAP_RX_LENGTH >> 0) & 0xFF;
    msg->data[3] = (RADIOTAP_RX_LENGTH >> 8) & 0xFF;
    msg->data[4] = (RADIOTAP_RX_PRESENT >> 0) & 0xFF;
    msg->data[5] = (RADIOTAP_RX_PRESENT >> 8) & 0xFF;
    msg->data[6] = (RADIOTAP_RX_PRESENT >> 16) & 0xFF;
    msg->data[7] = (RADIOTAP_RX_PRESENT >> 24) & 0xFF;
    uint64_t current = Simulator::Now ().GetMicroSeconds ();
    msg->data[8] = (current >> 0) & 0xFF;
    msg->data[9] = (current >> 8) & 0xFF;
    msg->data[10] = (current >> 16) & 0xFF;
    msg->data[11] = (current >> 24) & 0xFF;
    msg->data[12] = (current >> 32) & 0xFF;
    msg->data[13] = (current >> 40) & 0xFF;
    msg->data[14] = (current >> 48) & 0xFF;
    msg->data[15] = (current >> 56) & 0xFF;
    uint8_t flags = RADIOTAP_FLAG_NONE;
    if (isShortPreamble)
      flags |= RADIOTAP_FLAG_SHORTPRE;
    msg->data[16] = flags;
    msg->data[17] = rate & 0xFF;
    msg->data[18] = (channelFreqMhz >> 0) & 0xFF;
    msg->data[19] = (channelFreqMhz >> 8) & 0xFF;
    uint16_t channelFlags;
    if (channelFreqMhz < 2500)
      channelFlags = RADIOTAP_CHANNEL_2GHZ | RADIOTAP_CHANNEL_CCK;
    else
      channelFlags = RADIOTAP_CHANNEL_5GHZ | RADIOTAP_CHANNEL_OFDM;
    msg->data[20] = (channelFlags >> 0) & 0xFF;
    msg->data[21] = (channelFlags >> 8) & 0xFF;
    msg->data[22] = RoundToInt8(signalDbm);
    msg->data[23] = RoundToInt8(noiseDbm);
    
    // copy received packet
    packet->CopyData (msg->data+RADIOTAP_RX_LENGTH, packet->GetSize());

    // send message
    WifiEmuComm::Send(m_clientid, (uint8_t *)msg, msg_size);

    NS_LOG_LOGIC("Sent monitor mode frame");
    }

  return true;
}

void
WifiEmuBridge::LinkStateChanged(void)
{
  NS_LOG_FUNCTION_NOARGS();

  NS_LOG_LOGIC("Link state changed");
  
  // update bssid
  m_bridgedDevice->GetMac()->GetBssid().CopyTo(m_wstats.remote_mac);
  // reset quality
  m_wstats.qual.qual = 0;
  m_wstats.qual.level = 0;
  m_wstats.qual.noise = 0;
  // send stats message (if already connected)
  if(m_isConnected)
    SendMsgStats();
}

void
WifiEmuBridge::PacketSent(Ptr<const Packet> packet, const WifiMacHeader *hdr)
{
  NS_LOG_FUNCTION(packet << hdr);

  NS_LOG_LOGIC("Packet in lower mac layer sent");

  // This is a bit tricky: Whenever we hand down a packet, it is enqueued in the WiFi stack.
  // In order to maintain a virtual buffer of queued packets, we need to know when the packet 
  // is really sent over the channel. This is what this callback is for. However this might be 
  // managment packets which we have not sent and the size of the packets is also larger than 
  // before (extra headers). Therefore we attached a tag with the original size to the packets.
  // If we now check for this tag, we know that we once sent it and we have the original size as well.

  // search for a WifiEmuTag
  WifiEmuTag tag;
  if(m_useVirtualBuf && packet->PeekPacketTag(tag))
    {
      NS_LOG_LOGIC("This is a packet which we queud once, sending size to driver");

      // send size of packet to driver, so that it can update its virtual buffer
      struct wifi_emu_msg_frame_sent *msg = (struct wifi_emu_msg_frame_sent *)malloc(sizeof(struct wifi_emu_msg_frame_sent));
      NS_ABORT_MSG_IF (msg == 0, "WifiEmuBridge::PacketSent(): malloc failed");
      msg->h.type = WIFI_EMU_MSG_FRAME_SENT;
      msg->h.client_id = htons(m_clientid);

      msg->len = htons(tag.GetSize());

      WifiEmuComm::Send(m_clientid, (uint8_t *)msg, sizeof(*msg));
    }
}


int8_t 
WifiEmuBridge::RoundToInt8 (double value)
{
  NS_LOG_FUNCTION(value);

  if (value < -128)
    {
      return -128;
    }
  if (value > 127)
    {
      return 127;
    }
  return ((int8_t) round(value));
}


void 
WifiEmuBridge::SetIfIndex(const uint32_t index)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_ifIndex = index;
}

uint32_t 
WifiEmuBridge::GetIfIndex(void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_ifIndex;
}

Ptr<Channel> 
WifiEmuBridge::GetChannel (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return 0;
}

void
WifiEmuBridge::SetAddress (Address address)
{
  NS_LOG_FUNCTION (address);
  m_address = Mac48Address::ConvertFrom (address);
}

Address 
WifiEmuBridge::GetAddress (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_address;
}

bool 
WifiEmuBridge::SetMtu (const uint16_t mtu)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_mtu = mtu;
  return true;
}

uint16_t 
WifiEmuBridge::GetMtu (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_mtu;
}


bool 
WifiEmuBridge::IsLinkUp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

void 
WifiEmuBridge::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION_NOARGS ();
}

bool 
WifiEmuBridge::IsBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
WifiEmuBridge::GetBroadcast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
WifiEmuBridge::IsMulticast (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address
WifiEmuBridge::GetMulticast (Ipv4Address multicastGroup) const
{
 NS_LOG_FUNCTION (this << multicastGroup);
 Mac48Address multicast = Mac48Address::GetMulticast (multicastGroup);
 return multicast;
}

bool 
WifiEmuBridge::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return false;
}

bool 
WifiEmuBridge::IsBridge (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  //
  // Returning false from IsBridge in a device called WifiEmuBridge may seem odd
  // at first glance, but this test is for a device that bridges ns-3 devices
  // together.  The Bridge bridge doesn't do that -- it bridges an ns-3 device to
  // a Linux device.  This is a completely different story.
  // 
  return false;
}

bool 
WifiEmuBridge::Send (Ptr<Packet> packet, const Address& dst, uint16_t protocol)
{
  NS_LOG_FUNCTION (packet << dst << protocol);
  NS_FATAL_ERROR ("WifiEmuBridge::Send: You may not call Send on a WifiEmuBridge directly");
  return false;
}

bool 
WifiEmuBridge::SendFrom (Ptr<Packet> packet, const Address& src, const Address& dst, uint16_t protocol)
{
  NS_LOG_FUNCTION (packet << src << dst << protocol);
  NS_FATAL_ERROR ("WifiEmuBridge::Send: You may not call SendFrom on a WifiEmuBridge directly");
  return false;
}

Ptr<Node> 
WifiEmuBridge::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}

void 
WifiEmuBridge::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_node = node;
}

bool 
WifiEmuBridge::NeedsArp (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

void 
WifiEmuBridge::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_rxCallback = cb;
}

void 
WifiEmuBridge::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_promiscRxCallback = cb;
}

bool
WifiEmuBridge::SupportsSendFrom () const
{
  NS_LOG_FUNCTION_NOARGS ();
  return true;
}

Address WifiEmuBridge::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address::GetMulticast (addr);
}

} // namespace ns3
