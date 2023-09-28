/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4-l3-protocol-MIM.h"
#include "mim-server.h"
//#include "ns3/global-variables.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MIMServerApplication");

NS_OBJECT_ENSURE_REGISTERED (MIMServer);

TypeId
MIMServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MIMServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MIMServer> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&MIMServer::m_port),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

MIMServer::MIMServer ()
{
  NS_LOG_FUNCTION (this);
}

MIMServer::~MIMServer()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socket6 = 0;
}

void
MIMServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
MIMServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  if (m_socket6 == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket6 = Socket::CreateSocket (GetNode (), tid);
      Inet6SocketAddress local6 = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
      m_socket6->Bind (local6);
      if (addressUtils::IsMulticast (local6))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket6);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, local6);
            }
          else
            {
              NS_FATAL_ERROR ("Error: Failed to join multicast group");
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&MIMServer::HandleRead, this));
  m_socket6->SetRecvCallback (MakeCallback (&MIMServer::HandleRead, this));
}

void
MIMServer::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_socket6 != 0)
    {
      m_socket6->Close ();
      m_socket6->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
MIMServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("MIMServer::HandleRead >>> At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("MIMServer::HandleRead >>> At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }

      //Ptr<Ipv4L3ProtocolMIM> ipv4L3Protocol = ipv4->GetObject<Ipv4L3ProtocolMIM> ();
      Ptr<Packet> copy = packet->Copy();
      Ipv4Header ipHeader;
      copy->RemoveHeader(ipHeader);
      Ipv4Address destAddr = ipHeader.GetDestination(); 
      NS_LOG_INFO ("MIMServer::HandleRead >>> destAddr: " << destAddr);

      packet->RemoveAllPacketTags ();
      packet->RemoveAllByteTags ();

      /*if(mitm_flag == false) {
    	  NS_LOG_INFO ("=============================================================");
    	  NS_LOG_INFO ("==================Man in the middle mode=====================");
    	  NS_LOG_INFO ("1. Disconnect the routing process");
    	  NS_LOG_INFO ("2. Send packet with payload '0' to the destination");
    	  NS_LOG_INFO ("=============================================================");

    	  int select = 0;
    	  std::cin >> select;

    	  if(select == 1) {
    		  NS_LOG_INFO ("MIMServer::HandleRead >>> Routing process is terminated by Man in the middle");
    	  } else if(select == 2) {
    		  NS_LOG_INFO ("MIMServer::HandleRead >>> Forwarding packet");
    		  Ptr<Packet> packet_mitm = Create<Packet> (777);

    		  socket->SendTo (packet_mitm, 0, InetSocketAddress(destAddr, InetSocketAddress::ConvertFrom (from).GetPort ()));

    		  if (InetSocketAddress::IsMatchingType (from)) {
    			  NS_LOG_INFO ("MIMServer::HandleRead >>> At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
    					  destAddr << " port " << InetSocketAddress::ConvertFrom (from).GetPort ());
    		  }
    		  else if (Inet6SocketAddress::IsMatchingType (from))
    		  {
    			  NS_LOG_INFO ("MIMServer::HandleRead >>> At time " << Simulator::Now ().GetSeconds () << "s server sent " << packet->GetSize () << " bytes to " <<
    					  destAddr << " port " << Inet6SocketAddress::ConvertFrom (from).GetPort ());
    		  }
    	  } else {
    		  NS_LOG_INFO ("You input a wrong value.");
    	  }
      } else {
    	  NS_LOG_INFO ("MIMServer::HandleRead >>> Man in the middle node is the final destination.");
      }*/
    }
}
} // Namespace ns3
