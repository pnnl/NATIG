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
#include <iostream>
#include <fstream>
#include <vector>
using namespace std;
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/names.h"
#include "ns3/string.h"
#include "fncs-application.h"
#include "ns3/random-variable-stream.h"

//#ifdef FNCS
#include </rd2c/include/fncs.hpp>
//#endif

#include <algorithm>
#include <sstream>
#include <string>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FncsApplication");

NS_OBJECT_ENSURE_REGISTERED (FncsApplication);

TypeId
FncsApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FncsApplication")
    .SetParent<Application> ()
    .AddConstructor<FncsApplication> ()
    .AddAttribute ("Name", 
                   "The name of the application",
                   StringValue (),
                   MakeStringAccessor (&FncsApplication::m_name),
                   MakeStringChecker ())
    .AddAttribute ("Sent", 
                   "The counter for outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&FncsApplication::m_sent),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LocalAddress", 
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&FncsApplication::m_localAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort", 
                   "The source port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&FncsApplication::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("JitterMinNs",
                   "The source port of the outbound packets",
                   DoubleValue  (1000),
                   MakeDoubleAccessor (&FncsApplication::m_jitterMinNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("JitterMaxNs",
                   "The source port of the outbound packets",
                   DoubleValue (100000),
                   MakeDoubleAccessor (&FncsApplication::m_jitterMaxNs),
                   MakeDoubleChecker<double> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&FncsApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddAttribute ("OutFileName", 
                   "The name of the output file",
                   StringValue (),
                   MakeStringAccessor (&FncsApplication::f_name),
                   MakeStringChecker ())
  ;
  return tid;
}

FncsApplication::FncsApplication ()
{
  NS_LOG_FUNCTION (this);
  m_sent = 0;
  m_socket = 0;
  
  //Setting up jitter random variable stream
  m_rand_delay_ns = CreateObject<UniformRandomVariable> ();
  m_rand_delay_ns->SetAttribute ("Min", DoubleValue  (m_jitterMinNs));
  m_rand_delay_ns->SetAttribute ("Max", DoubleValue  (m_jitterMaxNs));
}

FncsApplication::~FncsApplication()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
}

void
FncsApplication::SetName (const std::string &name)
{
  NS_LOG_FUNCTION (this << name);
  m_name = name;
  Names::Add("fncs_"+name, this);
}

std::string
FncsApplication::GetName (void) const
{
  return m_name;
}

void 
FncsApplication::SetLocal (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void 
FncsApplication::SetLocal (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = Address (ip);
  m_localPort = port;
}

void 
FncsApplication::SetLocal (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = Address (ip);
  m_localPort = port;
}

void
FncsApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void 
FncsApplication::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(m_localAddress) == true)
        {
          m_socket->Bind (InetSocketAddress (Ipv4Address::ConvertFrom(m_localAddress), m_localPort));
        }
      else if (Ipv6Address::IsMatchingType(m_localAddress) == true)
        {
          m_socket->Bind (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_localAddress), m_localPort));
        }
      else
        {
          m_socket->Bind();
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&FncsApplication::HandleRead, this));

  if (m_name.empty()) {
    NS_FATAL_ERROR("FncsApplication is missing name");
  }

  if (f_name.empty())
  {
    NS_LOG_INFO("FncsApplication is missing an output file in CSV format.");
  }
}

void 
FncsApplication::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }
}

static uint8_t
char_to_uint8_t (char c)
{
  return uint8_t(c);
}

// break long topic of structure simname/from@to/topic into its component parts
static std::vector<std::string> splitTopic(std::string topic)
{
  std::vector<std::string> retval;
 
  std::size_t found1 = topic.find_first_of('/');
  std::size_t found2 = topic.find_first_of('/', found1 + 1);
  retval.push_back(topic.substr(0, found1)); // Simulator name
  std::string fromTo = topic.substr(found1 + 1, found2 - found1 - 1); // Source@Destination = SourceLevel_SourceID @ DestinationLevel_DestinationID
  std::size_t found3 = fromTo.find('@');
  // analyzing the source string
  std::string source = fromTo.substr(0, found3); // SourceLevel_SourceID
  std::size_t found4 = source.find_last_of('_');
  std::string sourceLevel = source.substr(0, found4); // Sourcelevel
  std::size_t found5 = sourceLevel.find("Aggregator");
  if (found5 == std::string::npos)
  {
    sourceLevel = "House";
  }
  retval.push_back(sourceLevel);
  retval.push_back(source.substr(found4 + 1, std::string::npos)); // SourceID
  // analyzing the destination string
  std::string destination = fromTo.substr(found3 + 1, std::string::npos); // DestinationLevel_DestinationID
  std::size_t found6 = destination.find_last_of('_');
  std::string destLevel = destination.substr(0, found6); // Destinationlevel
  std::size_t found7 = destLevel.find("Aggregator");
  if (found7 == std::string::npos)
  {
    destLevel = "House";
  }
  retval.push_back(destLevel);
  retval.push_back(destination.substr(found6 + 1, std::string::npos));
  retval.push_back(topic.substr(found2 + 1, std::string::npos)); // small topic name
  
  return retval;
}

void 
FncsApplication::Send (Ptr<FncsApplication> to, std::string topic, std::string value)
{
  NS_LOG_FUNCTION (this << to << topic << value);
 
  Ptr<Packet> p;

  // Convert given value into a Packet.
  size_t total_size = topic.size() + 1 + value.size();
  uint8_t *buffer = new uint8_t[total_size];
  uint8_t *buffer_ptr = buffer;
  std::transform (topic.begin(), topic.end(), buffer_ptr, char_to_uint8_t);
  buffer_ptr += topic.size();
  buffer_ptr[0] = '=';
  buffer_ptr += 1;
  std::transform (value.begin(), value.end(), buffer_ptr, char_to_uint8_t);
  p = Create<Packet> (buffer, total_size);
  NS_LOG_INFO("buffer='" << p << "'");
  delete [] buffer;

  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs,m_jitterMaxNs) + 0.5);

  if (Ipv4Address::IsMatchingType (m_localAddress))
    {
      InetSocketAddress address = to->GetLocalInet();
      if (~f_name.empty())
      {
        std::vector<std::string> topicParts = splitTopic(topic);
        std::ofstream outFile(f_name.c_str(), ios::app);
        outFile << Simulator::Now ().GetNanoSeconds ()  << ","
                << p->GetUid () << ","
                << "s,"
                << total_size << ","
                << topicParts[0] << ","
                << topicParts[1] << ","
                << topicParts[2] << ","
                << topicParts[3] << ","
                << topicParts[4] << ","
                << address.GetIpv4() << ","
                << address.GetPort() << ","
                << topicParts[5] << ","
                << value << endl;
        outFile.close();
      }
      NS_LOG_INFO ("At time '"
          << Simulator::Now ().GetNanoSeconds () + delay_ns
          << "'ns '"
          << m_name
          << "' sent "
          << total_size
          << " bytes to '"
          << to->GetName()
          << "' at address "
          << address.GetIpv4()
          << " port "
          << address.GetPort()
		  << " topic '"
          << topic
		  << "' value '"
          << value 
		  << "' uid '"
		  << p->GetUid () <<"'");
      //m_socket->SendTo(p, 0, address);
      //Simulator::Schedule(NanoSeconds (delay_ns), &Socket::SendTo, m_socket, buffer_ptr, p->GetSize(), 0, address); //non-virtual method
      int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
      Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0, address); //virtual method
    }
  else if (Ipv6Address::IsMatchingType (m_localAddress))
    {
      Inet6SocketAddress address = to->GetLocalInet6();
      if (~f_name.empty())
      {
        std::vector<std::string> topicParts = splitTopic(topic);
        std::ofstream outFile(f_name.c_str(), ios::app);
        outFile << Simulator::Now ().GetNanoSeconds ()  << ","
                << p->GetUid () << ","
                << "s,"
                << total_size << ","
                << topicParts[0] << ","
                << topicParts[1] << ","
                << topicParts[2] << ","
                << topicParts[3] << ","
                << topicParts[4] << ","
                << address.GetIpv6() << ","
                << address.GetPort() << ","
                << topicParts[5] << ","
                << value << endl;
        outFile.close();
      }
      NS_LOG_INFO ("At time '"
          << Simulator::Now ().GetNanoSeconds () + delay_ns
          << "'ns '"
          << m_name
          << "' sent "
          << total_size
          << " bytes to '"
          << to->GetName()
          << "' at address "
          << address.GetIpv6()
          << " port "
          << address.GetPort()
		  << " topic '"
          << topic
		  << "' value '"
          << value 
		  << "' uid '"
		  << p->GetUid () <<"'");
      //m_socket->SendTo(p, 0, address);
      //Simulator::Schedule(NanoSeconds (delay_ns), &Socket::SendTo, m_socket, buffer_ptr, p->GetSize(), 0, address); //non-virtual method
      int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
      Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0, address); //virtual method
    }
  ++m_sent;
}

InetSocketAddress FncsApplication::GetLocalInet (void) const
{
  return InetSocketAddress(Ipv4Address::ConvertFrom(m_localAddress), m_localPort);
}

Inet6SocketAddress FncsApplication::GetLocalInet6 (void) const
{
  return Inet6SocketAddress(Ipv6Address::ConvertFrom(m_localAddress), m_localPort);
}

void
FncsApplication::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      uint32_t size = packet->GetSize();
	  std::ostringstream odata;
      packet->CopyData (&odata, size);
      std::string sdata = odata.str();
      size_t split = sdata.find('=', 0);
      if (std::string::npos == split) {
          NS_FATAL_ERROR("HandleRead could not locate '=' to split topic=value");
      }
      std::string topic = sdata.substr(0, split);
      std::string value = sdata.substr(split+1);
      if (InetSocketAddress::IsMatchingType (from))
        {
          if (~f_name.empty())
          {
            std::vector<std::string> topicParts = splitTopic(topic);
            std::ofstream outFile(f_name.c_str(), ios::app);
            outFile << Simulator::Now ().GetNanoSeconds ()  << ","
                    << packet->GetUid () << ","
                    << "r,"
                    << size << ","
                    << topicParts[0] << ","
                    << topicParts[1] << ","
                    << topicParts[2] << ","
                    << topicParts[3] << ","
                    << topicParts[4] << ","
                    << InetSocketAddress::ConvertFrom (from).GetIpv4 () << ","
                    << InetSocketAddress::ConvertFrom (from).GetPort () << ","
                    << topicParts[5] << ","
                    << value << endl;
            outFile.close();
          }
          NS_LOG_INFO ("At time '"
		          << Simulator::Now ().GetNanoSeconds ()
				  << "'ns '"
				  << m_name
                  << "' received "
                  << size
                  << " bytes at address "
                  << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                  << " port "
                  << InetSocketAddress::ConvertFrom (from).GetPort ()
				  << " topic '"
				  << topic
				  << "' value '"
				  << value
				  << "' uid '"
		          << packet->GetUid () <<"'");
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          if (~f_name.empty())
          {
            std::vector<std::string> topicParts = splitTopic(topic);
            std::ofstream outFile(f_name.c_str(), ios::app);
            outFile << Simulator::Now ().GetNanoSeconds ()  << ","
                    << packet->GetUid () << ","
                    << "r,"
                    << size << ","
                    << topicParts[0] << ","
                    << topicParts[1] << ","
                    << topicParts[2] << ","
                    << topicParts[3] << ","
                    << topicParts[4] << ","
                    << Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << ","
                    << Inet6SocketAddress::ConvertFrom (from).GetPort () << ","
                    << topicParts[5] << ","
                    << value << endl;
            outFile.close();
          }
          NS_LOG_INFO ("At time '"
		          << Simulator::Now ().GetNanoSeconds ()
				  << "'ns '"
				  << m_name
                  << "' received "
                  << size
                  << " bytes at address "
                  << Inet6SocketAddress::ConvertFrom (from).GetIpv6 ()
                  << " port "
                  << Inet6SocketAddress::ConvertFrom (from).GetPort ()
				  << " topic '"
				  << topic
				  << "' value '"
				  << value
				  << "' uid '"
		          << packet->GetUid () <<"'");
        }
      //fncs::fncs Texture;
      fncs::publish(topic, value);
    }
}

} // Namespace ns3
