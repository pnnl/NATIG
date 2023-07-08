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

#ifndef NS3_Dnp3_SIMULATOR_IMPL_H
#define NS3_Dnp3_SIMULATOR_IMPL_H

#include "ns3/simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/ptr.h"
#include "ns3/dnp3-application.h"
#include <list>

namespace ns3 {

/**
 * \ingroup simulator
 * \ingroup Dnp3
 *
 * \brief Dnp3 simulator implementation
 */
class Dnp3SimulatorImpl : public SimulatorImpl
{
public:
  static TypeId GetTypeId (void);

  Dnp3SimulatorImpl ();
  ~Dnp3SimulatorImpl ();

  // virtual from SimulatorImpl
  virtual void Destroy ();
  virtual bool IsFinished (void) const;
  virtual void Stop (void);
  virtual void Stop (Time const &time);
  virtual EventId Schedule (Time const &time, EventImpl *event);
  virtual void ScheduleWithContext (uint32_t context, Time const &time, EventImpl *event);
  virtual EventId ScheduleNow (EventImpl *event);
  virtual EventId ScheduleDestroy (EventImpl *event);
  virtual void Remove (const EventId &id);
  virtual void Cancel (const EventId &id);
  virtual bool IsExpired (const EventId &id) const;
  virtual void Run (void);
  virtual Time Now (void) const;
  virtual Time GetDelayLeft (const EventId &id) const;
  virtual Time GetMaximumSimulationTime (void) const;
  virtual void SetScheduler (ObjectFactory schedulerFactory);
  virtual uint32_t GetSystemId (void) const;
  virtual uint32_t GetContext (void) const;
  virtual uint64_t GetEventCount (void) const;

private:
  virtual void DoDispose (void);
  bool IsLocalFinished (void) const;

  void ProcessOneEvent (void);
  uint64_t NextTs (void) const;
  Time Next (void) const;
  typedef std::list<EventId> DestroyEvents;

  DestroyEvents m_destroyEvents;
  bool m_stop;
  bool m_globalFinished;     // Are all parallel instances completed.
  Ptr<Scheduler> m_events;
  uint32_t m_uid;
  uint32_t m_currentUid;
  uint64_t m_currentTs;
  uint32_t m_currentContext;
  // number of events that have been inserted but not yet scheduled,
  // not counting the "destroy" events; this is used for validation
  int m_unscheduledEvents;

  Time m_grantedTime; // Last LBTS

};

} // namespace ns3

#endif /* NS3_Dnp3_SIMULATOR_IMPL_H */
