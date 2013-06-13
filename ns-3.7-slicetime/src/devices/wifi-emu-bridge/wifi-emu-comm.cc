/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

#include "wifi-emu-comm.h"
#include "wifi-emu-comm-udp.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/global-value.h"
#include "ns3/enum.h"

#include <stdlib.h>

NS_LOG_COMPONENT_DEFINE ("WifiEmuComm");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (WifiEmuComm);

WifiEmuComm* WifiEmuComm::m_comm = NULL;

GlobalValue g_wifiEmuCommType = GlobalValue ("WifiEmuCommType",
  "The type of communication used for message exchange",
  EnumValue (WifiEmuComm::CommTypeUDP),
  MakeEnumChecker (WifiEmuComm::CommTypeUDP, "CommTypeUDP"));

TypeId
WifiEmuComm::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WifiEmuComm")
    .SetParent<Object> ()
    ;
  return tid;
}

WifiEmuComm::WifiEmuComm ()
{
  return;
}

WifiEmuComm::~WifiEmuComm ()
{
  return;
}

void WifiEmuComm::Register (uint16_t clientid, WifiEmuBridge* bridge)
{
  NS_LOG_FUNCTION (clientid << bridge);

  NS_LOG_LOGIC("WifiEmuBridge registered with clientid " << clientid);

  // instantiate WifiEmuComm if needed
  if(m_comm == NULL)
    {
      EnumValue type;
      g_wifiEmuCommType.GetValue (type);
      switch(type.Get ())
        {
        case CommTypeUDP:
          NS_LOG_LOGIC("Creating WifiEmuCommUDP");
          m_comm = new WifiEmuCommUdp ();
          break;
        default:
          NS_ABORT_MSG("WifiEmuComm::Register(): WifiEmuCommType contains unknown type");
          break;
        }
    }

  // insert an entry for this client
  NS_ABORT_MSG_IF (m_comm->m_allBridges.count (clientid) != 0, "WifiEmuComm::Register(): clientid " << clientid << " is already registered");
  m_comm->m_allBridges.insert (std::pair<uint16_t, WifiEmuBridge*> (clientid, bridge));
}

void WifiEmuComm::Unregister (uint16_t clientid)
{
  NS_LOG_FUNCTION(clientid);
  NS_ASSERT(m_comm != NULL);
  NS_LOG_LOGIC("WifiEmuComm " << clientid << " wants to unregister");

  // remove from map
  NS_ABORT_MSG_IF (m_comm->m_allBridges.count (clientid) != 1, "WifiEmuComm::Unregister(): clientid " << clientid << " is not registered");
  m_comm->m_allBridges.erase (clientid);

  // destroy WifiEmuComm object if not needed any more
  if(m_comm->m_allBridges.empty ())
    {
      NS_LOG_LOGIC("Stopping WifiEmuComm since last registered bridge was removed");

      // stop thread
      m_comm->m_readThread->Join ();
      m_comm->m_readThread = 0;
      
      // delete WifiEmuComm object
      delete(m_comm);
      m_comm = NULL;
    }

}

void WifiEmuComm::Send(uint16_t clientid, uint8_t* msg, int len)
{
  NS_LOG_FUNCTION(clientid << msg << len);
  NS_ASSERT(m_comm != NULL);

  // use subclass's send method
  m_comm->CommSend(clientid, msg, len);
}

void WifiEmuComm::CommStart ()
{
  NS_LOG_FUNCTION_NOARGS();

  // start the reading thread
  NS_LOG_LOGIC("Creating thread which waits for messages");
  m_readThread = Create<SystemThread> (MakeCallback (&WifiEmuComm::ReadThread, this));
  m_readThread->Start();
}

void WifiEmuComm::ReadThread()
{
  NS_LOG_FUNCTION_NOARGS();

  // wait in a loop for messages to arrive
  for(;;)
    {
      NS_LOG_LOGIC("Waiting for message to arrive");
      uint8_t *msg = (uint8_t*) malloc(WIFI_EMU_MSG_LEN);
      NS_ABORT_MSG_IF(msg == NULL, "WifiEmuComm::ReadThread(): malloc failed");

      uint16_t clientid;
      uint8_t type;
      int len;
      len = CommRecv(clientid, type, msg);
      NS_LOG_LOGIC("Received a message");

      // get bridge listed in packet
      std::map<uint16_t, WifiEmuBridge*>::iterator it;
      it = m_allBridges.find(clientid);
      if(it == m_allBridges.end ())
        {
          NS_LOG_LOGIC("Received packet with unknown clientid");
          free(msg);
          continue;
        }

      // pass message to that bridge
      it->second->RecvMsg(type, msg, len);
    }

}


} // namespace ns3



