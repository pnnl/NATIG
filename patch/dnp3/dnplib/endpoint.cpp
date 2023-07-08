//
// $Id: endpoint.cpp 4 2007-04-10 22:55:27Z sparky1194 $
//
// Copyright (C) 2007 Turner Technolgoies Inc. http://www.turner.ca
//
// Permission is hereby granted, free of charge, to any person 
// obtaining a copy of this software and associated documentation 
// files (the "Software"), to deal in the Software without 
// restriction, including without limitation the rights to use, 
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following 
// conditions:
//      
// The above copyright notice and this permission notice shall be 
// included in all copies or substantial portions of the Software. 
//      
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
// OTHER DEALINGS IN THE SOFTWARE.

#include <stdio.h>
#include <algorithm>
#include <assert.h>
#include "ns3/endpoint.hpp"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/log.h"

using namespace std;

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("endpoint");

Endpoint::Endpoint(const EndpointConfig& config, TracedCallback<Ptr<const Packet> > trace_p, Ptr<Socket> socket,
		   EventInterface* eventInterface_p)
  : tcp(config.tcp), m_txTrace(trace_p), m_socket(socket),
    listenPort( config.listenPort), deviceMap( config.deviceMap),
    m_jitterMinNs(config.jitterMinNs), m_jitterMaxNs(config.jitterMaxNs)
{
    LogComponentEnable ("endpoint", LOG_LEVEL_INFO);
    m_rand_delay_ns = CreateObject<UniformRandomVariable> ();
    //qRegisterMetaType<Bytes>("Bytes");
    // create a device map with IP as the key
    std::map<DnpAddr_t, RemoteDevice>::iterator iter;
    for (iter = deviceMap.begin(); iter != deviceMap.end(); iter++) {
        //NS_LOG_INFO("ENDPOINT remote IP" << iter->second.ip << "id: " << iter->first);
        deviceIpMap[iter->second.ip] = iter->second;
    }

    char name[Stats::MAX_USER_NAME_LEN];

    // init stats object
    Stats::Element   temp[] =
    {
	{ RX_UDP_MULTICAST,      "Rx UDP Multicast"     , Stats::NORMAL  ,0,0},
	{ RX_UDP_PACKET,         "Rx UDP Packet"        , Stats::NORMAL  ,0,0},
	{ TX_UDP_PACKET,         "Tx UDP Packet"        , Stats::NORMAL  ,0,0},
	{ RX_UNKNOWN_IP,         "Rx Unknown IP"        , Stats::ABNORMAL,0,0},
    };
    assert(sizeof(temp)/sizeof(Stats::Element) == NUM_STATS);
    memcpy(statElements, temp, sizeof(temp));
    //config.ownerDnpAddr.CopyTo((unsigned char*)name);
    snprintf(name, Stats::MAX_USER_NAME_LEN, "EP %6d ", config.ownerDnpAddr);
    stats = Stats( name, config.ownerDnpAddr,
		   config.debugLevel_p, statElements, NUM_STATS,
		   eventInterface_p,
		   EventInterface::EP_AB_ST );
}

Endpoint::~Endpoint()
{
}

void Endpoint::update_sock(Ptr<Socket> sock) {
    m_socket = sock;
}

Uptime_t Endpoint::transmit( const Lpdu& lpdu)
{
    Uptime_t       timeSent =0;
    RemoteDevice   device;

    device = deviceMap[lpdu.getDest()];
    send(lpdu, device.ip, device.port);

    timeSent = 0;
    //stats.increment(TX_UDP_PACKET);
    //stats.logNormal( "Tx %s",hex_repr( lpdu.ab, strbuf,sizeof(strbuf)));

    return timeSent;
}

static uint8_t
char_to_uint8_t (char c)
{
  return uint8_t(c);
}

void
Endpoint::send (const Lpdu& lpdu, Address remote_addr, uint16_t remote_port)
{
  Ptr<Packet> p;

  // Convert given value into a Packet.
  size_t total_size = Lpdu::MAX_LEN;
  uint8_t *buffer = new uint8_t[total_size];
  uint8_t *buffer_ptr = buffer;
  uint8_t addr_buf[20];
  char data[Lpdu::MAX_LEN];
  std::transform (lpdu.ab.begin(), lpdu.ab.end(), buffer_ptr, char_to_uint8_t);
  //copy(lpdu.ab.begin(), lpdu.ab.end(), data);
  p = Create<Packet> (buffer, lpdu.ab.size());
  //p = Create<Packet> ((const uint8_t *)data, lpdu.ab.size());
  cout << "Sending data: " << buffer << endl;
  delete [] buffer;

  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);
//
  //remote_addr.CopyTo(addr_buf);
  //Ipv4Address addr = Ipv4Address((const char*)addr_buf);

  stringstream ss;
  ss << remote_addr;
  if (ss.str() == "00-00-00"){
      //remote_addr = Ipv4Address("172.17.0.3");
      //remote_port = 20000;
      cout << "The remote address is " << remote_addr << endl;
  }

  if (Ipv4Address::IsMatchingType (remote_addr))
    {
      cout << "Remote addr: " << remote_addr << " remote port " << remote_port << endl;
      InetSocketAddress address = GetInet(remote_addr, remote_port);
      cout << "IPV4!!!!!!!!!!!!! " << address.GetIpv4() << endl;
      if (tcp) {
        int (Socket::*fp)(Ptr<Packet>, uint32_t)  = &Socket::Send;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0); //virtual method
      } else {
	cout << "UDP" << endl;
        int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0, address); //virtual method
	//Simulator::Schedule(Seconds(101), fp, m_socket, p, 0, address);
      }
    }
  else if (Ipv6Address::IsMatchingType (remote_addr))
    {
      cout << "IPV6:::Remote addr6: " << remote_addr << " remote port " << remote_port << endl;      
      Inet6SocketAddress address = GetInet6(remote_addr, remote_port);
      if (tcp) {
        int (Socket::*fp)(Ptr<Packet>, uint32_t)  = &Socket::Send;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0); //virtual method
      } else {
        int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0, address); //virtual method
      }
    }
  ++m_sent;
}

InetSocketAddress Endpoint::GetInet (Address remote_addr, uint16_t remote_port)
{
  return InetSocketAddress(Ipv4Address::ConvertFrom(remote_addr), remote_port);
}

Inet6SocketAddress Endpoint::GetInet6 (Address remote_addr, uint16_t remote_port)
{
  return Inet6SocketAddress(Ipv6Address::ConvertFrom(remote_addr), remote_port);
}

}
