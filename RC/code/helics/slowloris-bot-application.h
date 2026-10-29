/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2026 Kenneth Watts
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
 * Slow-DDoS (Slowloris-style) connection-exhaustion bot.
 *
 * Deliberately protocol-agnostic and shared across DNP3/Modbus/MMS: unlike
 * those protocols' own Application classes, this bot never parses any
 * protocol framing. It only opens plain TCP sockets against a victim
 * (ip, port) and holds them open, barely/never sending -- exhausting the
 * server's connection-tracking resources rather than its bandwidth (the
 * existing flat-rate DDoS's job). This works against all three TCP
 * protocols for the identical reason: none of their server-side accept
 * paths (ModbusApplicationNew/MmsApplicationNew/Dnp3ApplicationNew
 * HandleAccept) cap the number of accepted sockets or enforce any
 * idle/read timeout, so a held-open connection is never reaped.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#ifndef SLOWLORIS_BOT_APPLICATION_H
#define SLOWLORIS_BOT_APPLICATION_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/address.h"

#include <list>

namespace ns3 {

/**
 * \ingroup applications
 * \brief Slow-DDoS bot: opens ConnectionsPerBot TCP connections to
 * (RemoteAddress, RemotePort) at a fixed low rate (ConnectRate,
 * connections/sec) and holds them open for the lifetime of the
 * application, sending nothing unless TrickleBytes is nonzero.
 */
class SlowlorisBotApplication : public Application
{
public:
  static TypeId GetTypeId (void);

  SlowlorisBotApplication ();
  virtual ~SlowlorisBotApplication ();

protected:
  virtual void DoDispose (void);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /// Opens one more connection if under ConnectionsPerBot, then
  /// self-reschedules at 1/ConnectRate seconds -- the self-rescheduling
  /// loop (rather than a same-tick for loop) is what makes connection
  /// opening itself slow, distinct from a SYN-flood variant of the
  /// existing flat-rate DDoS.
  void OpenNextConnection (void);

  void HandleConnectionSucceeded (Ptr<Socket> socket);
  void HandleConnectionFailed (Ptr<Socket> socket);

  /// No-op unless TrickleBytes > 0 (forward-compatible only -- no
  /// server-side idle timeout exists today to defeat, see file header).
  void SendTrickle (Ptr<Socket> socket);

  Address m_remoteAddress;
  uint16_t m_remotePort;
  uint32_t m_connectionsPerBot;
  double m_connectRate;
  uint32_t m_trickleBytes;
  double m_trickleInterval;

  uint32_t m_connectionsOpened;
  std::list<Ptr<Socket> > m_socketList;
  EventId m_openEvent;
  bool m_running;
};

} // namespace ns3

#endif /* SLOWLORIS_BOT_APPLICATION_H */
