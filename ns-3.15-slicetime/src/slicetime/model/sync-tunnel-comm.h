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

#ifndef SYNC_TUNNEL_COMM_H
#define SYNC_TUNNEL_COMM_H

#include "sync-tunnel-bridge.h"
#include "ns3/assert.h"
#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/global-value.h"
#include "ns3/system-thread.h"
#include "ns3/realtime-simulator-impl.h"
#include "ns3/sync-simulator-impl.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <string>
#include <map>



namespace ns3 {

/**
 * \ingroup SyncTunnelBridgeModule
 * 
 * \brief Helper class for SyncTunnelBridge which handles the reception 
 * and sending of packets from a tunnel.
 * 
 * @see SyncTunnelBridge
 */

class SyncTunnelComm : public Object
{
public:
  static TypeId GetTypeId (void);
  
  /**
   * Registers a SyncTunnelBridge object at the SyncTunnelComm instance
   * and creates that instance if not done yet by another Register call which 
   * took place before.
   *
   * \param flowid the flowid to receive packets for
   * \param bridge The SyncTunnelBridge object to which the packets shall be handed
   */
  static void Register(uint32_t flowid, SyncTunnelBridge* bridge);

  /**
   * Unregisters a SyncTunnelBridge object and removes the SyncTunnelComm instance 
   * if no other SyncTunnelBridges are connected.
   * 
   * \param flowid the flowid to unregister
   */
  static void Unregister(uint32_t flowid);

  /**
  * Sends a packet over the socket which is also used for receiving data.
  * (is used by SyncTunnelBridge to send packets without using an extra socket)
  *
  * The parameters are the same as for sendto without the socket.
  */
  static ssize_t SendTo(const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);


private:
  // pointer to the instance of SyncTunnelComm
  // (of which one instance is created at maximum)
  static SyncTunnelComm *m_comm;

  SyncTunnelComm ();
  virtual ~SyncTunnelComm ();

  // runs in extra thread and receives packets
  void ReadThread ();

  // handle for the extra thread used to receive
  Ptr<SystemThread> m_readThread;

  // map to store the association of flowids to registered SyncTunnelBridge objects
  std::map<int32_t, SyncTunnelBridge*> m_allBridges;

  // socket to receive with
  int32_t m_sock;

  /**
   * A copy of a raw pointer to the required real-time simulator implementation.
   * Never free this pointer!
   */
  RealtimeSimulatorImpl *m_rtImpl;

  /**
   * A copy of a raw pointer to the required synchronized simulator implementation.
   * Never free this pointer!
   */
  SyncSimulatorImpl *m_syncImpl;
    
};

} // namespace ns3

#endif /* SYNC_TUNNEL_COMM_H */
