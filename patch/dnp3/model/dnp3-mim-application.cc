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
 *
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
using namespace std;

#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "dnp3-application.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/names.h"
#include "ns3/string.h"
#include "ns3/random-variable-stream.h"
#include "dnp3-application.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/event_interface.hpp"
#include "ns3/object.hpp"
#include "ns3/asdu.hpp"
#include "ns3/global-variables.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Dnp3Application");

NS_OBJECT_ENSURE_REGISTERED (Dnp3Application);


class CSVRow
{
    public:
        string const& geti(std::size_t index) const
        {
            return m_data[index];
        }
        std::size_t size() const
        {
            return m_data.size();
        }
        void readNextRow(std::istream& str)
        {
            std::string         line;
            std::getline(str,line);

            std::stringstream   lineStream(line);
            std::string         cell;

            m_data.clear();
            while(std::getline(lineStream,cell,','))
            {
                m_data.push_back(cell);
            }
        }
    private:
        std::vector<std::string>    m_data;
};

TypeId
Dnp3Application::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Dnp3Application")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<Dnp3Application> ()
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&Dnp3Application::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&Dnp3Application::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddAttribute ("LocalAddress",
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&Dnp3Application::m_localAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort",
                   "The source port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Dnp3Application::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MasterDeviceAddress",
                   "master device address",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Dnp3Application::m_master_device_addr),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("StationDeviceAddress",
                   "station device address",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Dnp3Application::m_station_device_addr),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RemoteAddress",
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&Dnp3Application::m_remoteAddress),
                   MakeAddressChecker ())
	.AddAttribute ("RemotePort",
				   "The destination port of the outbound packets",
				   UintegerValue (0),
	               MakeUintegerAccessor (&Dnp3Application::m_remotelPort),
	               MakeUintegerChecker<uint16_t> ())
	.AddAttribute ("isMaster",
				   "master or outstation",
				   BooleanValue (false),
				   MakeBooleanAccessor (&Dnp3Application::m_isMaster),
				   MakeBooleanChecker())
	.AddAttribute ("IntegrityPollInterval",
				   "Integrity poll interval",
				   UintegerValue (0),
				   MakeUintegerAccessor (&Dnp3Application::m_integrityInterval),
				   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("JitterMinNs",
                   "The source port of the outbound packets",
                   DoubleValue  (1000),
                   MakeDoubleAccessor (&Dnp3Application::m_jitterMinNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("JitterMaxNs",
                   "The source port of the outbound packets",
                   DoubleValue (100000),
                   MakeDoubleAccessor (&Dnp3Application::m_jitterMaxNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableTCP", "Enable TCP connection",
                   BooleanValue (true),
                   MakeBooleanAccessor (&Dnp3Application::m_enableTcp),
                   MakeBooleanChecker())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&Dnp3Application::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddAttribute ("AttackSelection", "Select the type of attack. Disconnect or send 0 payload",
                     UintegerValue (0),
                     MakeUintegerAccessor (&Dnp3Application::m_attackType),
                     MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("AttackStartTime", "Attack start time in seconds",
                     UintegerValue (0),
                     MakeUintegerAccessor (&Dnp3Application::m_attackStartTime),
                     MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("AttackEndTime", "Attack end time in seconds",
                     UintegerValue (0),
                     MakeUintegerAccessor (&Dnp3Application::m_attackEndTime),
                     MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Name",
               "The name of the application",
               StringValue (),
               MakeStringAccessor (&Dnp3Application::m_name),
               MakeStringChecker ())
    .AddAttribute ("OutFileName",
                   "The name of the output file",
                   StringValue (),
                   MakeStringAccessor (&Dnp3Application::f_name),
                   MakeStringChecker ())
  ;
  return tid;
}

Dnp3Application::Dnp3Application ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  //Setting up jitter random variable stream
  m_rand_delay_ns = CreateObject<UniformRandomVariable> ();
  m_rand_delay_ns->SetAttribute ("Min", DoubleValue  (m_jitterMinNs));
  m_rand_delay_ns->SetAttribute ("Max", DoubleValue  (m_jitterMaxNs));
}

Dnp3Application::~Dnp3Application()
{
  NS_LOG_FUNCTION (this);
}

uint32_t Dnp3Application::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
Dnp3Application::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
Dnp3Application::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void Dnp3Application::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}

void
Dnp3Application::SetName (const std::string &name)
{
  NS_LOG_FUNCTION (this << name);
  m_name = name;
  Names::Add("Dnp3_"+name, this);
}

std::string
Dnp3Application::GetName (void) const
{
  return m_name;
}

void
Dnp3Application::SetLocal (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void Dnp3Application::Store(std::string point, std::string value)
{
  //NS_LOG_FUNCTION (this << value);
  static int cnt = 0;

  if (false == m_isMaster) {
    //cout << "Point: " << point << "Value: " << value << endl;
    store_points(point, value);
//    //o_p->store(point, value);

  }
}

void
Dnp3Application::SetLocal (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  //m_localAddress = Address (ip);
  m_localAddress = ip;
  m_localPort = port;
}

void
Dnp3Application::SetLocal (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = Address (ip);
  m_localPort = port;
}

// Application Methods
void Dnp3Application::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  NS_LOG_FUNCTION (this << m_enableTcp);

  m_input_select = 0;
  m_victim = 0;
  m_attack_on = false;
  if(m_enableTcp)
  {
    makeTcpConnection();
  }
  else {
    makeUdpConnection();
  }
}

void Dnp3Application::makeTcpConnection(void) {
    // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);
      m_socket->Bind (m_localAddress);


      if (addressUtils::IsMulticast (m_localAddress))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
        }
    }

    if(m_isMaster == true) {
        NS_LOG_INFO("I'm Master");
        Simulator::Schedule(Seconds(10), &Dnp3Application::ConnectToPeer, this, m_socket,  m_remotelPort);
        startMaster();
        if (m_name.compare("MIM")!= 0) {
            periodic_poll(0);
        }
    } else {
        NS_LOG_INFO("I'm Outstation!");
        m_socket->Listen ();
        //m_socket->ShutdownSend ();
    }
    m_socket->SetRecvCallback (MakeCallback (&Dnp3Application::HandleRead, this));
    m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&Dnp3Application::HandleAccept, this));
    m_socket->SetCloseCallbacks (
    MakeCallback (&Dnp3Application::HandlePeerClose, this),
    MakeCallback (&Dnp3Application::HandlePeerError, this));
}

void Dnp3Application::makeUdpConnection(void) {
  NS_LOG_FUNCTION (this);

   // Create the socket if not already Onoff
  if (!m_socket)
       {
         m_socket = Socket::CreateSocket (GetNode (), m_tid);
         if (Inet6SocketAddress::IsMatchingType (m_peer))
           {
             if (m_socket->Bind6 () == -1)
               {
                 NS_FATAL_ERROR ("Failed to bind socket");
               }
           }
         else if (InetSocketAddress::IsMatchingType (m_peer) ||
                  PacketSocketAddress::IsMatchingType (m_peer))
           {
             if (m_socket->Bind () == -1)
               {
                 NS_FATAL_ERROR ("Failed to bind socket");
               }
           }
         m_socket->Connect (m_peer);
         m_socket->SetAllowBroadcast (true);
         m_socket->ShutdownRecv ();
   
         m_socket->SetConnectCallback (
           MakeCallback (&OnOffApplication::ConnectionSucceeded, this),
           MakeCallback (&OnOffApplication::ConnectionFailed, this));
       }
     m_cbrRateFailSafe = m_cbrRate;


  //Packet sink
    m_socket = Socket::CreateSocket (GetNode (), m_tid);
    if (m_socket->Bind (m_local) == -1)
           {
             NS_FATAL_ERROR ("Failed to bind socket");
           }
         m_socket->Listen ();
         m_socket->ShutdownSend ();
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
                 NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
               }
           }
       }
   
     m_socket->SetRecvCallback (MakeCallback (&PacketSink::HandleRead, this));
     m_socket->SetAcceptCallback (
       MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
       MakeCallback (&PacketSink::HandleAccept, this));
     m_socket->SetCloseCallbacks (
       MakeCallback (&PacketSink::HandlePeerClose, this),
       MakeCallback (&PacketSink::HandlePeerError, this));

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::Ipv4RawSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);

       if (Ipv4Address::IsMatchingType(m_localAddress) == true)
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_localPort);
          m_socket->Bind(local);
        }
      else if (Ipv6Address::IsMatchingType(m_localAddress) == true)
        {
          Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_localPort);
          m_socket->Bind(local);
        }
      else
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_localPort);
          m_socket->Bind(local);
          //NS_ASSERT_MSG (false, "Incompatible address type: " << m_localAddress);
        }
//Added TWE
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      mim_socket = Socket::CreateSocket (GetNode (), tid);

       if (Ipv4Address::IsMatchingType(m_remoteAddress) == true)
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_remotePort);
        mim_socket->Bind(local);
        }
      else if (Ipv6Address::IsMatchingType(m_remoteAddress) == true)
        {
          Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_remotePort);
          mim_socket->Bind(local);
        }
      else
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_remotePort);
         mim_socket->Bind(local);
          NS_ASSERT_MSG (false, "Incompatible address type: " << m_localAddress);
        }

    }
    m_socket->SetRecvCallback (MakeCallback (&Dnp3Application::HandleRead, this));

    if(m_isMaster == true) {

        startMaster();
        if (m_name.compare("MIM")!= 0) {
            NS_LOG_INFO("I'm Master");
            periodic_poll(0);
            Simulator::Schedule(Seconds(11), &Dnp3Application::send_control_binary, this, Dnp3Application::SELECT_OPERATE, 0, ControlOutputRelayBlock::TRIP);
            Simulator::Schedule(Seconds(21), &Dnp3Application::send_control_binary, this, Dnp3Application::DIRECT, 0, ControlOutputRelayBlock::CLOSE);
            Simulator::Schedule(Seconds(31), &Dnp3Application::send_control_analog, this, Dnp3Application::SELECT_OPERATE, 0, 3);
            Simulator::Schedule(Seconds(41), &Dnp3Application::send_control_analog, this, Dnp3Application::DIRECT, 0, 5);
        } else {
            NS_LOG_INFO("I'm MIM Master");
            if(m_attackStartTime)
            {
                //Schedule for start of the attack (in seconds)
                Simulator::Schedule(Seconds(m_attackStartTime), &Dnp3Application::set_attack, this, true); //virtual method
            }
            if(m_attackEndTime) {
                //Schedule for end of the attack (in seconds)
                Simulator::Schedule(Seconds(m_attackEndTime), &Dnp3Application::set_attack, this, false); //virtual method
            }
        }

    } else {
        if (m_name.compare("MIM")!= 0) {
            NS_LOG_INFO("I'm Outstation");
        } else {
                NS_LOG_INFO("I'm MIM Outstation");
                if(m_attackStartTime)
                {
                    //Schedule for start of the attack (in seconds)
                    Simulator::Schedule(Seconds(m_attackStartTime), &Dnp3Application::set_attack, this, true); //virtual method
                }
                if(m_attackEndTime) {
                    //Schedule for end of the attack (in seconds)
                    Simulator::Schedule(Seconds(m_attackEndTime), &Dnp3Application::set_attack, this, false); //virtual method
                }
        }
        m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_remoteAddress), m_remotelPort));
        startOutstation(m_socket);
    }
}

void Dnp3Application::ConnectToPeer(Ptr<Socket> localSocket, uint16_t servPort)
{
    NS_LOG_INFO("Remote: "<<m_remoteAddress);
    m_socket->Connect (InetSocketAddress(Ipv4Address::ConvertFrom(m_remoteAddress), m_remotelPort));
}

void Dnp3Application::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void Dnp3Application::initConfig(void)
{
    ifstream  pointsFile("./points.csv", ifstream::in);

    CSVRow row;
    int i = 0;
    while(pointsFile.good()) {
        row.readNextRow(pointsFile);

        if(row.geti(0).compare("ANALOG") == 0) {
            analog_points.insert(make_pair((string)row.geti(1), 0.0));
            analog_point_names.push_back((string)row.geti(1));
        } else if (row.geti(0).compare("BINARY") == 0) {
            char v = '0';
            bin_points[row.geti(1)] = 0;
            binary_point_names.push_back((string)row.geti(1));
        } else {
            printf("Invalid row %s", row.geti(1).c_str());
        }
        i++;
    }
}

void Dnp3Application::store_points(std::string name, std::string value)
{
    bool done = false;

    if (!analog_points.empty()) {
        if (analog_points.find(name) != analog_points.end()) {
            analog_points[name] = atof(value.c_str());
            //cout << "analog point found: " << name << "New item:" << analog_points[name] << endl;
        } else if (bin_points.find(name) != bin_points.end()) {
            if(value.compare("CLOSED") == 0) {
                bin_points[name] = 1;
            } else{
                bin_points[name] = 0;
            }
          //  cout << "bin point found: " << name << "New item:" << bin_points[name] << endl;
        } else {
            //cout << "point not found: " << name << endl;
        }
    }
}

void Dnp3Application::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << m_name << socket);
}
 
void Dnp3Application::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}
 

void Dnp3Application::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&Dnp3Application::HandleRead, this));
  m_socketList.push_back (s);
  startOutstation(s);
}

// implementation of EventInterface
void Dnp3Application::changePoint(        DnpAddr_t addr, DnpIndex_t index,
				     PointType_t    pointType,
				     int value, DnpTime_t timestamp)
{

}
void  Dnp3Application::registerName(       DnpAddr_t      addr,
				      DnpIndex_t     index,
				      PointType_t    pointType,
				      char*          name,
				      int            initialValue )
{
//    assert( pointType < NUM_POINT_TYPES);
//    QString key = convertDnpIndexToName(addr, index, pointType);
//    pointNameHash[key] = QString(name);
    changePoint( addr, index, pointType, initialValue);
}

void Dnp3Application::startMaster()
{
    //EventInterface ei_p;
    debugLevel = 0;
	// master and station config
	masterConfig.addr = m_master_device_addr;
	masterConfig.consecutiveTimeoutsForCommsFail = 1;
	integrityPollInterval = m_integrityInterval;
	masterConfig.integrityPollInterval_p = &integrityPollInterval;
	masterConfig.debugLevel_p = &debugLevel;
	stationConfig.addr = m_station_device_addr;
	stationConfig.debugLevel_p = &debugLevel;

	// datalink config
	datalinkConfig.addr                  = masterConfig.addr;
	datalinkConfig.isMaster              = true;
	datalinkConfig.debugLevel_p          = &debugLevel;

    // end point config
    endpointConfig.ownerDnpAddr     = masterConfig.addr;
    endpointConfig.tcp              = m_enableTcp;
    endpointConfig.initiating       = false;
    endpointConfig.listenPort       = m_remotelPort;

    remoteDevice.ip = m_remoteAddress;
    remoteDevice.port = m_remotelPort;
    deviceMap[stationConfig.addr]   = remoteDevice;

    endpointConfig.deviceMap        = deviceMap;
    endpointConfig.debugLevel_p     = &debugLevel;
    endpointConfig.jitterMinNs      = m_jitterMinNs;
    endpointConfig.jitterMaxNs      = m_jitterMaxNs;
	Endpoint* ep_p = new Endpoint(endpointConfig, m_txTrace, m_socket, this);

	// datalink required pointer to the transmit interface
	datalinkConfig.tx_p                  = ep_p;
	//TimerInterface* ti = NULL; //To check

	m_p = new Master (masterConfig, datalinkConfig, &stationConfig,
			  1,      // one station
			  this,   // event interface
			  &ti);   // timer interface
}



void Dnp3Application::periodic_poll(int count)
{
//    cout << count << endl;
    if (count == 0) {
        Simulator::Schedule(Seconds(4), &Dnp3Application::periodic_poll, this, ++count); //virtual method
    }
    else{
        if(count < 5500) {
            m_p->poll(Master::INTEGRITY);
            Simulator::Schedule(Seconds(4), &Dnp3Application::periodic_poll, this, ++count); //virtual method
        }
    }
}

void Dnp3Application::set_attack(bool state) {
    NS_LOG_INFO ("MIMServer::set_attack >>> Start Attack Mode: " << m_attackType);
    m_attack_on = state;
}

void Dnp3Application::send_control_binary(Dnp3Application::ControlType type, DnpIndex_t index, ControlOutputRelayBlock::Code code) {
    NS_LOG_INFO ("DNP3Application::send_control_binary");
    ControlOutputRelayBlock cb( code, index );
    if(type == Dnp3Application::SELECT_OPERATE) {
        m_p->control(cb);
    } else if(type == Dnp3Application::DIRECT) {
        m_p->direct_operate(true, cb);
    } else {
        NS_LOG_INFO ("Call to Dnp3Application::send_control_binary with bad control type");
    }
}

void Dnp3Application::send_control_analog(Dnp3Application::ControlType type, DnpIndex_t index, double value) {
    NS_LOG_INFO ("DNP3Application::send_control_analog");
    Bit32AnalogOutput ao(value, index);
    if(type == Dnp3Application::SELECT_OPERATE) {
        m_p->control(ao);
    } else if(type== Dnp3Application::DIRECT) {
        m_p->direct_operate(true, ao);
    } else {
        NS_LOG_INFO ("Call to Dnp3Application::send_control_analog with bad control type");
    }
}


void Dnp3Application::startOutstation(Ptr<Socket> sock)
{

    //OutstationConfig             outstationConfig;
    Endpoint::EndpointConfig                 epConfig;
    Datalink::DatalinkConfig                 dlConfig;
    RemoteDevice                             rd;
    std::map<DnpAddr_t, RemoteDevice>        rdMap;

    // debugs will be set by the outstation window
    debugLevel = 0;
    // outstation config
    outstationConfig.addr                = m_station_device_addr;
    outstationConfig.masterAddr          = m_master_device_addr;
    outstationConfig.userNum             = 3; // hard coded for prototype
    outstationConfig.debugLevel_p        = &debugLevel;

    // end point config
    epConfig.ownerDnpAddr          = m_master_device_addr;
    epConfig.tcp                   = m_enableTcp;
    epConfig.initiating            = false;
    epConfig.listenPort            = m_remotelPort;
    endpointConfig.debugLevel_p    = &debugLevel;

    rd.ip                          = m_remoteAddress;
    rd.port                        = m_remotelPort;
    rdMap[m_master_device_addr]    = rd;

    epConfig.deviceMap             = rdMap;
    epConfig.jitterMinNs           = m_jitterMinNs;
    epConfig.jitterMaxNs           = m_jitterMaxNs;

    // datalink config
    dlConfig.addr                  = outstationConfig.addr;
    dlConfig.isMaster              = false;
    dlConfig.debugLevel_p          = &debugLevel;

    Endpoint* ep_p = new Endpoint(epConfig, m_txTrace, sock, this);

    // datalink required pointer to the transmit interface
    dlConfig.tx_p  = ep_p;
    initConfig();
    //TimerInterface* ti = NULL; //Todo
    o_p = new Outstation ( outstationConfig, dlConfig,
			   this,   // event interface
			   &ti);   // timer interface
    o_p->set_point_names(analog_point_names, binary_point_names);
    vector<string>::iterator it;
}

static uint8_t
char_to_uint8_t (char c)
{
  return uint8_t(c);
}


void
Dnp3Application::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << m_name << socket);

  cout << m_name << " " << m_localAddress << endl;

  if (m_name.compare("MIM") == 0) {
      cout << m_name << endl;
      handle_MIM(socket);
  }
  else {
    handle_normal(socket);
  }
}

void Dnp3Application::handle_normal(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet;
    Uptime_t timeRxd = 0;

    while ((packet = socket->RecvFrom (from)))
    {
        m_txTrace(packet);
        //NS_LOG_INFO ("DNP3Application:: >>> address: " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << "   VictimAddress: " << victimAddr);
        uint32_t size = packet->GetSize();
        uint8_t *temp = new uint8_t[size];
        packet->CopyData(temp, size);

        Bytes buf((unsigned char*) temp, (unsigned char*)temp+size);

        DnpStat_t state;
        if (m_isMaster == true) {
          cout << "Master HandleRead Recieved: " << temp << endl;
          state = m_p->rxData(&buf, timeRxd);
        } else {
          cout << "Outstation HandleRead" << temp << endl;
          state = o_p->rxData(&buf, analog_points, bin_points, timeRxd);
        }
        //cout << state;
        if ( state != Station::IDLE) {
        }
    }
}



void Dnp3Application::handle_MIM(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> packet;
  Uptime_t timeRxd = 0;

    while ((packet = socket->RecvFrom (from)))
    {
      if(mitm_flag == false) {

        //NS_LOG_INFO ("MIMServer::HandleRead >>> destAddr: " << destAddr);
          if(m_attack_on) {
              if(m_attackType == 1) {
                  cout << "MIMServer::HandleRead >>> Attack is ON. Routing process is terminated by Man in the middle" << endl;
                  NS_LOG_INFO ("MIMServer::HandleRead >>> Attack is ON. Routing process is terminated by Man in the middle");
              } else if(m_attackType == 2) {
                  cout << "MIMServer::HandleRead >>> Attack is ON. Sending 0 payload by Man in the middle" << endl;
                  NS_LOG_INFO ("MIMServer::HandleRead >>> Attack is ON. Sending 0 payload by Man in the middle");
                  uint32_t size = packet->GetSize();

                  uint8_t *temp = new uint8_t[size];
                  packet->CopyData(temp, size);

                  NS_LOG_INFO ("MIMServer::HandleRead >>> MIM IP" << m_localAddress << " Forwarding packet to " << destAddr);
                  Bytes buf((unsigned char*) temp, (unsigned char*)temp+size);
                  Bytes emptyData;
                  Ptr<Packet> packet_mitm;
                  uint8_t *buffer = new uint8_t[Lpdu::MAX_LEN];
                  uint8_t *buffer_ptr = buffer;

                    //NS_LOG_INFO ("Dnp3Application for MIM::HandleRead >>> Sending to " << destAddr);

                    if(buf.size() >= Lpdu::HEADER_SIZE) {
                        int start1 = buf[0];
                        int start2 = buf[1];
                        int len = buf[2];
                        AppSeqNum_t seq = buf[11] & 0x0f;
                        printf ("Received Seq # %x\n", seq);
                        if((start1 == 0x05) && (0x64)) {
                            uint16_t dest = buf[5] << 0x08 | buf[4]; //To check
                            uint16_t src = buf[7] << 0x08 | buf[6]; //To check

                            Lpdu::UserData data;
                            data.dest = dest;
                            data.src = src;

                            if (m_isMaster) {
                                //m_p->sendEmptyPollRequest(data); //Does not make sense
                            } else {
                                o_p->transmitZeroResponse(data, analog_points, bin_points, seq);
                            }
                        }
                    }
              } else {
                      NS_LOG_INFO ("You input a wrong value.");
              }
          } else {
                send_directly(packet);
            }

      } else {
          NS_LOG_INFO ("MIMServer::HandleRead >>> Man in the middle node is the final destination.");
      }
    }
}

void Dnp3Application::send_directly(Ptr<Packet> p)
{
  NS_LOG_INFO ("MIMServer::HandleRead >>> Bypassing to destination: " << destAddr);
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

  if (Ipv4Address::IsMatchingType (m_remoteAddress))
    {
        //cout << "Remote addr: " << remote_addr << " remote port" << remote_port << endl;
      InetSocketAddress address = InetSocketAddress(Ipv4Address::ConvertFrom(m_remoteAddress), m_remotelPort);
      if (m_enableTcp) {
        int (Socket::*fp)(Ptr<Packet>, uint32_t)  = &Socket::Send;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0); //virtual method
      } else {
        int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0, address); //virtual method
      }
    }
  else if (Ipv6Address::IsMatchingType (m_remoteAddress))
    {
      Inet6SocketAddress address = Inet6SocketAddress(Ipv6Address::ConvertFrom(m_remoteAddress), m_remotelPort);
      if (m_enableTcp) {
        int (Socket::*fp)(Ptr<Packet>, uint32_t)  = &Socket::Send;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0); //virtual method
      } else {
        int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
        Simulator::Schedule(NanoSeconds (delay_ns), fp, m_socket, p, 0, address); //virtual method
      }
    }
}


} // Namespace ns3
