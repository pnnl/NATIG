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
 * Slow-DDoS (Slowloris-style) connection-exhaustion bot implementation.
 * See slowloris-bot-application.h for the design rationale.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#include "slowloris-bot-application.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/node.h"

#include <iostream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SlowlorisBotApplication");

// Required so GetTypeId() runs at static-initialization time and
// registers this TypeId by name -- without it,
// SlowlorisBotApplicationHelper's m_factory.SetTypeId("ns3::SlowlorisBotApplication")
// finds nothing registered, and the following m_factory.Set(...) call
// fails ("Invalid attribute set"). Same gotcha ModbusApplicationNew hit
// (see its own NS_OBJECT_ENSURE_REGISTERED comment).
NS_OBJECT_ENSURE_REGISTERED (SlowlorisBotApplication);

TypeId
SlowlorisBotApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SlowlorisBotApplication")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<SlowlorisBotApplication> ()
    .AddAttribute ("RemoteAddress",
                   "The victim's address",
                   AddressValue (),
                   MakeAddressAccessor (&SlowlorisBotApplication::m_remoteAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort",
                   "The victim's listening port",
                   UintegerValue (0),
                   MakeUintegerAccessor (&SlowlorisBotApplication::m_remotePort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ConnectionsPerBot",
                   "Number of TCP connections this bot opens and holds open",
                   UintegerValue (1),
                   MakeUintegerAccessor (&SlowlorisBotApplication::m_connectionsPerBot),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ConnectRate",
                   "Connections opened per second by this bot -- the low, "
                   "steady rate that makes this attack slow rather than a "
                   "same-tick SYN burst",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&SlowlorisBotApplication::m_connectRate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TrickleBytes",
                   "Bytes sent on each held-open socket every "
                   "TrickleInterval seconds. Default 0 (pure silence): no "
                   "server-side idle/read timeout exists on any of "
                   "DNP3/Modbus/MMS today (verified against "
                   "HandlePeerClose/HandlePeerError and the unused "
                   "respTimeout member in all three), so a silent held-open "
                   "socket already exhausts the server's connection list. "
                   "Kept as a forward-compatible knob only.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&SlowlorisBotApplication::m_trickleBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("TrickleInterval",
                   "Seconds between trickle sends (ignored if TrickleBytes is 0)",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&SlowlorisBotApplication::m_trickleInterval),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

SlowlorisBotApplication::SlowlorisBotApplication ()
  : m_connectionsOpened (0),
    m_running (false)
{
  NS_LOG_FUNCTION (this);
}

SlowlorisBotApplication::~SlowlorisBotApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
SlowlorisBotApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socketList.clear ();
  Application::DoDispose ();
}

void
SlowlorisBotApplication::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  m_running = true;
  m_connectionsOpened = 0;
  OpenNextConnection ();
}

void
SlowlorisBotApplication::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  m_running = false;
  Simulator::Cancel (m_openEvent);

  while (!m_socketList.empty ())
    {
      Ptr<Socket> socket = m_socketList.front ();
      m_socketList.pop_front ();
      socket->Close ();
    }
}

void
SlowlorisBotApplication::OpenNextConnection (void)
{
  if (!m_running || m_connectionsOpened >= m_connectionsPerBot)
    {
      return;
    }

  Ptr<Socket> socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());

  if (Ipv6Address::IsMatchingType (m_remoteAddress))
    {
      socket->Bind6 ();
    }
  else
    {
      socket->Bind ();
    }

  socket->SetConnectCallback (
    MakeCallback (&SlowlorisBotApplication::HandleConnectionSucceeded, this),
    MakeCallback (&SlowlorisBotApplication::HandleConnectionFailed, this));

  if (Ipv6Address::IsMatchingType (m_remoteAddress))
    {
      socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom (m_remoteAddress), m_remotePort));
    }
  else
    {
      socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_remoteAddress), m_remotePort));
    }

  m_connectionsOpened++;

  if (m_connectionsOpened < m_connectionsPerBot && m_connectRate > 0.0)
    {
      m_openEvent = Simulator::Schedule (Seconds (1.0 / m_connectRate),
                                          &SlowlorisBotApplication::OpenNextConnection, this);
    }
}

void
SlowlorisBotApplication::HandleConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  m_socketList.push_back (socket);

  if (m_trickleBytes > 0)
    {
      SendTrickle (socket);
    }
}

void
SlowlorisBotApplication::HandleConnectionFailed (Ptr<Socket> socket)
{
  // std::cout, not NS_LOG_*: this build's optimized configuration
  // compiles out NS_LOG_INFO/NS_LOG_WARN (see
  // feedback_ns3_build_environment_gotchas.md), so a real failure here
  // would otherwise be silently invisible.
  std::cout << "SlowlorisBotApplication: connection attempt failed" << std::endl;
}

void
SlowlorisBotApplication::SendTrickle (Ptr<Socket> socket)
{
  if (!m_running)
    {
      return;
    }

  Ptr<Packet> packet = Create<Packet> (m_trickleBytes);
  socket->Send (packet);

  if (m_trickleInterval > 0.0)
    {
      Simulator::Schedule (Seconds (m_trickleInterval),
                            &SlowlorisBotApplication::SendTrickle, this, socket);
    }
}

} // namespace ns3
