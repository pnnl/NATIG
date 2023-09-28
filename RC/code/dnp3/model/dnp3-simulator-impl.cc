/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: George Riley <riley@ece.gatech.edu>
 *
 */

#include "dnp3-simulator-impl.h"

#include "ns3/simulator.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/channel.h"
#include "ns3/node-container.h"
#include "ns3/ptr.h"
#include "ns3/pointer.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/names.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef FNCS
#include </rd2c/include/fncs.hpp>
#include <exception>
typedef std::vector<std::pair<std::string,std::string> > match_list_t;
#endif

#include "ns3/dnp3-application.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Dnp3SimulatorImpl");

NS_OBJECT_ENSURE_REGISTERED (Dnp3SimulatorImpl);

TypeId
Dnp3SimulatorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Dnp3SimulatorImpl")
    .SetParent<Object> ()
    .AddConstructor<Dnp3SimulatorImpl> ()
  ;
  return tid;
}

#ifdef FNCS
static void Dnp3_die()
{
  fncs::die();
}
#endif

Dnp3SimulatorImpl::Dnp3SimulatorImpl ()
{
  NS_LOG_FUNCTION (this);

NS_LOG_LOGIC ("Dnp3SimulatorImpl");

#ifdef FNCS
  fncs::initialize();

  m_grantedTime = Seconds (0);

  std::set_terminate (Dnp3_die);
#else
  NS_FATAL_ERROR ("Can't use Dnp3 simulator without Fncs compiled in");
#endif

  m_stop = false;
  m_globalFinished = false;
  // uids are allocated from 4.
  // uid 0 is "invalid" events
  // uid 1 is "now" events
  // uid 2 is "destroy" events
  m_uid = 4;
  // before ::Run is entered, the m_currentUid will be zero
  m_currentUid = 0;
  m_currentTs = 0;
  m_currentContext = 0xffffffff;
  m_unscheduledEvents = 0;
  m_events = 0;
 NS_LOG_LOGIC ("Dnp3SimulatorImpl end");
}

Dnp3SimulatorImpl::~Dnp3SimulatorImpl ()
{
  NS_LOG_FUNCTION (this);
}

void
Dnp3SimulatorImpl::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  while (!m_events->IsEmpty ())
    {
      Scheduler::Event next = m_events->RemoveNext ();
      next.impl->Unref ();
    }
  m_events = 0;
  SimulatorImpl::DoDispose ();
}

void
Dnp3SimulatorImpl::Destroy ()
{
  NS_LOG_FUNCTION (this);

  while (!m_destroyEvents.empty ())
    {
      Ptr<EventImpl> ev = m_destroyEvents.front ().PeekEventImpl ();
      m_destroyEvents.pop_front ();
      NS_LOG_LOGIC ("handle destroy " << ev);
      if (!ev->IsCancelled ())
        {
          ev->Invoke ();
        }
    }
}

void
Dnp3SimulatorImpl::SetScheduler (ObjectFactory schedulerFactory)
{
  NS_LOG_FUNCTION (this << schedulerFactory);

  Ptr<Scheduler> scheduler = schedulerFactory.Create<Scheduler> ();

  if (m_events != 0)
    {
      while (!m_events->IsEmpty ())
        {
          Scheduler::Event next = m_events->RemoveNext ();
          scheduler->Insert (next);
        }
    }
  m_events = scheduler;
}

void
Dnp3SimulatorImpl::ProcessOneEvent (void)
{
  NS_LOG_FUNCTION (this);

  Scheduler::Event next = m_events->RemoveNext ();

  NS_ASSERT (next.key.m_ts >= m_currentTs);
  m_unscheduledEvents--;

  NS_LOG_LOGIC ("handle " << next.key.m_ts);
  m_currentTs = next.key.m_ts;
  m_currentContext = next.key.m_context;
  m_currentUid = next.key.m_uid;
  next.impl->Invoke ();
  next.impl->Unref ();
}

bool
Dnp3SimulatorImpl::IsFinished (void) const
{
  NS_LOG_FUNCTION (this);

  return m_globalFinished;
}

bool
Dnp3SimulatorImpl::IsLocalFinished (void) const
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("m_events->IsEmpty " << m_events->IsEmpty () << " m_stop " << m_stop);
  return m_events->IsEmpty () || m_stop;
}

uint64_t
Dnp3SimulatorImpl::NextTs (void) const
{
  NS_LOG_FUNCTION (this);

  // If local Dnp3 task is has no more events or stop was called
  // next event time is infinity.
  if (IsLocalFinished ())
    {
      return GetMaximumSimulationTime ().GetTimeStep ();
    }
  else
    {
      Scheduler::Event ev = m_events->PeekNext ();
      return ev.key.m_ts;
    }
}

Time
Dnp3SimulatorImpl::Next (void) const
{
  NS_LOG_FUNCTION (this);

  return TimeStep (NextTs ());
}

static std::vector<std::string>
split(std::string str, char delimiter) {
  std::vector<std::string> retval;
  std::stringstream ss(str);
  std::string tok;
  while (std::getline(ss, tok, delimiter)) {
    retval.push_back(tok);
  }
  return retval;
}

void
Dnp3SimulatorImpl::Run (void)
{
  NS_LOG_FUNCTION (this);

#ifdef FNCS
  m_stop = false;
  while (!m_globalFinished)
    {
      Time nextTime = Next ();

      // If local event is beyond grantedTime then need to request
      // new time.
      //NS_LOG_INFO("Dnp3SimulatorImpl-Run");
      NS_LOG_LOGIC ("nextTime " << nextTime << " m_grantedTime " << m_grantedTime);
      if (nextTime > m_grantedTime || IsLocalFinished () )
        {
          fncs::time requested = static_cast<fncs::time> (NextTs ());
          NS_LOG_LOGIC ("requested " << requested);
          fncs::time granted = fncs::time_request(requested);
          NS_LOG_LOGIC ("granted " << granted);
          uint64_t grantedTs = static_cast<uint64_t> (granted);
          m_grantedTime = TimeStep (grantedTs);
          NS_LOG_LOGIC ("m_grantedTime " << m_grantedTime);
          m_currentTs = grantedTs;
          if (m_grantedTime.GetTimeStep() == GetMaximumSimulationTime ().GetTimeStep()) {
            Stop();
          }
          else {
            //NS_LOG_INFO("Dnp3SimulatorImpl-Run found data");
            // Check for Dnp3 messages.
            vector<std::string> events = fncs::get_events();
            for (vector<std::string>::iterator it=events.begin();
                it!=events.end(); ++it) {
              std::string topic = *it;
              NS_LOG_INFO("FNCS variable: " << topic);
              std::vector<std::string> parts = split(topic, '/');
              int slash_count = parts.size()-1;
              //cout << "Simulator received topic : " << topic << "slash count: " << slash_count << endl;
              if (1 == slash_count) {
                // We have 'simname/topic'.
                std::string value = fncs::get_value(*it);
                std::string simname = "Dnp3_" + parts[0];  
                std::string point = parts[1];
                // Locate the Dnp3Application instances with the same names.
                Ptr<Dnp3Application> dnp3SS =
                  Names::Find<Dnp3Application>(simname);

                if (!dnp3SS) {
                    //FATAL_ERROR("failed Dnp3Application lookup from ");
                    NS_LOG_INFO("Failed Dnp3Application lookup " << simname);
                    cout << "failed Dnp3Application lookup from" << endl;
                } else {
                    //NS_LOG_INFO("Storing point  " << point << " object" << simname << " value" << value);
                    dnp3SS->Store(point, value);
                }
              }
              else {
                NS_LOG_INFO("ignoring topic '" << topic);
              }
            }
          }
        }


      // Execute next event if it is within the current time window.
      // Local task may be completed.
      if ( (nextTime <= m_grantedTime) && (!IsLocalFinished ()) )
        { // Safe to process
          ProcessOneEvent ();
        }
    }

  // If the simulator stopped naturally by lack of events, make a
  // consistency test to check that we didn't lose any events along the way.
  NS_ASSERT (!m_events->IsEmpty () || m_unscheduledEvents == 0);
#else
  NS_FATAL_ERROR ("Can't use Dnp3 simulator without Dnp3 compiled in");
#endif
}

void
Dnp3SimulatorImpl::Stop (void)
{
  NS_LOG_FUNCTION (this);

#ifdef FNCS
  fncs::finalize();
#else
  NS_FATAL_ERROR ("Can't use Dnp3 simulator without Dnp3 compiled in");
#endif

  m_globalFinished = true;

  m_stop = true;
}

void
Dnp3SimulatorImpl::Stop (Time const &time)
{
  NS_LOG_FUNCTION (this << time.GetTimeStep ());

  Simulator::Schedule (time, &Simulator::Stop);
}

//
// Schedule an event for a _relative_ time in the future.
//
EventId
Dnp3SimulatorImpl::Schedule (Time const &time, EventImpl *event)
{
  NS_LOG_FUNCTION (this << time.GetTimeStep () << event);

  Time tAbsolute = time + TimeStep (m_currentTs);

  NS_ASSERT (tAbsolute.IsPositive ());
  NS_ASSERT (tAbsolute >= TimeStep (m_currentTs));
  NS_LOG_LOGIC ("Scheduled for " << tAbsolute);
  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = static_cast<uint64_t> (tAbsolute.GetTimeStep ());
  ev.key.m_context = GetContext ();
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

void
Dnp3SimulatorImpl::ScheduleWithContext (uint32_t context, Time const &time, EventImpl *event)
{
  NS_LOG_FUNCTION (this << context << time.GetTimeStep () << m_currentTs << event);

  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = m_currentTs + time.GetTimeStep ();
  ev.key.m_context = context;
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
}

EventId
Dnp3SimulatorImpl::ScheduleNow (EventImpl *event)
{
  NS_LOG_FUNCTION (this << event);

  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = m_currentTs;
  ev.key.m_context = GetContext ();
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

EventId
Dnp3SimulatorImpl::ScheduleDestroy (EventImpl *event)
{
  NS_LOG_FUNCTION (this << event);

  EventId id (Ptr<EventImpl> (event, false), m_currentTs, 0xffffffff, 2);
  m_destroyEvents.push_back (id);
  m_uid++;
  return id;
}

Time
Dnp3SimulatorImpl::Now (void) const
{
  return TimeStep (m_currentTs);
}

Time
Dnp3SimulatorImpl::GetDelayLeft (const EventId &id) const
{
  if (IsExpired (id))
    {
      return TimeStep (0);
    }
  else
    {
      return TimeStep (id.GetTs () - m_currentTs);
    }
}

void
Dnp3SimulatorImpl::Remove (const EventId &id)
{
  if (id.GetUid () == 2)
    {
      // destroy events.
      for (DestroyEvents::iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
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
  Scheduler::Event event;
  event.impl = id.PeekEventImpl ();
  event.key.m_ts = id.GetTs ();
  event.key.m_context = id.GetContext ();
  event.key.m_uid = id.GetUid ();
  m_events->Remove (event);
  event.impl->Cancel ();
  // whenever we remove an event from the event list, we have to unref it.
  event.impl->Unref ();

  m_unscheduledEvents--;
}

void
Dnp3SimulatorImpl::Cancel (const EventId &id)
{
  if (!IsExpired (id))
    {
      id.PeekEventImpl ()->Cancel ();
    }
}

bool
Dnp3SimulatorImpl::IsExpired (const EventId &id) const
{
  if (id.GetUid () == 2)
    {
      if (id.PeekEventImpl () == 0
          || id.PeekEventImpl ()->IsCancelled ())
        {
          return true;
        }
      // destroy events.
      for (DestroyEvents::const_iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
        {
          if (*i == id)
            {
              return false;
            }
        }
      return true;
    }
  if (id.PeekEventImpl () == 0
      || id.GetTs () < m_currentTs
      || (id.GetTs () == m_currentTs
          && id.GetUid () <= m_currentUid)
      || id.PeekEventImpl ()->IsCancelled ())
    {
      return true;
    }
  else
    {
      return false;
    }
}

Time
Dnp3SimulatorImpl::GetMaximumSimulationTime (void) const
{
  /// \todo I am fairly certain other compilers use other non-standard
  /// post-fixes to indicate 64 bit constants.
  return TimeStep (0x7fffffffffffffffLL);
}

uint32_t
Dnp3SimulatorImpl::GetSystemId (void) const
{
#ifdef FNCS
  return fncs::get_id();
#else
  return 0;
#endif
}

uint32_t
Dnp3SimulatorImpl::GetContext (void) const
{
  return m_currentContext;
}

uint64_t Dnp3SimulatorImpl::GetEventCount (void) const 
{
    return 0;
}

} // namespace ns3
