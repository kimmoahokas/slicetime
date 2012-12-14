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

// this file is adapted from simulator/realtime-simulator-impl.cc

#include "ns3/simulator.h"
#include "ns3/sync-simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"

#include "ns3/ptr.h"
#include "ns3/pointer.h"
#include "ns3/assert.h"
#include "ns3/fatal-error.h"
#include "ns3/log.h"


#include <math.h>
#include <sys/time.h>
#include <time.h>

NS_LOG_COMPONENT_DEFINE ("SyncSimulatorImpl");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SyncSimulatorImpl);

TypeId
SyncSimulatorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SyncSimulatorImpl")
    .SetParent<Object> ()
    .AddConstructor<SyncSimulatorImpl> ()
    ;
  return tid;
}


SyncSimulatorImpl::SyncSimulatorImpl ()
{
  NS_LOG_FUNCTION_NOARGS ();

  m_stop = false;
  m_running = false;
  // uids are allocated from 4.
  // uid 0 is "invalid" events
  // uid 1 is "now" events
  // uid 2 is "destroy" events
  m_uid = 4; 
  // before ::Run is entered, the m_currentUid will be zero
  m_currentUid = 0;
  m_currentTs = 0;
  m_unscheduledEvents = 0;
 
  // will be set if events are scheduled while waiting for a runtime permission
  m_newEventArrived = false;

  // create a sync client for communcation with the server
  m_syncClient = CreateObject<SyncClient> ();
}

SyncSimulatorImpl::~SyncSimulatorImpl ()
{}

void 
SyncSimulatorImpl::DoDispose (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  while (m_events->IsEmpty () == false)
    {
      Scheduler::Event next = m_events->RemoveNext ();
      next.impl->Unref ();
    }
  m_events = 0;

  SimulatorImpl::DoDispose();
}

void
SyncSimulatorImpl::Destroy ()
{
  NS_LOG_FUNCTION_NOARGS ();

  //
  // This function is only called with the private version "disconnected" from
  // the main simulator functions.  We rely on the user not calling 
  // Simulator::Destroy while there is a chance that a worker thread could be
  // accessing the current instance of the private object.  In practice this
  // means shutting down the workers and doing a Join() before calling the
  // Simulator::Destroy().
  //
  while (m_destroyEvents.empty () == false) 
    {
      Ptr<EventImpl> ev = m_destroyEvents.front ().PeekEventImpl ();
      m_destroyEvents.pop_front ();
      NS_LOG_LOGIC ("handle destroy " << ev);
      if (ev->IsCancelled () == false)
        {
          ev->Invoke ();
        }
    }
}

void
SyncSimulatorImpl::SetScheduler (ObjectFactory schedulerFactory)
{
  NS_LOG_FUNCTION_NOARGS ();

  Ptr<Scheduler> scheduler = schedulerFactory.Create<Scheduler> ();

  { 
    CriticalSection cs (m_mutex);

    if (m_events != 0)
      {
        while (m_events->IsEmpty () == false)
          {
            Scheduler::Event next = m_events->RemoveNext ();
            scheduler->Insert (next);
          }
      }
    m_events = scheduler;
  }
}

void
SyncSimulatorImpl::ProcessOneEvent (void)
{
  NS_LOG_FUNCTION_NOARGS ();


  // this label is needed to jump here in case a packet was received while waiting for a run permission
  // (goto is not nice, but breaking out of nested loops is also quite confusing)
  beginningOfProcessOneEvent:
  
  //usleep(10000); a very generic slowdown to show e.g simulation overload. don't uncomment //elias
  

  NS_LOG_LOGIC ("Checking if next event is within the current timeslice");

  // will be used to save the runtime of the last assigned timeslice
  uint32_t runTime = 0;

  // will be set to true if a new event was scheduled while waiting
  // but is only checked after receiving a run permission
  m_newEventArrived = false;

  // get timestamp of the next event we want to process
  uint64_t tsNext = (TimeStep (NextTs ())).GetNanoSeconds();

  // send finish packets and wait for runpermissions until we reach the timeslice 
  //   where we are allowed to execute the next event
  while (tsNext >= m_barrierTime)
    {
      m_isWaitingForPermission = true;

      // send finished packet
      // (except in the first round, because the server always sends a run permission
      //  after receiving the register packet)
      if (m_firstRound)
        {
          NS_LOG_LOGIC("Not sending finish packet since it is the first round");
          m_firstRound=false;
        }
      else
        {
          // calculate how much time this timeslice took
          uint32_t realTime = 0;
          #ifdef SEND_REALTIME
          struct timeval curTimeval;
          gettimeofday (&curTimeval, NULL);
          realTime = (curTimeval.tv_sec - lastTimeval.tv_sec) * 1000000 + (curTimeval.tv_usec - lastTimeval.tv_usec);
          #endif

          // send the packet
          NS_LOG_LOGIC ("Sending finish packet");
          m_syncClient->SendFinished (runTime, realTime);
        }

      // wait for run permission
      NS_LOG_LOGIC("Waiting for run permission");
      runTime = m_syncClient->WaitForRunPermission();

      // update lastTimeval
      #ifdef SEND_REALTIME
      gettimeofday (&lastTimeval, NULL);
      #endif

      // increase the barrier time
      // (runTime is in microseconds, but m_barrierTime is nanoseconds)
      m_barrierTime += runTime*1000;

      // if a new event arrived stop waiting 
      if (m_newEventArrived)
        {
          NS_LOG_LOGIC ("New event arrived while waiting ...");
          // start from the beginning of the function because the received packet might 
          // be scheduled before the event we are waiting for
          goto beginningOfProcessOneEvent; 
        }
    }
    m_isWaitingForPermission = false;

  NS_LOG_LOGIC("Ready to execute the next event");

  // ok, we are ready to execute the next event
  Scheduler::Event next;

  { 
    CriticalSection cs (m_mutex);

    // 
    // We do know we're waiting for an event, so there had better be an event on the 
    // event queue.  Let's pull it off.  When we release the critical section, the
    // event we're working on won't be on the list and so subsequent operations won't
    // mess with us.
    //
    NS_ASSERT_MSG (m_events->IsEmpty () == false, 
      "SyncSimulatorImpl::ProcessOneEvent(): event queue is empty");
    next = m_events->RemoveNext ();
    m_unscheduledEvents--;

    //
    // We cannot make any assumption that "next" is the same event we originally waited 
    // for.  We can only assume that only that it must be due and cannot cause time 
    // to move backward.
    //
    NS_ASSERT_MSG (next.key.m_ts >= m_currentTs,
                   "SyncSimulatorImpl::ProcessOneEvent(): "
                   "next.GetTs() earlier than m_currentTs (list order error)");
    NS_LOG_LOGIC ("handle " << next.key.m_ts);

    // 
    // Update the current simulation time to be the timestamp of the event we're 
    // executing.  From the rest of the simulation's point of view, simulation time
    // is frozen until the next event is executed.
    //
    m_currentTs = next.key.m_ts;
    m_currentContext = next.key.m_context;
    m_currentUid = next.key.m_uid;
  }

  //
  // We have got the event we're about to execute completely disentangled from the 
  // event list so we can execute it outside a critical section without fear of someone
  // changing things out from under us.

  EventImpl *event = next.impl;
  event->Invoke ();
  event->Unref ();

  NS_LOG_LOGIC("Executed and deleted that event");
}

bool 
SyncSimulatorImpl::IsFinished (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  bool rc;
  {
    CriticalSection cs (m_mutex);
    rc = m_events->IsEmpty () || m_stop;
  }

  return rc;
}

//
// Peeks into event list.  Should be called with critical section locked.
//
uint64_t
SyncSimulatorImpl::NextTs (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_ASSERT_MSG (m_events->IsEmpty () == false, 
    "SyncSimulatorImpl::NextTs(): event queue is empty");
  Scheduler::Event ev = m_events->PeekNext ();
  return ev.key.m_ts;
}

//
// Calls NextTs().  Should be called with critical section locked.
//
Time
SyncSimulatorImpl::Next (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return TimeStep (NextTs ());
}

void
SyncSimulatorImpl::Run (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_LOG_LOGIC("Registering at synchronization server");
  // connect to sync server
  m_syncClient->ConnectAndSendRegister();
  m_barrierTime = 0;
  m_isWaitingForPermission = true;
  m_firstRound = true;

  NS_ASSERT_MSG (m_running == false, 
                 "SyncSimulatorImpl::Run(): Simulator already running");

  m_stop = false;
  m_running = true;

  for (;;) 
    {
      bool done = false;

      {
        CriticalSection cs (m_mutex);
        //
        // In all cases we stop when the event list is empty.  If you are doing a 
        // realtime simulation and you want it to extend out for some time, you must
        // call StopAt.  In the realtime case, this will stick a placeholder event out
        // at the end of time.
        //
        if (m_stop || m_events->IsEmpty ())
          {
            done = true;
          }
      }

      if (done)
        {
          NS_LOG_LOGIC("done with processing events");
          break;
        }

      NS_LOG_LOGIC("About to process the next event");
      ProcessOneEvent ();
    }

  //
  // If the simulator stopped naturally by lack of events, make a
  // consistency test to check that we didn't lose any events along the way.
  //
  {
    CriticalSection cs (m_mutex);

    NS_ASSERT_MSG (m_events->IsEmpty () == false || m_unscheduledEvents == 0,
      "SyncSimulatorImpl::Run(): Empty queue and unprocessed events");
  }

  NS_LOG_LOGIC("Unregistering at synchronization server");
  // disconnect from sync server
  m_syncClient->SendUnregAndDisconnect();


  m_running = false;
}

bool
SyncSimulatorImpl::Running (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_running;
}

//
// This will run the first event on the queue without considering any realtime
// synchronization.  It's mainly implemented to allow simulations requiring
// the multithreaded ScheduleInCurrentSlice() functions the possibility of driving
// the simulation from their own event loop.
//
// It is expected that if there are any realtime requirements, the responsibility
// for synchronizing with real time in an external event loop will be picked up
// by that loop.  For example, they may call Simulator::Next() to find the 
// execution time of the next event and wait for that time somehow -- then call
// RunOneEvent to fire the event.
// 
void
SyncSimulatorImpl::RunOneEvent (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT_MSG (m_running == false, 
                 "SyncSimulatorImpl::RunOneEvent(): An internal simulator event loop is running");

  EventImpl *event = 0;

  //
  // Run this in a critical section in case there's another thread around that
  // may be inserting things onto the event list.
  //
  {
    CriticalSection cs (m_mutex);

    Scheduler::Event next = m_events->RemoveNext ();

    NS_ASSERT (next.key.m_ts >= m_currentTs);
    m_unscheduledEvents--;

    NS_LOG_LOGIC ("handle " << next.key.m_ts);
    m_currentTs = next.key.m_ts;
    m_currentContext = next.key.m_context;
    m_currentUid = next.key.m_ts;
    event = next.impl;
  }
  event->Invoke ();
  event->Unref ();
}

void 
SyncSimulatorImpl::Stop (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_stop = true;
}

void 
SyncSimulatorImpl::Stop (Time const &time)
{
  Simulator::Schedule (time, &Simulator::Stop);
}

//
// Schedule an event for a _relative_ time in the future.
//
EventId
SyncSimulatorImpl::Schedule (Time const &time, EventImpl *impl)
{
  NS_LOG_FUNCTION (time << impl);

  Scheduler::Event ev;
  {
    CriticalSection cs (m_mutex);
    //
    // This is the reason we had to bring the absolute time calcualtion in from the
    // simulator.h into the implementation.  Since the implementations may be 
    // multi-threaded, we need this calculation to be atomic.  You can see it is
    // here since we are running in a CriticalSection.
    //
    Time tAbsolute = Simulator::Now () + time;
    NS_ASSERT_MSG (tAbsolute.IsPositive (), "SyncSimulatorImpl::Schedule(): Negative time");
    NS_ASSERT_MSG (tAbsolute >= TimeStep (m_currentTs), "SyncSimulatorImpl::Schedule(): time < m_currentTs");
    ev.impl = impl;
    ev.key.m_ts = (uint64_t) tAbsolute.GetTimeStep ();
    ev.key.m_context = GetContext();
    ev.key.m_uid = m_uid;
    m_uid++;
    m_unscheduledEvents++;
    m_events->Insert (ev);
    m_newEventArrived = true;
  }

  return EventId (impl, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

void
SyncSimulatorImpl::ScheduleWithContext (uint32_t context, Time const &time, EventImpl *impl)
{
  NS_LOG_FUNCTION (context << time << impl);

  {
    CriticalSection cs (m_mutex);
    uint64_t ts;

    ts = m_currentTs + time.GetTimeStep ();
    NS_ASSERT_MSG (ts >= m_currentTs, "SyncSimulatorImpl::ScheduleWithContext(): schedule for time < m_currentTs");
    Scheduler::Event ev;
    ev.impl = impl;
    ev.key.m_ts = ts;
    ev.key.m_context = context;
    ev.key.m_uid = m_uid;
    m_uid++;
    m_unscheduledEvents++;
    m_events->Insert (ev);
    m_newEventArrived = true;
  }
}

EventId
SyncSimulatorImpl::ScheduleNow (EventImpl *impl)
{
  NS_LOG_FUNCTION (impl);
  Scheduler::Event ev;
  {
    CriticalSection cs (m_mutex);

    ev.impl = impl;
    ev.key.m_ts = m_currentTs;
    ev.key.m_context = GetContext();
    ev.key.m_uid = m_uid;
    m_uid++;
    m_unscheduledEvents++;
    m_events->Insert (ev);
    m_newEventArrived = true;
  }

  return EventId (impl, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

void
SyncSimulatorImpl::ScheduleInCurrentSliceWithContext (uint32_t context, EventImpl *impl)
{
  NS_LOG_FUNCTION (context << impl);
  Scheduler::Event ev;
  {
    CriticalSection cs (m_mutex);

    if (m_isWaitingForPermission)
      ev.key.m_ts = m_barrierTime; // schedule at beginning of timeslice we are waiting for
    else
      ev.key.m_ts = m_barrierTime - 1; // schedule at the end of the timeslice we are currently running

    NS_LOG_LOGIC ("Scheduling event in current timeslice (event is at " << ev.key.m_ts << ", barrier time is " << m_barrierTime << ")");

    ev.impl = impl;
    ev.key.m_uid = m_uid;
    m_uid++;
    m_unscheduledEvents++;
    m_events->Insert (ev);
    m_newEventArrived = true;
  }
}

void
SyncSimulatorImpl::ScheduleInCurrentSlice (EventImpl *impl)
{
  NS_LOG_FUNCTION(impl);
  ScheduleInCurrentSliceWithContext(GetContext (), impl);
}

Time
SyncSimulatorImpl::Now (void) const
{
  return TimeStep (m_currentTs);
}

EventId
SyncSimulatorImpl::ScheduleDestroy (EventImpl *impl)
{
  NS_LOG_FUNCTION_NOARGS ();

  EventId id;
  {
    CriticalSection cs (m_mutex);

    //
    // Time doesn't really matter here (especially in realtime mode).  It is 
    // overridden by the uid of 2 which identifies this as an event to be 
    // executed at Simulator::Destroy time.
    //
    id = EventId (Ptr<EventImpl> (impl, false), m_currentTs, 0xffffffff, 2);
    m_destroyEvents.push_back (id);
    m_uid++;
  }

  return id;
}

Time 
SyncSimulatorImpl::GetDelayLeft (const EventId &id) const
{
  //
  // If the event has expired, there is no delay until it runs.  It is not the
  // case that there is a negative time until it runs.
  //
  if (IsExpired (id))
    {
      return TimeStep (0);
    }

  return TimeStep (id.GetTs () - m_currentTs);
}

void
SyncSimulatorImpl::Remove (const EventId &id)
{
  if (id.GetUid () == 2)
    {
      // destroy events.
      for (DestroyEvents::iterator i = m_destroyEvents.begin (); 
           i != m_destroyEvents.end (); 
           i++)
        {
          if (*i == id)
            {
              m_destroyEvents.erase (i);
              break;
            }
         }
      return;
    }
  if (IsExpired (id))
    {
      return;
    }

  {
    CriticalSection cs (m_mutex);

    Scheduler::Event event;
    event.impl = id.PeekEventImpl ();
    event.key.m_ts = id.GetTs ();
    event.key.m_context = id.GetContext ();
    event.key.m_uid = id.GetUid ();
    
    m_events->Remove (event);
    m_unscheduledEvents++;
    event.impl->Cancel ();
    event.impl->Unref ();
  }
}

void
SyncSimulatorImpl::Cancel (const EventId &id)
{
  if (IsExpired (id) == false)
    {
      id.PeekEventImpl ()->Cancel ();
    }
}

bool
SyncSimulatorImpl::IsExpired (const EventId &ev) const
{
  if (ev.GetUid () == 2)
    {
      if (ev.PeekEventImpl () == 0 ||
          ev.PeekEventImpl ()->IsCancelled ())
        {
          return true;
        }
      // destroy events.
      for (DestroyEvents::const_iterator i = m_destroyEvents.begin (); 
           i != m_destroyEvents.end (); i++)
        {
          if (*i == ev)
            {
              return false;
            }
         }
      return true;
    }

  //
  // If the time of the event is less than the current timestamp of the 
  // simulator, the simulator has gone past the invocation time of the 
  // event, so the statement ev.GetTs () < m_currentTs does mean that 
  // the event has been fired even in realtime mode.
  //
  // The same is true for the next line involving the m_currentUid.
  //
  if (ev.PeekEventImpl () == 0 ||
      ev.GetTs () < m_currentTs ||
      (ev.GetTs () == m_currentTs && ev.GetUid () <= m_currentUid) ||
      ev.PeekEventImpl ()->IsCancelled ()) 
    {
      return true;
    }
  else
    {
      return false;
    }
}

Time 
SyncSimulatorImpl::GetMaximumSimulationTime (void) const
{
  // XXX: I am fairly certain other compilers use other non-standard
  // post-fixes to indicate 64 bit constants.
  return TimeStep (0x7fffffffffffffffLL);
}

uint32_t
SyncSimulatorImpl::GetContext (void) const
{
  return m_currentContext;
}

uint32_t
SyncSimulatorImpl::GetSystemId () const
{
  return m_currentSystemId;
}

}; // namespace ns3
