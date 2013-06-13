/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

#include "sync-tunnel-comm.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"
#include "ns3/realtime-simulator-impl.h"
#include "ns3/sync-simulator-impl.h"
#include "ns3/system-thread.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>

NS_LOG_COMPONENT_DEFINE ("SyncTunnelComm");


namespace ns3 {


NS_OBJECT_ENSURE_REGISTERED (SyncTunnelComm);

// in the beginning there is no instance of SyncTunnelComm
SyncTunnelComm* SyncTunnelComm::m_comm = NULL;

// create a global variable for the address and port to receive on
GlobalValue g_syncTunRecvAddress = GlobalValue ("SyncTunnelReceiveAddress",
  "The address on which packets of the sync tunnel are received",
  Ipv4AddressValue ("0.0.0.0"),
  MakeIpv4AddressChecker());

GlobalValue g_syncTunRecvPort = GlobalValue ("SyncTunnelReceivePort",
  "The port on which packets of the sync tunnel are received",
  UintegerValue (7544),
  MakeUintegerChecker<uint16_t>());


TypeId 
SyncTunnelComm::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SyncTunnelComm")
    .SetParent<Object> ()
    ;
  return tid;
}

SyncTunnelComm::SyncTunnelComm ()
{
  NS_LOG_FUNCTION_NOARGS ();

  // get raw pointers of simulator implementations (needed to schedule received packets)
  // (the decision which of them to use is made later on)
  Ptr<RealtimeSimulatorImpl> rtImpl = DynamicCast<RealtimeSimulatorImpl> (Simulator::GetImplementation ());
  m_rtImpl = GetPointer (rtImpl);
  Ptr<SyncSimulatorImpl> syncImpl = DynamicCast<SyncSimulatorImpl> (Simulator::GetImplementation ());
  m_syncImpl = GetPointer (syncImpl);

  // get port and address to receive on (are stored in a global variable)
  UintegerValue localPort;
  g_syncTunRecvPort.GetValue (localPort);
  Ipv4AddressValue localAddress;
  g_syncTunRecvAddress.GetValue (localAddress);

  // create socket
  NS_LOG_LOGIC("Creating socket to receive tunnel data");
  m_sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_IP);
  NS_ABORT_MSG_IF (m_sock < 0, "SyncTunnelComm::SyncTunnelComm(): Could not create socket!");

  // bind socket
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl ((localAddress.Get ()).Get ());
  sa.sin_port = htons (localPort.Get ());
  int bound = bind (m_sock, (struct sockaddr *)&sa, sizeof (struct sockaddr));
  NS_ABORT_MSG_IF (bound < 0, "SyncTunnelComm::SyncTunnelComm(): Could not bind socket!");

  // start up the read thread
  NS_LOG_LOGIC("Creating thread which waits for tunnel data");
  m_readThread = Create<SystemThread> (MakeCallback (&SyncTunnelComm::ReadThread, this));
  m_readThread->Start ();
}

SyncTunnelComm::~SyncTunnelComm ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void SyncTunnelComm::Register (uint32_t flowid, SyncTunnelBridge* bridge)
{
  NS_LOG_FUNCTION (flowid << bridge);

  NS_LOG_LOGIC("SyncTunnelBridge registered with flowid " << flowid);

  // check if a SyncTunnelComm instance has been created already and do so otherwise
  if(m_comm == NULL)
    {
    NS_LOG_LOGIC("Creating SyncTunnelComm since this is the first bridge which registered");
    m_comm = new SyncTunnelComm();
    }

  // insert an entry for this flowid in the map
  NS_ABORT_MSG_IF (m_comm->m_allBridges.count (flowid) != 0, "SyncTunnelComm::Register(): Flowid " << flowid << " is already registered");
  m_comm->m_allBridges.insert (std::pair<int32_t, SyncTunnelBridge*> (flowid, bridge));

}

void SyncTunnelComm::Unregister (uint32_t flowid)
{
  NS_LOG_FUNCTION (flowid);

  NS_LOG_LOGIC("SyncTunnelBridge with flowid " << flowid << " wants to unregister");

  // remove from map
  NS_ABORT_MSG_IF (m_comm->m_allBridges.count (flowid) != 1, "SyncTunnelComm::Unregister(): Flowid " << flowid << " is not registered");
  m_comm->m_allBridges.erase (flowid);

  // if this was the last registered bridge, stop and remove the comm object
  if (m_comm->m_allBridges.empty ())
    {
    NS_LOG_LOGIC("Stopping the SyncTunnelComm instance since this was the last registered bridge");
    
    // stop thread
    m_comm->m_readThread->Join ();
    m_comm->m_readThread = 0;

    // close socket
    close(m_comm->m_sock);

    // delete comm object
    free(m_comm);
    m_comm = NULL;
    }

}

ssize_t SyncTunnelComm::SendTo(const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
  NS_ASSERT(m_comm != NULL);
  NS_LOG_LOGIC("Sending packet through SyncTunnel");
  return sendto(m_comm->m_sock, buf, len, flags, dest_addr, addrlen);
}

void SyncTunnelComm::ReadThread() {
  NS_LOG_FUNCTION_NOARGS ();

  // run in an infinte loop and wait for packets to arrive
  for(;;)
   {
     // receive next packet
     NS_LOG_LOGIC("Waiting for the next packet to arrive");
     uint8_t* databuffer = (uint8_t*) malloc (2000);
     NS_ABORT_MSG_IF(databuffer == NULL, "SyncTunnelComm::ReadThread(): malloc failed");
     int bytes_received = recv (m_sock, databuffer, 2000, 0);
     if(bytes_received == -1)
       {
       free(databuffer);
       continue;
       }
     NS_LOG_LOGIC("Received a packet");

     // Split up the TunPacket into its components
     struct SyncBridgeCom::TunPacket *tpacket = (struct SyncBridgeCom::TunPacket*) databuffer;

     int32_t packet_flowid = ntohs(tpacket->flowid);
     int32_t packet_len = ntohs(tpacket->len);
     uint8_t* packet = (uint8_t*) malloc (packet_len);
     NS_ABORT_MSG_IF(packet == NULL, "SyncTunnelComm::ReadThread(): malloc failed");
     memcpy(packet, tpacket->data, packet_len);
     free(databuffer);

     // Get the bridge object this packet is for
     std::map<int32_t, SyncTunnelBridge*>::iterator it;
     it = m_allBridges.find(packet_flowid);
     // if the flowid is not known, simply drop the packet
     if(it == m_allBridges.end ())
       {
       NS_LOG_LOGIC("Discarding packet since the flowid is unknown");
       continue;
       }

    SyncTunnelBridge *bridge = it->second;

    NS_LOG_INFO ("SyncTunnelBridge::ReadThread(): Received packet");

    // create and schedule an event for this packet
    EventImpl *event =  MakeEvent (&SyncTunnelBridge::ForwardToBridgedDevice, bridge, packet, packet_len);
    uint32_t node_id = bridge->GetNode ()->GetId (); 

    // SyncTunnelComm requires a special SimulatorImpl.
    // If this is RealTimeSimulatorImpl, the received packet has to be scheduled as RealtimeEvent.
    // In case of SyncSimulatorImpl there is another special function which is used.
    if (m_rtImpl != NULL)
      {
        NS_LOG_INFO ("SyncTunnelBridge::ReadThread(): Scheduling handler in RealtimeSimulatorImpl");
        m_rtImpl->ScheduleRealtimeNowWithContext (node_id, event);
      }
    else if (m_syncImpl != NULL)
      {
        NS_LOG_INFO ("SyncTunnelBridge::ReadThread(): Scheduling handler in SyncSimulatorImpl");
        m_syncImpl->ScheduleInCurrentSliceWithContext (node_id, event);
      }
    else
      {
        NS_FATAL_ERROR("SyncTunnelBridge::ReadThread(): SyncTunnelBridge has to be used with RealtimeSimulatorImplementation or SyncSimulatorImplementation!");
      }

   }

}

} // namespace ns3
