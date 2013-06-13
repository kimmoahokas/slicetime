/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 University of Washington
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

// this file is adapted from simulator/realtime-simulator-impl.h

/* The difference to the RealtimeSimulatorImpl is that the synchronization 
 * is not with regard to wall-clock time but to the synchronization server.
 * The first idea was to put this functionality into a class similar to WallClockSynchronizer, 
 * but although Synchronizer is quite abstract, it fits only for the realtime case. 
 * For this reason we completely removed the usage of Synchronizer, of which we would have only 
 * used the condition variable, which we can simply do ourselves since here only a normal bool 
 * is needed.
 * DefaultSimulatorImpl as a basis was however too simple, since the support of threads for 
 * "real-world devices" is needed.
 */

#ifndef SYNC_SIMULATOR_IMPL_H
#define SYNC_SIMULATOR_IMPL_H

#include "simulator-impl.h"
#include "scheduler.h"
#include "event-impl.h"
#include "sync-client.h"

#include "ns3/ptr.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/system-mutex.h"

#include <list>
#include <time.h>


// if this is set, the used realtime will be reported to the server
// otherwise just 0 is reported
#define SEND_REALTIME

namespace ns3 {

/**
* \brief Implements a simulation engine which processes events synchronized with other 
* simulation toolkits or virtual machines over a siple protocol.
* 
* The communcation to the synchronization server is done with the helper class SyncClient, 
* where also the communcation protocol is described.
* 
* In order to use this implementation, the global value \em SimulatorImplementationType has to be set to
* \em ns3::SyncSimulatorImpl and the attributes of SyncClient have to be configured accordingly.
* 
* In the file simulator/sync-simulator-impl.h is a define \em SEND_REALTIME, which controls whether the 
* used realtime shall be calculated and reported to the server. If the server doesn't use this information 
* at all it's not very helpful to calculate this value and the define can be commented out. 
*
* If \em SyncTunnelBridge, \em TapBridge or \em EmuNetDevice are used, received packets are scheduled at 
* the end of the timeslice in which they have been received or at the beginning of the next timslice in case
* a packet has been received while waiting for the next run permission. For this reason the chosen timeslice length 
* has a large influence on the packet delay.
* 
* \see SyncClient
*/

class SyncSimulatorImpl : public SimulatorImpl
{
public:
  static TypeId GetTypeId (void);

  SyncSimulatorImpl ();
  ~SyncSimulatorImpl ();

  virtual void Destroy ();
  virtual bool IsFinished (void) const;
  virtual Time Next (void) const;
  virtual void Stop (void);
  virtual void Stop (Time const &time);
  virtual EventId Schedule (Time const &time, EventImpl *event);
  virtual void ScheduleWithContext (uint32_t context, Time const &time, EventImpl *event);
  virtual EventId ScheduleNow (EventImpl *event);
  virtual EventId ScheduleDestroy (EventImpl *event);
  virtual void Remove (const EventId &ev);
  virtual void Cancel (const EventId &ev);
  virtual bool IsExpired (const EventId &ev) const;
  virtual void Run (void);
  virtual void RunOneEvent (void);
  virtual Time Now (void) const;
  virtual Time GetDelayLeft (const EventId &id) const;
  virtual Time GetMaximumSimulationTime (void) const;
  virtual void SetScheduler (ObjectFactory schedulerFactory);
  virtual uint32_t GetContext (void) const;

  // schedules an event at the end of the current sychronized timeslice
  virtual void ScheduleInCurrentSliceWithContext (uint32_t context, EventImpl *event);
  virtual void ScheduleInCurrentSlice (EventImpl *event);

private:
  bool Running (void) const;

  void ProcessOneEvent (void);
  uint64_t NextTs (void) const;
  virtual void DoDispose (void);

  typedef std::list<EventId> DestroyEvents;
  DestroyEvents m_destroyEvents;
  bool m_stop;
  bool m_running;
  
  // instance of an SyncClient to do the communication
  Ptr<SyncClient> m_syncClient;

  // needed because at the first time no finish packet has to be sent
  bool m_firstRound;

  // contains the time limit up to which we are allowed to execute events
  // after that the sync server has to be asked to release the next timeslice
  uint64_t m_barrierTime;

  // stores whether the simulation is currently running or waiting for the next run permission
  // this is needed to determine when a received packet shall be scheduled
  bool m_isWaitingForPermission;

  // used to determine how much realtime it took to process a timeslice
  #ifdef SEND_REALTIME
  struct timeval lastTimeval;
  #endif

  // The following variables are protected using the m_mutex
  Ptr<Scheduler> m_events;
  int m_unscheduledEvents;
  uint32_t m_uid;
  uint32_t m_currentUid;
  uint64_t m_currentTs;
  uint32_t m_currentContext;
  // is set if new event arrived while waiting for the next timeslice
  bool m_newEventArrived;

  mutable SystemMutex m_mutex;
};

} // namespace ns3

#endif /* SYNC_SIMULATOR_IMPL_H */
