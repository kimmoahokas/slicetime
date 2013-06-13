/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 Emmanuelle Laprise
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
 * Author: Emmanuelle Laprise <emmanuelle.laprise@bluekazoo.ca>
 * adaptions by: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

// this file is an adapted version of devices/csma/csma-channel.cc

#include "csma-duplex-channel.h"
#include "csma-duplex-net-device.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/csma-channel.h"

NS_LOG_COMPONENT_DEFINE ("CsmaDuplexChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (CsmaDuplexChannel);

  TypeId 
CsmaDuplexChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CsmaDuplexChannel")
    .SetParent<Channel> ()
    .AddConstructor<CsmaDuplexChannel> ()
    .AddAttribute ("DataRate", 
                   "The transmission data rate to be provided to devices connected to the channel",
                   DataRateValue (DataRate (0xffffffff)),
                   MakeDataRateAccessor (&CsmaDuplexChannel::m_bps),
                   MakeDataRateChecker ())
    .AddAttribute ("Delay", "Transmission delay through the channel",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&CsmaDuplexChannel::m_delay),
                   MakeTimeChecker ())
    ;
  return tid;
}

CsmaDuplexChannel::CsmaDuplexChannel ()
: 
  //Channel ("Csma Channel")
  // suraj: port to ns3.7
  Channel()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_state0 = IDLE;
  m_state1 = IDLE;
  m_deviceList.clear();
}

  int32_t
CsmaDuplexChannel::Attach (Ptr<CsmaDuplexNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  NS_ABORT_MSG_IF (m_deviceList.size () == 2, "CsmaDuplexChannel::Attach(): CsmaDuplexChannel may only be used with up to two devices attached");

  CsmaDuplexDeviceRec rec (device);
  
  m_deviceList.push_back (rec);
  return (m_deviceList.size () - 1);
}

  bool
CsmaDuplexChannel::Reattach (Ptr<CsmaDuplexNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  std::vector<CsmaDuplexDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end( ); it++) 
    {
      if (it->devicePtr == device) 
        {
          if (!it->active) 
            {
              it->active = true;
              return true;
            } 
          else 
            {
              return false;
            }
        }
    }
  return false;
}

  bool
CsmaDuplexChannel::Reattach (uint32_t deviceId)
{
  NS_LOG_FUNCTION (this << deviceId);

  if (deviceId < m_deviceList.size ())
    {
      return false;
    }

  if (m_deviceList[deviceId].active)
    {
      return false;
    } 
  else 
    {
      m_deviceList[deviceId].active = true;
      return true;
    }
}

  bool
CsmaDuplexChannel::Detach (uint32_t deviceId)
{
  NS_LOG_FUNCTION (this << deviceId);

  if (deviceId < m_deviceList.size ())
    {
      if (!m_deviceList[deviceId].active)
        {
          NS_LOG_WARN ("CsmaDuplexChannel::Detach(): Device is already detached (" << deviceId << ")");
          return false;
        }

      m_deviceList[deviceId].active = false;

      if ((m_state0 == TRANSMITTING) && (m_currentSrc0 == deviceId))
        {
          NS_LOG_WARN ("CsmaDuplexChannel::Detach(): Device is currently" << "transmitting (" << deviceId << ")");
        }
      if ((m_state1 == TRANSMITTING) && (m_currentSrc1 == deviceId))
        {
          NS_LOG_WARN ("CsmaDuplexChannel::Detach(): Device is currently" << "transmitting (" << deviceId << ")");
        }

      return true;
    } 
  else 
    {
      return false;
    }
}

  bool
CsmaDuplexChannel::Detach (Ptr<CsmaDuplexNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT (device != 0);

  std::vector<CsmaDuplexDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if ((it->devicePtr == device) && (it->active)) 
        {
          it->active = false;
          return true;
        }
    }
  return false;
}

  bool
CsmaDuplexChannel::TransmitStart (Ptr<Packet> p, uint32_t srcId)
{
  NS_LOG_FUNCTION (this << p << srcId);
  NS_LOG_INFO ("UID is " << p->GetUid () << ")");

  if ((srcId == 0 && m_state0 != IDLE) || (srcId == 1 && m_state1 != IDLE))
    {
      NS_LOG_WARN ("CsmaDuplexChannel::TransmitStart(): State is not IDLE");
      return false;
    }

  if (!IsActive(srcId))
    {
      NS_LOG_ERROR ("CsmaDuplexChannel::TransmitStart(): Seclected source is not currently attached to network");
      return false;
    }

  NS_LOG_LOGIC ("switch to TRANSMITTING");
  if (srcId == 0)
    {
      m_currentPkt0 = p;
      m_currentSrc0 = srcId;
      m_state0 = TRANSMITTING;
    }
  else
    {
      m_currentPkt1 = p;
      m_currentSrc1 = srcId;
      m_state1 = TRANSMITTING;
    }
  return true;
}


  bool
CsmaDuplexChannel::IsActive(uint32_t deviceId) 
{
  return (m_deviceList[deviceId].active);
}

  bool
CsmaDuplexChannel::TransmitEnd(uint32_t srcId)
{
  if (srcId == 0)
    {
      NS_LOG_FUNCTION (this << m_currentPkt0 << m_currentSrc0);
      NS_LOG_INFO ("UID is " << m_currentPkt0->GetUid () << ")");

      NS_ASSERT (m_state0 == TRANSMITTING);
      m_state0 = PROPAGATING;

      bool retVal = true;

      if (!IsActive (m_currentSrc0))
        {
          NS_LOG_ERROR ("CsmaDuplexChannel::TransmitEnd(): Seclected source was detached before the end of the transmission");
          retVal = false;
        }

      NS_LOG_LOGIC ("Schedule event in " << m_delay.GetSeconds () << " sec");

      //Suraj: Added the below. porting to ns3.7
      NS_LOG_LOGIC ("Receive");
      
      std::vector<CsmaDuplexDeviceRec>::iterator it;
      uint32_t devId = 0;
      for (it = m_deviceList.begin (); it < m_deviceList.end(); it++) 
        {
          if (it->IsActive ())
          {
			//Suraj: porting to ns3.7
            //it->devicePtr->Receive (m_currentPkt0->Copy (), m_deviceList[m_currentSrc0].devicePtr);
            Simulator::ScheduleWithContext (it->devicePtr->GetNode ()->GetId (), 
											m_delay,
										    &CsmaDuplexNetDevice::Receive, it->devicePtr,
     										m_currentPkt0->Copy (), m_deviceList[m_currentSrc0].devicePtr);
          }
          devId++;
        }

      Simulator::Schedule (m_delay, &CsmaDuplexChannel::PropagationCompleteEvent,
        this, 0);
      return retVal;
    }
  else
    {
      NS_LOG_FUNCTION (this << m_currentPkt1 << m_currentSrc1);
      NS_LOG_INFO ("UID is " << m_currentPkt1->GetUid () << ")");

      NS_ASSERT (m_state1 == TRANSMITTING);
      m_state1 = PROPAGATING;

      bool retVal = true;

      if (!IsActive (m_currentSrc1))
        {
          NS_LOG_ERROR ("CsmaDuplexChannel::TransmitEnd(): Seclected source was detached before the end of the transmission");
          retVal = false;
        }

      NS_LOG_LOGIC ("Schedule event in " << m_delay.GetSeconds () << " sec");

      // Suraj: added below.
      NS_LOG_LOGIC ("Receive");
      
      std::vector<CsmaDuplexDeviceRec>::iterator it;
      uint32_t devId = 0;
      for (it = m_deviceList.begin (); it < m_deviceList.end(); it++) 
        {
          if (it->IsActive ())
          {
			//Suraj: porting to ns3.7
            //it->devicePtr->Receive (m_currentPkt1->Copy (), m_deviceList[m_currentSrc1].devicePtr);
			Simulator::ScheduleWithContext (it->devicePtr->GetNode ()->GetId (), 
											m_delay,
										    &CsmaDuplexNetDevice::Receive, it->devicePtr,
     										m_currentPkt1->Copy (), m_deviceList[m_currentSrc1].devicePtr);
          }
          devId++;
        }

      Simulator::Schedule (m_delay, &CsmaDuplexChannel::PropagationCompleteEvent,
        this, 1);
      return retVal;
    }

}

  void
CsmaDuplexChannel::PropagationCompleteEvent(uint32_t srcId)
{
  if (srcId == 0)
    {
      NS_LOG_FUNCTION (this << m_currentPkt0);
      NS_LOG_INFO ("UID is " << m_currentPkt0->GetUid () << ")");

      NS_ASSERT (m_state0 == PROPAGATING);

     /* NS_LOG_LOGIC ("Receive");
      
      std::vector<CsmaDuplexDeviceRec>::iterator it;
      uint32_t devId = 0;
      for (it = m_deviceList.begin (); it < m_deviceList.end(); it++) 
        {
          if (it->IsActive ())
          {
			//Suraj: porting to ns3.7
            //it->devicePtr->Receive (m_currentPkt0->Copy (), m_deviceList[m_currentSrc0].devicePtr);
            Simulator::ScheduleWithContext (it->devicePtr->GetNode()->GetId(), 
											m_delay,
										    &CsmaDuplexNetDevice::Receive, it->devicePtr,
     										m_currentPkt0->Copy (), m_deviceList[m_currentSrc0].devicePtr);
          }
          devId++;
        } */
      m_state0 = IDLE;
    }
  else
    {
      NS_LOG_FUNCTION (this << m_currentPkt1);
      NS_LOG_INFO ("UID is " << m_currentPkt1->GetUid () << ")");

      NS_ASSERT (m_state1 == PROPAGATING);

     /* NS_LOG_LOGIC ("Receive");
      
      std::vector<CsmaDuplexDeviceRec>::iterator it;
      uint32_t devId = 0;
      for (it = m_deviceList.begin (); it < m_deviceList.end(); it++) 
        {
          if (it->IsActive ())
          {
			//Suraj: porting to ns3.7
            //it->devicePtr->Receive (m_currentPkt1->Copy (), m_deviceList[m_currentSrc1].devicePtr);
			Simulator::ScheduleWithContext (it->devicePtr->GetNode()->GetId(), 
											m_delay,
										    &CsmaDuplexNetDevice::Receive, it->devicePtr,
     										m_currentPkt1->Copy (), m_deviceList[m_currentSrc1].devicePtr);
          }
          devId++;
        } */
      m_state1 = IDLE;
    }
}

  uint32_t 
CsmaDuplexChannel::GetNumActDevices (void)
{
  int numActDevices = 0;
  std::vector<CsmaDuplexDeviceRec>::iterator it;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if (it->active)
        {
          numActDevices++;
        }
    }
  return numActDevices;
}

  uint32_t 
CsmaDuplexChannel::GetNDevices (void) const
{
  return (m_deviceList.size ());
}

  Ptr<CsmaDuplexNetDevice>
CsmaDuplexChannel::GetCsmaDevice (uint32_t i) const
{
  Ptr<CsmaDuplexNetDevice> netDevice = m_deviceList[i].devicePtr;
  return netDevice;
}

  int32_t
CsmaDuplexChannel::GetDeviceNum (Ptr<CsmaDuplexNetDevice> device)
{
  std::vector<CsmaDuplexDeviceRec>::iterator it;
  int i = 0;
  for (it = m_deviceList.begin (); it < m_deviceList.end (); it++) 
    {
      if (it->devicePtr == device)
        {
          if (it->active) 
            {
              return i;
            } 
          else 
            {
              return -2;
            }
        }
      i++;
    }
  return -1;
}

  bool 
CsmaDuplexChannel::IsBusy (uint32_t srcId)
{
  if ((srcId == 0 && m_state0 == IDLE) || (srcId == 1 && m_state1 == IDLE))
    {
      return false;
    } 
  else 
    {
      return true;
    }
}

  DataRate
CsmaDuplexChannel::GetDataRate (void)
{
  return m_bps;
}

  Time
CsmaDuplexChannel::GetDelay (void)
{
  return m_delay;
}

  WireState
CsmaDuplexChannel::GetState (uint32_t srcId)
{
  if (srcId == 0)
    return m_state0;
  else
    return m_state1;
}

  Ptr<NetDevice>
CsmaDuplexChannel::GetDevice (uint32_t i) const
{
  return GetCsmaDevice (i);
}

CsmaDuplexDeviceRec::CsmaDuplexDeviceRec ()
{
  active = false;
}

CsmaDuplexDeviceRec::CsmaDuplexDeviceRec (Ptr<CsmaDuplexNetDevice> device)
{
  devicePtr = device;
  active = true;
}

  bool
CsmaDuplexDeviceRec::IsActive ()
{
  return active;
}

} // namespace ns3
