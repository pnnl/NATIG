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
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/writer.h>

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
#include "dnp3-application-new.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/names.h"
#include "ns3/string.h"
#include "ns3/random-variable-stream.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/event_interface.hpp"
#include "ns3/object.hpp"
#include "ns3/asdu.hpp"
//#include "ns3/global-variables.h"
//#include "fncs.hpp"
#include "ns3/seq-ts-header.h"
#include <cstdlib>
#include "ns3/helics.h"
#include "ns3/helics-simulator-impl.h"
#include "helics/helics.hpp"
#include "ns3/helics-helper.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Dnp3ApplicationNew");

NS_OBJECT_ENSURE_REGISTERED (Dnp3ApplicationNew);

std::string&
SanitizeName2 (std::string &name)
	{
		  std::replace (name.begin(), name.end(), '/', '+');
		    return name;
	}

std::string
SanitizeName2 (const std::string &name)
	{
		  std::string copy = name;
		    std::replace (copy.begin(), copy.end(), '/', '+');
		      return copy;
	}

std::string
toEndpointName2 (const std::string &name)
	{
		  std::string copy = name;
		  std::replace (copy.begin(), copy.end(), '/', '_');
		  return copy;
	}

vector<std::string> split2 (std::string s, std::string delimiter) {
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	string token;
	vector<string> res;

	while ((pos_end = s.find (delimiter, pos_start)) != std::string::npos) {
	     token = s.substr (pos_start, pos_end - pos_start);
	     pos_start = pos_end + delim_len;
	     res.push_back (token);
        }

        res.push_back (s.substr (pos_start));
        return res;
}

static bool endsWith2(const std::string& str, const std::string& suffix)
{
        return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

static bool startsWith2(const std::string& str, const std::string& prefix)
{
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

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
Dnp3ApplicationNew::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Dnp3ApplicationNew")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<Dnp3ApplicationNew> ()
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&Dnp3ApplicationNew::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&Dnp3ApplicationNew::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddAttribute ("LocalAddress",
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&Dnp3ApplicationNew::m_localAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort",
                   "The source port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Dnp3ApplicationNew::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MasterDeviceAddress",
                   "master device address",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Dnp3ApplicationNew::m_master_device_addr),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("StationDeviceAddress",
                   "station device address",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Dnp3ApplicationNew::m_station_device_addr),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RemoteAddress",
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&Dnp3ApplicationNew::m_remoteAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemoteAddress2",
		    "The source of the outbound packets for the insider",
		    AddressValue (Ipv4Address("10.0.0.0")),
		    MakeAddressAccessor (&Dnp3ApplicationNew::m_remoteAddress2),
		    MakeAddressChecker())
	.AddAttribute ("RemotePort",
				   "The destination port of the outbound packets",
				   UintegerValue (0),
	               MakeUintegerAccessor (&Dnp3ApplicationNew::m_remotelPort),
	               MakeUintegerChecker<uint16_t> ())
	.AddAttribute ("MasterPort", "The Master's destination port", UintegerValue (0), MakeUintegerAccessor (&Dnp3ApplicationNew::m_masterport), MakeUintegerChecker<uint16_t>())
	.AddAttribute ("isMaster",
				   "master or outstation",
				   BooleanValue (false),
				   MakeBooleanAccessor (&Dnp3ApplicationNew::m_isMaster),
				   MakeBooleanChecker())
	.AddAttribute ("IntegrityPollInterval",
				   "Integrity poll interval",
				   UintegerValue (0),
				   MakeUintegerAccessor (&Dnp3ApplicationNew::m_integrityInterval),
				   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PointsFilename",
                   "Input Points Definitions",
                   StringValue (),
                   MakeStringAccessor (&Dnp3ApplicationNew::points_filename),
                   MakeStringChecker ())
    .AddAttribute ("JitterMinNs",
                   "The source port of the outbound packets",
                   DoubleValue  (1000),
                   MakeDoubleAccessor (&Dnp3ApplicationNew::m_jitterMinNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("JitterMaxNs",
                   "The source port of the outbound packets",
                   DoubleValue (100000),
                   MakeDoubleAccessor (&Dnp3ApplicationNew::m_jitterMaxNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableTCP", "Enable TCP connection",
                   BooleanValue (true),
                   MakeBooleanAccessor (&Dnp3ApplicationNew::m_enableTcp),
                   MakeBooleanChecker())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&Dnp3ApplicationNew::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx2", "A packet has been received",
                       MakeTraceSourceAccessor (&Dnp3ApplicationNew::m_rxTraces),
                      "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                       MakeTraceSourceAccessor (&Dnp3ApplicationNew::m_rxTraceWithAddresses),
                      "ns3::Packet::TwoAddressTracedCallback")
    .AddAttribute ("AttackSelection", "Select the type of attack. Disconnect or send 0 payload",
                     UintegerValue (0),
                     MakeUintegerAccessor (&Dnp3ApplicationNew::m_attackType),
                     MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Value_attck", "Select a value to set the point that is being manipulated",
		    StringValue ("NA"),
		    MakeStringAccessor (&Dnp3ApplicationNew::m_attack_point_val),
		    MakeStringChecker ())
    .AddAttribute ("Value_attck_max", "Select the max value to set the point that is being manipulated",
		    StringValue ("NA"),
		    MakeStringAccessor (&Dnp3ApplicationNew::m_attack_max),
		    MakeStringChecker ())
    .AddAttribute ("Value_attck_min", "Select the min value to set the point that is being manipulated",
		    StringValue ("NA"),
		    MakeStringAccessor (&Dnp3ApplicationNew::m_attack_min),
		    MakeStringChecker ())
    .AddAttribute ("PointID", "The ID of the point that is being modified for nodeX ex:Pref, Qref",
		    StringValue (),
		    MakeStringAccessor (&Dnp3ApplicationNew::point_id),
		    MakeStringChecker ())
    .AddAttribute ("NodeID", "The ID of the node that has a point being modified, note before the $",
		    StringValue (),
		    MakeStringAccessor (&Dnp3ApplicationNew::node_id),
		    MakeStringChecker())
    .AddAttribute ("RealVal", "The value that the victim should be set back to after the attack ends",
		    StringValue ("NA"),
		    MakeStringAccessor (&Dnp3ApplicationNew::RealVal),
		    MakeStringChecker())
    .AddAttribute ("AttackConf", "The config file that contains the attack parameters",
		    StringValue ("NA"),
		    MakeStringAccessor (&Dnp3ApplicationNew::configFile),
		    MakeStringChecker())
    .AddAttribute ("AttackStartTime", "Attack start time in seconds",
                     StringValue ("0"),
                     MakeStringAccessor (&Dnp3ApplicationNew::m_attackStartTime),
		     MakeStringChecker())
                     //MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("AttackEndTime", "Attack end time in seconds",
                     StringValue ("0"),
                     MakeStringAccessor (&Dnp3ApplicationNew::m_attackEndTime),
                     MakeStringChecker())
    .AddAttribute ("AttackChance", "Attack chance in percentage (0 to 1)",
		   DoubleValue (1.0),
                   MakeDoubleAccessor (&Dnp3ApplicationNew::m_attackChance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Name",
               "The name of the application",
               StringValue (),
               MakeStringAccessor (&Dnp3ApplicationNew::m_name),
               MakeStringChecker ())
    .AddAttribute ("ID", "Int representing the ID of the MIM attacker",
		     UintegerValue (0),
		     MakeUintegerAccessor (&Dnp3ApplicationNew::MIM_ID),
		     MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OutFileName",
                   "The name of the output file",
                   StringValue (),
                   MakeStringAccessor (&Dnp3ApplicationNew::f_name),
                   MakeStringChecker ())
    .AddAttribute ("mitmFlag", "Man in the middle flag",
		    BooleanValue (false),
		    MakeBooleanAccessor (&Dnp3ApplicationNew::mitm_flag),
		    MakeBooleanChecker())
  ;
  return tid;
}

Dnp3ApplicationNew::Dnp3ApplicationNew ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  mim_socket = 0;
  //Setting up jitter random variable stream
  m_rand_delay_ns = CreateObject<UniformRandomVariable> ();
  m_rand_delay_ns->SetAttribute ("Min", DoubleValue  (m_jitterMinNs));
  m_rand_delay_ns->SetAttribute ("Max", DoubleValue  (m_jitterMaxNs));

}

Dnp3ApplicationNew::~Dnp3ApplicationNew()
{
  NS_LOG_FUNCTION (this);
}

uint32_t Dnp3ApplicationNew::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
Dnp3ApplicationNew::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
Dnp3ApplicationNew::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void Dnp3ApplicationNew::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  mim_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}

void
Dnp3ApplicationNew::SetName (const std::string &name)
{
  NS_LOG_FUNCTION (this << name);
  NS_LOG_FUNCTION (this << name);
  m_name = name;
  std::string fedName = helics_federate->getName();
  size_t pos = name.find(fedName);
  if(pos != std::string::npos) {
      m_name.erase(pos, fedName.length()+1);
  }
  std::cout << "I am trying to add the following" << std::endl;
  std::cout << m_name << std::endl;
  if (strcmp(m_name.c_str(), "mg1") == 0){
    helics_federate->registerEndpoint("trip_shad_inv1$Pref");
  }
  if(m_name.find("MIM") == string::npos){
      Names::Add (SanitizeName2 (m_name), this);
  }
  //m_name = name;
  //Names::Add("Dnp3_"+name, this);
}

std::string
Dnp3ApplicationNew::GetName (void) const
{
  return m_name;
}

void
Dnp3ApplicationNew::SetLocal (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void Dnp3ApplicationNew::Store(std::string point, std::string value)
{
  //NS_LOG_FUNCTION (this << value);

  if (false == m_isMaster) {
    //cout << "Point: " << point << "Value: " << value << endl;
    store_points(point, value);
//    //o_p->store(point, value);

  }
}

void
Dnp3ApplicationNew::SetLocal (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  //m_localAddress = Address (ip);
  m_localAddress = ip;
  m_localPort = port;
}

void
Dnp3ApplicationNew::SetLocal (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = Address (ip);
  m_localPort = port;
}

// Application Methods
void Dnp3ApplicationNew::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  NS_LOG_FUNCTION (this << m_enableTcp);

  running = true;
  m_input_select = 0;
  m_victim = 0;
  m_attack_on = false;
  NS_LOG_LOGIC("I am is the start application :)");
  if(m_enableTcp)
  {
    makeTcpConnection();
  }
  else {
    makeUdpConnection();
  }
}

void Dnp3ApplicationNew::makeTcpConnection(void) {
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
        NS_LOG_INFO("I'm Master with port " << m_remotelPort);
        Simulator::Schedule(Seconds(10), &Dnp3ApplicationNew::ConnectToPeer, this, m_socket,  m_remotelPort);
        startMaster();
    } else {
        NS_LOG_INFO("I'm Outstation! with port " << m_remotelPort);
        m_socket->Listen ();
        //m_socket->ShutdownSend ();
    }
    m_socket->SetRecvCallback (MakeCallback (&Dnp3ApplicationNew::HandleRead, this));
    m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&Dnp3ApplicationNew::HandleAccept, this));
    m_socket->SetCloseCallbacks (
    MakeCallback (&Dnp3ApplicationNew::HandlePeerClose, this),
    MakeCallback (&Dnp3ApplicationNew::HandlePeerError, this));
}

void Dnp3ApplicationNew::readMicroGridConfig(std::string fpath, Json::Value& configobj)
{
           std::ifstream tifs(fpath);
           Json::Reader configreader;
           configreader.parse(tifs, configobj);
}

void Dnp3ApplicationNew::GetStartStopArray(){
  NS_LOG_FUNCTION (this);
  //I need to create an array of the start and stop times that can be read from the config file
  Json::Value configObject;
  readMicroGridConfig(configFile, configObject);
  
  std::map<std::string, std::string> attack;
  
  for (uint32_t j = 1; j < configObject["MIM"].size(); j++){
	  for(const auto& item : configObject["MIM"][j].getMemberNames() ){
		  std::string ID = "MIM-"+std::to_string(j)+"-"+item;
		  std::string my_str = configObject["MIM"][j][item].asString();
		  my_str.erase(remove(my_str.begin(), my_str.end(), '"'), my_str.end());
		  std::cout << "This is the keys value: " << ID << "  and the value is " << my_str << std::endl;
		  attack.insert(pair<std::string,std::string >(ID, my_str));
	  }
	  
  }
  std::vector<float> timer;
  std::string delimiter = "-";
  std::string delimiter2 = ",";
  size_t pos = 0;
  size_t pos_1 = 0;
  std::string token;
  while ((pos = attack["MIM-"+std::to_string(MIM_ID)+"-Start"].find(delimiter)) != std::string::npos) {
        token = attack["MIM-"+std::to_string(MIM_ID)+"-Start"].substr(0, pos);
	std::cout << "Stof 1" << std::endl;
	std::string token_int;
        while((pos_1 = token.find(delimiter2)) != std::string::npos){
          token_int = token.substr(0,pos_1);
	  timer.push_back(std::stof(token_int));
	  token.erase(0, pos_1 + delimiter2.length());
	}
	timer.push_back(std::stof(token));
        attack["MIM-"+std::to_string(MIM_ID)+"-Start"].erase(0, pos + delimiter.length());
  }
  std::cout << "I am printing here " << attack["MIM-"+std::to_string(MIM_ID)+"-Start"] << std::endl;
  std::string token_int;
  while((pos_1 = attack["MIM-"+std::to_string(MIM_ID)+"-Start"].find(delimiter2)) != std::string::npos){
      token_int = attack["MIM-"+std::to_string(MIM_ID)+"-Start"];
      timer.push_back(std::stof(token_int));
      attack["MIM-"+std::to_string(MIM_ID)+"-Start"].erase(0, pos_1 + delimiter2.length());
  }
  timer.push_back(std::stof(attack["MIM-"+std::to_string(MIM_ID)+"-Start"]));

  std::vector<float> timer_end;
  pos = 0;
  pos_1 = 0;
  std::string token1;
  while ((pos = attack["MIM-"+std::to_string(MIM_ID)+"-End"].find(delimiter)) != std::string::npos) {
        token1 = attack["MIM-"+std::to_string(MIM_ID)+"-End"].substr(0, pos);
	std::cout << "Stof 1" << std::endl;
	std::string token_int;
        while((pos_1 = token1.find(delimiter2)) != std::string::npos){
          token_int = token1.substr(0,pos_1);
	  timer.push_back(std::stof(token_int));
	  token1.erase(0, pos_1 + delimiter2.length());
	}
	timer.push_back(std::stof(token1));
        attack["MIM-"+std::to_string(MIM_ID)+"-End"].erase(0, pos + delimiter.length());
  }
  std::cout << "I am printing here " << attack["MIM-"+std::to_string(MIM_ID)+"-End"] << std::endl;
  //std::string token_int;
  while((pos_1 = attack["MIM-"+std::to_string(MIM_ID)+"-End"].find(delimiter2)) != std::string::npos){
      token_int = attack["MIM-"+std::to_string(MIM_ID)+"-End"];
      timer.push_back(std::stof(token_int));
      attack["MIM-"+std::to_string(MIM_ID)+"-End"].erase(0, pos_1 + delimiter2.length());
  }
  timer.push_back(std::stof(attack["MIM-"+std::to_string(MIM_ID)+"-End"]));
  /*size_t pos1 = 0;
  std::string token1;
  while ((pos1 = attack["MIM-"+std::to_string(MIM_ID)+"-End"].find(delimiter)) != std::string::npos){
        token1 = attack["MIM-"+std::to_string(MIM_ID)+"-End"].substr(0, pos1);
	std::cout << "Stof 2" << std::endl;
	timer_end.push_back(std::stof(token1));
	attack["MIM-"+std::to_string(MIM_ID)+"-End"].erase(0, pos1 + delimiter.length());
  }
  std::cout << "I am printing here 2 " << attack["MIM-"+std::to_string(MIM_ID)+"-End"] << std::endl;
  timer_end.push_back(std::stof(attack["MIM-"+std::to_string(MIM_ID)+"-End"]));*/
}

std::vector<float> Dnp3ApplicationNew::GetVal(std::map<std::string, std::string> attack, std::string tag) {
  std::vector<float> timer;
  std::string delimiter = ",";
  size_t pos = 0;
  std::string token;
  std::cout << MIM_ID << " " << tag << std::endl;
  while ((pos = attack["MIM-"+std::to_string(MIM_ID)+"-"+tag].find(delimiter)) != std::string::npos) {
        token = attack["MIM-"+std::to_string(MIM_ID)+"-"+tag].substr(0, pos);
	std::cout << "Stof 1" << std::endl;
	timer.push_back(std::stof(token));
        attack["MIM-"+std::to_string(MIM_ID)+"-"+tag].erase(0, pos + delimiter.length());
  }
  std::cout << "I am printing here ----------------------- " << attack["MIM-"+std::to_string(MIM_ID)+"-"+tag] << std::endl;
  timer.push_back(std::stof(attack["MIM-"+std::to_string(MIM_ID)+"-"+tag]));
  return timer;
}

void Dnp3ApplicationNew::makeUdpConnection(void) {
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC("I AM IN MAKEUDP CONNECTIONS :)");
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);

       if (Ipv4Address::IsMatchingType(m_remoteAddress) == true)
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_localPort);
          m_socket->Bind(local);
	  NS_LOG_LOGIC("dddddddd");
        }
      else if (Ipv6Address::IsMatchingType(m_remoteAddress) == true)
        {
          Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_localPort);
          m_socket->Bind(local);
	  NS_LOG_INFO("QQQQQQQQQQQQ");
        }
      else
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_localPort);
          m_socket->Bind(local);
	  NS_LOG_INFO("GOOD!!!!!!!!! " << m_localAddress);
          //NS_ASSERT_MSG (false, "Incompatible address type: " << m_localAddress);
        }
//Added TWE
      TypeId tid2 = TypeId::LookupByName ("ns3::UdpSocketFactory");
      mim_socket = Socket::CreateSocket (GetNode (), tid2);

       if (Ipv4Address::IsMatchingType(m_remoteAddress2) == true)
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_remotelPort);
        mim_socket->Bind(local);
	NS_LOG_INFO ("HELP");
        }
      else if (Ipv6Address::IsMatchingType(m_remoteAddress2) == true)
        {
	  NS_LOG_INFO("ME!!!!!");
          Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_remotelPort);
          mim_socket->Bind(local);
        }
      else
        {
	  NS_LOG_INFO("PLEASE!!");
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_remotelPort);
         mim_socket->Bind(local);
          //NS_ASSERT_MSG (false, "Incompatible address type: " << m_localAddress);
        }

    }
    m_socket->SetRecvCallback (MakeCallback (&Dnp3ApplicationNew::HandleRead, this));
    mim_socket->SetRecvCallback (MakeCallback (&Dnp3ApplicationNew::HandleRead, this));
    
    //Getting the list of start time
    std::vector<float> timer;
    std::string delimiter = ",";
    size_t pos = 0;
    std::string token;
    while ((pos = m_attackStartTime.find(delimiter)) != std::string::npos) {
	token = m_attackStartTime.substr(0, pos);
	std::cout << "Stof 1" << std::endl;
	timer.push_back(std::stof(token));
        m_attackStartTime.erase(0, pos + delimiter.length());
    }
    std::cout << "I am printing here " << m_attackStartTime << std::endl;
    timer.push_back(std::stof(m_attackStartTime));

    std::vector<float> timer_end;
    size_t pos1 = 0;
    std::string token1;
    while ((pos1 = m_attackEndTime.find(delimiter)) != std::string::npos){
        token1 = m_attackEndTime.substr(0, pos1);
	std::cout << "Stof 2" << std::endl;
	timer_end.push_back(std::stof(token1));
	m_attackEndTime.erase(0, pos1 + delimiter.length());
    }
    std::cout << "I am printing here 2 " << m_attackEndTime << std::endl;
    timer_end.push_back(std::stof(m_attackEndTime));

    std::cout << "My name is: " << std::endl;
    std::cout << m_name << std::endl;
    for (int index = 0; index < timer.size(); index ++){
    if(m_isMaster == true) {

        startMaster();
        if(m_name.find("Inside") != std::string::npos or m_name.find("MIM") != std::string::npos){
            NS_LOG_UNCOND("I'm MIM or Insider Master");
            if(timer[index])
            {
                //Schedule for start of the attack (in seconds)
		std::cout << "Start threads ----------------------" << std::endl;
                Simulator::Schedule(Seconds(timer[index]), &Dnp3ApplicationNew::set_attack, this, true); //virtual method
		StartVect.push_back(std::to_string(timer[index]));
            }
            if(timer_end[index]) {
                //Schedule for end of the attack (in seconds)
                Simulator::Schedule(Seconds(timer_end[index]), &Dnp3ApplicationNew::set_attack, this, false); //virtual method
		StopVect.push_back(std::to_string(timer_end[index]));
            }
        }

    } else {
	 if(m_name.find("Inside") != std::string::npos or m_name.find("MIM") != std::string::npos){
		//if (m_name.find("Inside") != std::string::npos){
                  startMaster();
		  //Simulator::Schedule(Seconds(11), &Dnp3Application::send_control_binary, this, Dnp3Application::SELECT_OPERATE, 0, ControlOutputRelayBlock::TRIP);
		  //Simulator::Schedule(Seconds(21), &Dnp3Application::send_control_binary, this, Dnp3Application::DIRECT, 0, ControlOutputRelayBlock::CLOSE);
		  //Simulator::Schedule(Seconds(31), &Dnp3Application::send_control_analog, this, Dnp3Application::SELECT_OPERATE, 0, 3);
		  //Simulator::Schedule(Seconds(41), &Dnp3Application::send_control_analog, this, Dnp3Application::DIRECT, 0, 5);
		//}
                NS_LOG_UNCOND("I'm MIM or Insider Outstation");
                if(timer[index])
                {
                    //Schedule for start of the attack (in seconds)
		    std::cout << "start threads " << timer[index] << std::endl;
                    Simulator::Schedule(Seconds(timer[index]), &Dnp3ApplicationNew::set_attack, this, true); //virtual method
		    StartVect.push_back(std::to_string(timer[index]));
		    std::cout << "Added to vector" << std::endl;
                }
                if(timer_end[index]) {
                    //Schedule for end of the attack (in seconds)
		    std::cout << "End threads " << timer_end[index] << std::endl;
                    Simulator::Schedule(Seconds(timer_end[index]), &Dnp3ApplicationNew::set_attack, this, false); //virtual method
		    StopVect.push_back(std::to_string(timer_end[index]));
		    std::cout << "Added to vector" << std::endl;
                }
        }
	//m_socket->SetRecvCallback( MakeCallback(&Dnp3Application::HandleRead, this));
        m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_remoteAddress), m_remotelPort));
	std::cout << "FLAG1" << std::endl;
	mim_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_remoteAddress2), m_localPort));
	std::cout << "FLAG2" << std::endl;
        startOutstation(m_socket);
	std::cout << "HEERREE" << std::endl;
	if (m_name.find("Inside") != std::string::npos or m_name.find("MIM")!=std::string::npos){
	  startOutstation(mim_socket);
	}
    }
    }
}

void Dnp3ApplicationNew::ConnectToPeer(Ptr<Socket> localSocket, uint16_t servPort)
{
    NS_LOG_INFO("Remote: "<<m_remoteAddress);
    m_socket->Connect (InetSocketAddress(Ipv4Address::ConvertFrom(m_remoteAddress), m_remotelPort));
    mim_socket->Connect (InetSocketAddress(Ipv4Address::ConvertFrom(m_remoteAddress2),m_localPort));
}

void Dnp3ApplicationNew::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  running = false;
  NS_LOG_INFO("CLOSING APPLICATION @@@@@@@@@@@@@@@@@@@@@@@@");
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
  if(mim_socket)
  {
     mim_socket->Close();
     mim_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> >());
  }
}

/*void Dnp3ApplicationNew::initConfig(void)
{
NS_LOG_INFO("DNp3Application:initConfig");
    ifstream  pointsFile(points_filename, ifstream::in);
    //get all values from the zpl file to compare
    //vector <string> zplKeys = fncs::get_keys();

    vector <string> zplKeys = {"asg", "jskjf", "kdlfs"};

    if(pointsFile) {
      //have bool to track if the value is valid
      bool isValid;
	    CSVRow row;
	    int i = 0;
	    while(pointsFile.good()) {
		row.readNextRow(pointsFile);
                if(row.size() > 0) {
		    if(row.geti(0).compare("ANALOG") == 0) {
		        NS_LOG_INFO("Adding Analog: " << ((string) row.geti(1)));
            isValid = true;
		        analog_points.insert(make_pair((string)row.geti(1), stoi(row.geti(2))));//0.0));
		        analog_point_names.push_back((string)row.geti(1));

		    } else if (row.geti(0).compare("BINARY") == 0) {
                        NS_LOG_INFO("Adding Binary: " << ((string) row.geti(1)));
            isValid = true;
	                bin_points.insert(make_pair((string)row.geti(1), stoi(row.geti(2))));
		        //bin_points[row.geti(1)] = stoi(row.geti(2));//0;
		        binary_point_names.push_back((string)row.geti(1));
            //NS_LOG_INFO("Current binary_point_names size " << binary_point_names.size());
		    } else {
		        NS_LOG_INFO("Invalid row %s" << row.geti(1).c_str());
            isValid = false;
		    }
        //only check if they are slaves and are valid

        if(!m_isMaster && isValid){
          //concatenate strings so they will match the zpl strings

          int indexStart = points_filename.find("-",0);
          int indexEnd = points_filename.find("-", indexStart+1);
          string combinedName = (string)row.geti(1);
          string alteredFileName = points_filename.substr(indexStart+1, indexEnd-indexStart-1);
          //capitalize the file name to match expected format in zpl file
          transform(alteredFileName.begin(), alteredFileName.end(), alteredFileName.begin(), ::toupper);
          combinedName = alteredFileName+ "/" + combinedName;
          //check the zpl file to see if there is a corresponding entry
          NS_LOG_INFO("Checking for " << combinedName << " in config file");

          vector<string>::iterator it = find(zplKeys.begin(), zplKeys.end(), combinedName);
            // NS_LOG_INFO("Binary name: " << name);
          if(it == zplKeys.end()){
	    zplKeys.push_back(combinedName);
          }



        }

		    i++;
                }
	    }
    } else {
        NS_LOG_INFO("Unable to open points file:" << points_filename);
        exit(-1);
    }
    //TO DO Check all points values added and see if they are all accounted
    //for in the fncs configuration .zpl file
    //attempting to include fncs header to be able to use the function
    //fncs::getkeys()
    //vector <string> zplKeys = fncs::get_keys();
*/
/*
    for(vector<string>::const_iterator i = zplKeys.begin(); i != zplKeys.end();i++){
      NS_LOG_INFO("Retrieved zpl key ");
      string temp = *i;
      NS_LOG_INFO(temp);
    }
*/
    //make sure all points parsed appear on the zplKeys




    // NS_LOG_INFO("Analog size: " << analog_points.size() << " Binary size: " << bin_points.size());
//}
void Dnp3ApplicationNew::initConfig(void)
{
	NS_LOG_FUNCTION (this);
	std::cout << points_filename << std::endl;
	ifstream  pointsFile(points_filename, ifstream::in);
	if(pointsFile) {
		bool isValid;
		CSVRow row;
		int i = 0;
		//helics_federate->registerEndpoint("trip_shad_inv1$Pref");
		while(pointsFile.good()) {
			row.readNextRow(pointsFile);
			if(row.size() > 0) {
				if(row.geti(0).compare("ANALOG") == 0) {
					NS_LOG_INFO("Adding Analog: " << ((string) row.geti(1)));
					isValid = true;
					analog_points.insert(make_pair((string)row.geti(1), stoi(row.geti(2))));//0.0));
					analog_point_names.push_back((string)row.geti(1));
				} else if (row.geti(0).compare("BINARY") == 0) {
					NS_LOG_INFO("Adding Binary: " << ((string) row.geti(1)));
					isValid = true;
					bin_points[row.geti(1)] = stoi(row.geti(2));//0;
					binary_point_names.push_back((string)row.geti(1));
				} else {
					NS_LOG_INFO("Invalid row %s" << row.geti(1).c_str());
					isValid = false;
				}
				
				if(!m_isMaster && isValid){
					
					int indexStart = points_filename.find("-",0);
					int indexEnd = points_filename.find("-", indexStart+1);
					string combinedName = (string)row.geti(1);
					string alteredFileName = points_filename.substr(indexStart+1, indexEnd-indexStart-1);
					transform(alteredFileName.begin(), alteredFileName.end(), alteredFileName.begin(), ::toupper);
					//helics_federate->registerEndpoint(combinedName);
					combinedName = alteredFileName+ "/" + combinedName;
					NS_LOG_INFO("Checking for " << combinedName << " in config file");
					//m_endpoint_id = helics_federate->registerEndpoint (combinedName);
				}
				i++;
			}
		}
	} else {
		NS_LOG_INFO("Unable to open points file:" << points_filename);
		exit(-1);
	}
}


void Dnp3ApplicationNew::store_points(std::string name, std::string value)
{

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
            NS_LOG_INFO("point not found " << name);
        }
    }
}

void Dnp3ApplicationNew::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << m_name << socket);
}

void Dnp3ApplicationNew::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}


void Dnp3ApplicationNew::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&Dnp3ApplicationNew::HandleRead, this));
  m_socketList.push_back (s);
  startOutstation(s);
  NS_LOG_INFO("In Handle Accept");
}


// implementation of EventInterface
void Dnp3ApplicationNew::changePoint(        DnpAddr_t addr, DnpIndex_t index,
				     PointType_t    pointType,
				     int value, DnpTime_t timestamp)
{

}
            // NS_LOG_INFO("Binary name: " << name);
void  Dnp3ApplicationNew::registerName(       DnpAddr_t      addr,
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

void Dnp3ApplicationNew::startMaster()
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

    if (m_remoteAddress2 == Ipv4Address("10.0.0.0")){
        remoteDevice.ip = m_remoteAddress;
	remoteDevice.port = m_remotelPort;
    }else{
        remoteDevice.ip = m_remoteAddress2;
        remoteDevice.port = m_localPort;
	endpointConfig.listenPort = m_localPort;
    }
    deviceMap[stationConfig.addr]   = remoteDevice;

    endpointConfig.deviceMap        = deviceMap;
    endpointConfig.debugLevel_p     = &debugLevel;
    endpointConfig.jitterMinNs      = m_jitterMinNs;
    endpointConfig.jitterMaxNs      = m_jitterMaxNs;
    Endpoint* ep_p = new Endpoint(endpointConfig, m_txTrace, mim_socket, this);

    // datalink required pointer to the transmit interface
    datalinkConfig.tx_p                  = ep_p;
    //DummyTimer ti2; //To check

    // NS_LOG_INFO("Binary name: " << name);
    m_p = new Master (masterConfig, datalinkConfig, &stationConfig,
			  1,      // one station
			  this,   // event interface
			  &ti);   // timer interface
}

void Dnp3ApplicationNew::attack_data(int freq)
{
    if(running) {
        o_p->transmitScadaData(analog_points, bin_points);	
        Simulator::Schedule(MilliSeconds(count), &Dnp3ApplicationNew::attack_data, this, count);
    }
}

void Dnp3ApplicationNew::periodic_poll(int count)
{
//    cout << count << endl;
//    if (count == 0) {
//        Simulator::Schedule(Seconds(4), &Dnp3Application::periodic_poll, this, ++count); //virtual method
//    }
//    else{

        if(running) {
            m_p->poll(Master::INTEGRITY);
            Simulator::Schedule(MilliSeconds(count), &Dnp3ApplicationNew::periodic_poll, this, count); //virtual method
        }
//    }
}

void Dnp3ApplicationNew::periodic_poll_Integrity(int pollRate){
  if(running){
    NS_LOG_INFO("INSIDE periodic_poll_Integrity");
    m_p->poll(Master::INTEGRITY);
    Simulator::Schedule(MilliSeconds(pollRate), &Dnp3ApplicationNew::periodic_poll_Integrity, this, pollRate); //virtual method
  }

}

void Dnp3ApplicationNew::periodic_poll_Binary(int pollRate){
  if(running){
    NS_LOG_INFO("INSIDE periodic_poll_Binary");
    m_p->poll(Master::BINARY);
    Simulator::Schedule(MilliSeconds(pollRate), &Dnp3ApplicationNew::periodic_poll_Binary, this, pollRate);
  }

}

void Dnp3ApplicationNew::periodic_poll_Analog(int pollRate){
  if(running){
    NS_LOG_INFO("INSIDE periodic_poll_Analog");
    m_p->poll(Master::ANALOG);
    Simulator::Schedule(MilliSeconds(pollRate), &Dnp3ApplicationNew::periodic_poll_Analog, this, pollRate);
  }
}

            // NS_LOG_INFO("Binary name: " << name);
void Dnp3ApplicationNew::periodic_poll_Class(int pollRate){
  if(running){

    NS_LOG_INFO("INSIDE periodic_poll_Class");
    m_p->poll(Master::CLASS);
    Simulator::Schedule(MilliSeconds(pollRate), &Dnp3ApplicationNew::periodic_poll_Class, this, pollRate);
  }

}

void Dnp3ApplicationNew::set_attack(bool state) {
    NS_LOG_INFO ("MIMServer::set_attack >>> Start Attack Mode: " << m_attackType);
    m_attack_on = state;
}

void Dnp3ApplicationNew::send_control_binary(Dnp3ApplicationNew::ControlType type, DnpIndex_t index, ControlOutputRelayBlock::Code code) {
    NS_LOG_INFO ("DNP3Application::send_control_binary");
    NS_LOG_INFO ("Kyle application name: " << m_name);
    ControlOutputRelayBlock cb( code, index );
    if(type == Dnp3ApplicationNew::SELECT_OPERATE) {
        m_p->control(cb);
    } else if(type == Dnp3ApplicationNew::DIRECT) {
        m_p->direct_operate(true, cb);
    } else {
        NS_LOG_INFO ("Call to Dnp3Application::send_control_binary with bad control type");
    }
}

void Dnp3ApplicationNew::send_control_analog(Dnp3ApplicationNew::ControlType type, DnpIndex_t index, double value) {
    NS_LOG_INFO ("DNP3Application::send_control_analog");
    Bit32AnalogOutput ao(value, index);
    if(type == Dnp3ApplicationNew::SELECT_OPERATE) {
        m_p->control(ao);
    } else if(type== Dnp3ApplicationNew::DIRECT) {
        m_p->direct_operate(true, ao);
    } else {
        NS_LOG_INFO ("Call to Dnp3Application::send_control_analog with bad control type");
    }
}

void Dnp3ApplicationNew::send_device_attribute_request(DnpIndex_t index) {
    NS_LOG_INFO ("DNP3Application::send_device_attribute_request");
    
    m_p->getAttribute(index);
}


void Dnp3ApplicationNew::startOutstation(Ptr<Socket> sock)
{
    NS_LOG_INFO("Starting Outstation");

    std::cout << "Starting Outstation" << std::endl;

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
    outstationConfig.userNum             = 4; //3; // hard coded for prototype
    outstationConfig.debugLevel_p        = &debugLevel;
    
    std::cout << "PLEASE GET HERE!!!!!!!!!!!!!!!!!!!! " << std::endl;
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
    std::cout << "HHHHHHHHHHHHHHHHHHHHHHH" << std::endl;
    // datalink required pointer to the transmit interface
    dlConfig.tx_p  = ep_p;
    std::cout << "before init" << std::endl;
    initConfig();
    std::cout << "hhhhhhhhhhhhhhhhh" << std::endl;
    //TimerInterface* ti = NULL; //Todo
    NS_LOG_UNCOND("I AM INITIALIZING AN OUTSTATION");
    o_p = new Outstation ( outstationConfig, dlConfig,
			   this,   // event interface
			   &ti);   // timer interface
    NS_LOG_UNCOND("I AM HERE1");
    o_p->set_point_names(analog_point_names, binary_point_names);
    o_p->set_stationName(m_name);
    NS_LOG_UNCOND("I AM HERE2");
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    std::function<void(std::string,std::string,std::string)> func;
    func = std::bind (&Dnp3ApplicationNew::DoMessage, this, _1, _2, _3);
    std::cout << "binding" << std::endl;
    o_p->set_publishCallback(func);
    vector<string>::iterator it;
    m_respond = true;
    m_offline = false;
    //vector<string>::iterator it;
    //m_respond = true;
    //m_offline = false;
}

static uint8_t
char_to_uint8_t (char c)
{
  return uint8_t(c);
}

//This function provides a way to set an outstation to not respond to requests
void Dnp3ApplicationNew::set_respond(bool respond) {
    NS_LOG_INFO ("DNP3Application::setRespond");
    m_respond = respond;
}

//This function provides a way to set the values of an outstation to offline in DNP3 responses
void Dnp3ApplicationNew::set_offline(bool offline) {
    NS_LOG_INFO ("DNP3Application::set_offline");
    if (m_isMaster == true) {
      //cout << "Master HandleRead Recieved: " << temp << endl;
      NS_LOG_INFO ("Error: Tried to set a master DNP3 to offline; only valid for outstation DNP3 applications");
    } else {
      //cout << "Outstation HandleRead" << temp << endl;
      o_p->set_offline(offline);
      if(offline) {
          frozen_analog_points = analog_points;
          frozen_bin_points = bin_points;
      }
      m_offline = offline;
    }
}

//This function provides a way to set a multiplier for the analog values responded
void Dnp3ApplicationNew::set_multiplier(float multiplier) {
    NS_LOG_INFO ("DNP3Application::set_multiplier");
    if (m_isMaster == true) {
      //cout << "Master HandleRead Recieved: " << temp << endl;
      NS_LOG_INFO ("Error: Tried to set a master DNP3 to offline; only valid for outstation DNP3 applications");
    } else {
      //cout << "Outstation HandleRead" << temp << endl;
      o_p->set_multiplier(multiplier);
    }
}

void
Dnp3ApplicationNew::HandleRead (Ptr<Socket> socket)
{
  cout << "I am in handle read :)" << endl;
  cout << m_name << endl;
  if (m_name.find("MIM") != std::string::npos) {
      handle_MIM(socket);
  }else {
      handle_normal(socket);
  } 
}

/*
void Dnp3ApplicationNew::handle_inside(Ptr<Socket> socket) {
	cout << "I AM AN INSIDER!!!!!!!!" << endl;
	Address from;
	Ptr<Packet> packet;
	Address SourceAddr;
	socket->GetSockName(SourceAddr);
	Uptime_t timeRxd = 0;
	while ((packet = socket->RecvFrom (from)))
	{
		m_txTrace(packet);
		std::cout << "This is where the data is coming from" << std::endl;
		std::cout << SourceAddr << std::endl;
		if(mitm_flag == true) {
		       Json::Value configObject;
                       std::map<std::string, std::string> attack;
                       if (configFile.find("NA") == std::string::npos){
                            readMicroGridConfig(configFile, configObject);
                            for (uint32_t j = 1; j < configObject["MIM"].size(); j++){
                                for(const auto& item : configObject["MIM"][j].getMemberNames() ){
                                        std::string ID = "MIM-"+std::to_string(j)+"-"+item;
                                        std::string my_str = configObject["MIM"][j][item].asString();

                                        my_str.erase(remove(my_str.begin(), my_str.end(), '"'), my_str.end());
                                        std::cout << "This is the keys value: " << ID << "  and the value is " << my_str << std::endl;
                                        attack.insert(pair<std::string,std::string >(ID, my_str));
                                }

                            }
                        }
			if(m_attack_on) {
				if(m_attackType == 1) {
					NS_LOG_INFO ("MIMServer::HandleRead >>> Attack is ON. Routing process is terminated by Man in the middle");
				} else if(m_attackType == 2) {
					NS_LOG_INFO ("MIMServer::HandleRead >>> Attack is ON. Sending 0 payload by Man in the middle");
					uint32_t size = packet->GetSize();
					
					uint8_t *temp = new uint8_t[size];
					packet->CopyData(temp, size);
					std::cout << "HandleRead Insider" << temp << std::endl;
					Bytes buf((unsigned char*) temp, (unsigned char*)temp+size);
					Bytes emptyData;
					Ptr<Packet> packet_mitm;
					if(buf.size() >= Lpdu::HEADER_SIZE) {
						int start1 = buf[0];
						AppSeqNum_t seq = buf[11] & 0x0f;
						NS_LOG_INFO (seq);
						if((start1 == 0x05) && (0x64)) {
							uint16_t dest = buf[5] << 0x08 | buf[4]; //To check
							uint16_t src = buf[7] << 0x08 | buf[6]; //To check
							Lpdu::UserData data;
							data.dest = dest;
							data.src = src;
                                                        std::cout << "Dest: " << data.dest << " src: " << data.src << std::endl;
						        if (m_isMaster) {
								 NS_LOG_INFO("Am I the master----------------");
							} else {
								NS_LOG_INFO("sending false data to the controller");
								if (data.dest == 2){
							            m_p->sendEmptyPollRequest(data, seq); 
								    uint16_t temp = data.dest;
								    data.dest = data.src;
								    data.src = temp;
								//}
								//if (data.dest == 1){	
								  o_p->transmitZeroResponse(data, analog_points, bin_points, seq);
								}
						        }	    
						}
					}
					
				} else {
					
					NS_LOG_INFO ("You input a wrong value.");
				
				}
				
			} else {
				uint32_t size = packet->GetSize();
            			uint8_t *temp = new uint8_t[size];
				packet->CopyData(temp, size);
				std::cout << "HandleRead Insider" << temp << std::endl;
				Bytes buf((unsigned char*) temp, (unsigned char*)temp+size);
                                uint16_t dest = buf[5] << 0x08 | buf[4]; //To check
				uint16_t src = buf[7] << 0x08 | buf[6];
				if (dest == 1){
				    send_directly(packet);
				}
				if (dest == 2){
				    send_directly_server(packet);
				}
				
			}

         	} else {
			NS_LOG_INFO ("MIMServer::HandleRead >>> Man in the middle node is the final destination.");
			
		}
	}
	count += 1;
	
}
*/
void Dnp3ApplicationNew::send_directly_server(Ptr<Packet> p)
{
   m_txTrace (p);
   int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

   if (Ipv4Address::IsMatchingType (m_remoteAddress2))
    {
        std::cout << "HEEEEEEEEERRRRRRRRRREEEEEEEEEEEEEE" << std::endl;
        std::cout << "Remote addr: " << Ipv4Address::ConvertFrom(m_remoteAddress2) << " remote port " << m_localPort << std::endl;
        InetSocketAddress address = InetSocketAddress(Ipv4Address::ConvertFrom(m_remoteAddress2), m_localPort);
        if (m_enableTcp) {
            int (Socket::*fp)(Ptr<Packet>, uint32_t)  = &Socket::Send;
            Simulator::Schedule(NanoSeconds (delay_ns), fp, mim_socket, p, 0); //virtual method
	} else {
		int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
		Simulator::Schedule(NanoSeconds (delay_ns), fp, mim_socket, p, 0, address); //virtual method
	}
     }
    else if (Ipv6Address::IsMatchingType (m_remoteAddress2))
     {
       Inet6SocketAddress address = Inet6SocketAddress(Ipv6Address::ConvertFrom(m_remoteAddress2), m_localPort);
       if (m_enableTcp) {
          int (Socket::*fp)(Ptr<Packet>, uint32_t)  = &Socket::Send;
          Simulator::Schedule(NanoSeconds (delay_ns), fp, mim_socket, p, 0); //virtual method
        } else {
          int (Socket::*fp)(Ptr<Packet>, uint32_t, const Address&) = &Socket::SendTo;
          Simulator::Schedule(NanoSeconds (delay_ns), fp, mim_socket, p, 0, address); //virtual method
        }
      }
}

void 
Dnp3ApplicationNew::save_data(Ptr<Socket> socket, Ptr<Packet> packet, Address from)
{
  Address localAddress;
  socket->GetSockName (localAddress);
  if (packet->GetSize () > 0)
  {
    uint32_t receivedSize = packet->GetSize ();
    SeqTsHeader seqTs;
    packet->RemoveHeader (seqTs);
    uint32_t currentSequenceNumber = seqTs.GetSeq ();
    std::ofstream outfile;
    if(not (access( "perf.txt", F_OK ) == 0)){
       outfile.open("perf.txt", std::ios_base::app);
       outfile << "Timestamp : Bytes Received : From IP : Node ID : Sequence Number : Packet UID\n"; // : Delay\n";
       outfile.close();
     }

     outfile.open("perf.txt", std::ios_base::app); // append instead of overwrite
     outfile  << Simulator::Now () <<
     " : " << packet->GetSize () <<
     " : " << InetSocketAddress::ConvertFrom (from).GetIpv4 () <<
     " : " << m_remoteAddress <<
     " : " << currentSequenceNumber <<
     " : " << packet->GetUid () << "\n";
     //" : " << Simulator::Now () - seqTs.GetTs () << "\n";		    
     outfile.close();
  }

}

void Dnp3ApplicationNew::handle_normal(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet;
    Uptime_t timeRxd = 0;
    NS_LOG_LOGIC("I AM IN HANDLE NORMAL");

    while ((packet = socket->RecvFrom (from)))
    {
        m_txTrace(packet);
	
        NS_LOG_INFO ("DNP3Application:: >>> address: " << InetSocketAddress::ConvertFrom (from).GetIpv4 ()); // << "   VictimAddress: " << victimAddr);
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
	  cout << m_name << endl;
          if(m_respond) {
              if(!m_offline) state = o_p->rxData(&buf, analog_points, bin_points, timeRxd);
              else state = o_p->rxData(&buf, frozen_analog_points, frozen_bin_points, timeRxd);
	  }

        }
        if ( state != Station::IDLE) {
        }
	Record(packet, from);
    }
}

void Dnp3ApplicationNew::calc_crc(Bytes buf_temp, uint8_t * temp2, Ptr<Packet> testPack){
    static unsigned short crctable[256] = {
	    0x0000,  0x365e,  0x6cbc,  0x5ae2,  0xd978,  0xef26,  0xb5c4,  0x839a,
	    0xff89,  0xc9d7,  0x9335,  0xa56b,  0x26f1,  0x10af,  0x4a4d,  0x7c13,
	    0xb26b,  0x8435,  0xded7,  0xe889,  0x6b13,  0x5d4d,  0x07af,  0x31f1,
	    0x4de2,  0x7bbc,  0x215e,  0x1700,  0x949a,  0xa2c4,  0xf826,  0xce78,
	    0x29af,  0x1ff1,  0x4513,  0x734d,  0xf0d7,  0xc689,  0x9c6b,  0xaa35,
	    0xd626,  0xe078,  0xba9a,  0x8cc4,  0x0f5e,  0x3900,  0x63e2,  0x55bc,
	    0x9bc4,  0xad9a,  0xf778,  0xc126,  0x42bc,  0x74e2,  0x2e00,  0x185e,
	    0x644d,  0x5213,  0x08f1,  0x3eaf,  0xbd35,  0x8b6b,  0xd189,  0xe7d7,
	    0x535e,  0x6500,  0x3fe2,  0x09bc,  0x8a26,  0xbc78,  0xe69a,  0xd0c4,
	    0xacd7,  0x9a89,  0xc06b,  0xf635,  0x75af,  0x43f1,  0x1913,  0x2f4d,
	    0xe135,  0xd76b,  0x8d89,  0xbbd7,  0x384d,  0x0e13,  0x54f1,  0x62af,
	    0x1ebc,  0x28e2,  0x7200,  0x445e,  0xc7c4,  0xf19a,  0xab78,  0x9d26,
	    0x7af1,  0x4caf,  0x164d,  0x2013,  0xa389,  0x95d7,  0xcf35,  0xf96b,
	    0x8578,  0xb326,  0xe9c4,  0xdf9a,  0x5c00,  0x6a5e,  0x30bc,  0x06e2,
	    0xc89a,  0xfec4,  0xa426,  0x9278,  0x11e2,  0x27bc,  0x7d5e,  0x4b00,
	    0x3713,  0x014d,  0x5baf,  0x6df1,  0xee6b,  0xd835,  0x82d7,  0xb489,
	    0xa6bc,  0x90e2,  0xca00,  0xfc5e,  0x7fc4,  0x499a,  0x1378,  0x2526,
	    0x5935,  0x6f6b,  0x3589,  0x03d7,  0x804d,  0xb613,  0xecf1,  0xdaaf,
	    0x14d7,  0x2289,  0x786b,  0x4e35,  0xcdaf,  0xfbf1,  0xa113,  0x974d,
	    0xeb5e,  0xdd00,  0x87e2,  0xb1bc,  0x3226,  0x0478,  0x5e9a,  0x68c4,
	    0x8f13,  0xb94d,  0xe3af,  0xd5f1,  0x566b,  0x6035,  0x3ad7,  0x0c89,
	    0x709a,  0x46c4,  0x1c26,  0x2a78,  0xa9e2,  0x9fbc,  0xc55e,  0xf300,
	    0x3d78,  0x0b26,  0x51c4,  0x679a,  0xe400,  0xd25e,  0x88bc,  0xbee2,
	    0xc2f1,  0xf4af,  0xae4d,  0x9813,  0x1b89,  0x2dd7,  0x7735,  0x416b,
	    0xf5e2,  0xc3bc,  0x995e,  0xaf00,  0x2c9a,  0x1ac4,  0x4026,  0x7678,
	    0x0a6b,  0x3c35,  0x66d7,  0x5089,  0xd313,  0xe54d,  0xbfaf,  0x89f1,
	    0x4789,  0x71d7,  0x2b35,  0x1d6b,  0x9ef1,  0xa8af,  0xf24d,  0xc413,
	    0xb800,  0x8e5e,  0xd4bc,  0xe2e2,  0x6178,  0x5726,  0x0dc4,  0x3b9a,
	    0xdc4d,  0xea13,  0xb0f1,  0x86af,  0x0535,  0x336b,  0x6989,  0x5fd7,
	    0x23c4,  0x159a,  0x4f78,  0x7926,  0xfabc,  0xcce2,  0x9600,  0xa05e,
	    0x6e26,  0x5878,  0x029a,  0x34c4,  0xb75e,  0x8100,  0xdbe2,  0xedbc,
	    0x91af,  0xa7f1,  0xfd13,  0xcb4d,  0x48d7,  0x7e89,  0x246b,  0x1235
    };

    unsigned int crc32 = 0;
    int data_length = 8; //274;
    Bytes::const_iterator j = buf_temp.begin();
    int i;
    for (i = 0; i < data_length; i++, j++) {
	    std::cout << "I am in here 1" << std::endl;
	    const uint16_t lookupIndex = (crc32^(*j)) & 0x00ff;
	    crc32 = (crc32 >> 8) ^ crctable[lookupIndex];  // CRCTable is an array of 256 32-bit constants
    }
    
    crc32 = (~crc32 & 0xffff);
    Bytes crc3;
    appendUINT16(crc3, crc32);
    std::cout << "After the append" << std::endl;
    int crc1 = crc3[0]; //(crc32) & 0xff;
    int crc2 = crc3[1]; //(crc32 >> (8*1)) & 0xff;
    
    std::cout << "The calculated crc is "  << crc1 << " " << crc2 << " the observed crc is " << (int)buf_temp[8] << " and " << (int) buf_temp[9] << std::endl;
    temp2[8] = crc1;
    temp2[9] = crc2;
    
    int remainder = (buf_temp.size()-10)%18;
    Bytes crc3_temp;
    unsigned int crc32_temp = 0;
    Bytes::const_iterator jj = j;
    int index1 = 26;
    int index2 = 27;
    for (int ss = 0; ss < buf_temp.size()-(remainder+10); ss+=18){ //(buf.size()-(remainder+10)); ss++){
	    if((int)buf_temp[index1] != 0){
		    crc32_temp = 0;
		    jj = jj + 2;
		    
		    for (i = 0; i < 16; i++, jj++){
			    const uint16_t lookupIndex = (crc32_temp^(*jj)) & 0x00ff;
			    crc32_temp = (crc32_temp >> 8) ^ crctable[lookupIndex];
		    } 
		    crc32_temp = (~crc32_temp & 0xffff);
		    
		    appendUINT16(crc3_temp, crc32_temp);
		    
		    int crc1_temp = crc3_temp[crc3_temp.size()-2];
		    int crc2_temp = crc3_temp[crc3_temp.size()-1];
		    
		    std::cout << "The calculated crc is " << crc1_temp << " " << crc2_temp << " the observed crc is " << (int)buf_temp[index1] << " and " << (int)buf_temp[index2] << std::endl;
		    
		    temp2[index1] = crc1_temp;
		    temp2[index2] = crc2_temp;
		    
		    index1 = index1 + 18;
		    index2 = index2 + 18;
		    crc3_temp.clear();
	    }
    }
    std::cout << "The remainder is equal to " << remainder-2 << std::endl;
    if( testPack->GetSize() == 188 ){
	    crc32_temp = 0;
	    jj = jj+2;
	    
	    for (i = 0; i < remainder-2; i++, jj++){
		    const uint16_t lookupIndex = (crc32_temp^(*jj)) & 0x00ff;
		    crc32_temp = (crc32_temp >> 8) ^ crctable[lookupIndex];
	    }
	    crc32_temp = (~crc32_temp & 0xffff);
	    
	    appendUINT16(crc3_temp, crc32_temp);
	    
	    int crc1_temp = crc3_temp[crc3_temp.size()-2];
	    int crc2_temp = crc3_temp[crc3_temp.size()-1];
	    
	    std::cout << "The calculated crc is 11111 " << crc1_temp << " " << crc2_temp << std::endl;
	    temp2[index1-(18-remainder)] = crc1_temp;
	    temp2[index2-(18-remainder)] = crc2_temp;
	    crc3_temp.clear();
    }
    
    cout << "Captured Response: " << testPack->GetSize() << endl;
}

std::vector<std::string> Dnp3ApplicationNew::get_val_vector (std::string delimiter, std::string m_attack_val){
    size_t pos = 0;
    std::vector<std::string> val;
    std::string token;
    std::string VI = m_attack_val;
    while ((pos = VI.find(delimiter)) != std::string::npos) {
	    token = VI.substr(0, pos);
	    val.push_back(token);
	    VI.erase(0, pos + delimiter.length());
    }
    val.push_back(VI);
    return val;
}

int Dnp3ApplicationNew::start_byte(uint8_t * temp2, Ptr<Packet> testPack){
    int start;
    if ((int)temp2[15] == 30 && (int)temp2[16] == 5){
	    start = 0;
    }else if(testPack->GetSize() == 274){
	    start = 46;
    }else{
	    start = 92;
    }
    return start;
}

float Dnp3ApplicationNew::get_val(std::vector<std::string> val, std::vector<std::string> val_min, std::vector<std::string> val_max, int index){
    float f = 0.0;
    if (index < val_min.size() && index < val_max.size()){
        if (!val_max[index].empty() && !val_min[index].empty() && std::find_if(val_max[index].begin(), val_max[index].end(), [](unsigned char c) { return !std::isdigit(c); }) == val_max[index].end() && std::find_if(val_min[index].begin(),val_min[index].end(), [](unsigned char c) { return !std::isdigit(c); }) == val_min[index].end()){//val_min[index] != "NA" && val_max[index] != "NA"){
	    float r = (rand() % 10) + 1;//rand() / (RAND_MAX);
	    std::cout << "The random number that is selected is " << r << std::endl;
	    if (r > 5){
		    f = (std::stof(val_min[index])); 
	    }else{
		    f = (std::stof(val_max[index])); 
	    }
        }else{
	    std::cout << "The value is: " << val[index] << std::endl;
	    f = (std::stof(val[index])); 
        }
    }else{
        f = (std::stof(val[index]));
    }
    return f;

}

void Dnp3ApplicationNew::handle_MIM(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet;
    Address SourceAddr;
    socket->GetSockName(SourceAddr);

    while ((packet = socket->RecvFrom(from))) {
        m_txTrace(packet);
        std::cout << "MIMServer::HandleRead >>> Processing packet at time " << Simulator::Now().GetSeconds() << "s. MIM IP: " << SourceAddr << std::endl;

        // Extract packet data
        uint32_t size = packet->GetSize();
        uint8_t* temp = new uint8_t[size];
        packet->CopyData(temp, size);
        Bytes buf((unsigned char*)temp, (unsigned char*)temp + size);

        // Copy packet for detailed processing
        Ptr<Packet> testPack = packet->Copy();
        uint32_t size2 = testPack->GetSize();
        uint8_t* temp2 = new uint8_t[size2];
        testPack->CopyData(temp2, size2);
        Bytes buf2((unsigned char*)temp2, (unsigned char*)temp2 + size2);

        // Extract header information if packet size is sufficient
        int start1 = 0;
        AppSeqNum_t seq = 0;
        if (buf.size() >= Lpdu::HEADER_SIZE) {
            start1 = buf[0];
            seq = buf[11] & 0x0f;
            // Cast seq to int to avoid non-printable character display issues
            std::cout << "Packet header processed. Start: " << start1 << ", Sequence: " << static_cast<int>(seq) << std::endl;
        }

        // Parse attack configuration and point data
        std::string delimiter = ",";
        std::vector<std::string> val = get_val_vector(delimiter, m_attack_point_val);
        std::vector<std::string> val_min = get_val_vector(delimiter, m_attack_min);
        std::vector<std::string> val_max = get_val_vector(delimiter, m_attack_max);
        std::vector<std::string> nodes = get_val_vector(delimiter, node_id);
        std::vector<std::string> points = get_val_vector(delimiter, point_id);
        std::vector<std::string> real_val = get_val_vector(delimiter, RealVal);

        // Build node-point mappings
        std::vector<std::string> nodesPoints;
        for (size_t xx = 0; xx < nodes.size(); xx++) {
            nodesPoints.push_back(nodes[xx] + "$" + points[xx]);
        }

        // Map node-points to IDs for analog and binary points
        std::vector<int> ID_point;
	std::vector<std::string> pointID;
        for (const auto& nodePoint : nodesPoints) {
            bool found = false;
            for (size_t i = 0; i < analog_point_names.size(); i++) {
                if (analog_point_names[i].find(nodePoint) != std::string::npos) {
                    ID_point.push_back(i);
                    pointID.push_back(nodePoint);
                    found = true;
                    std::cout << "Mapped point " << nodePoint << " to analog ID " << i << " (name: " << analog_point_names[i] << ")" << std::endl;
                    break;
                }
            }
            if (!found) {
                for (size_t i = 0; i < binary_point_names.size(); i++) {
                    if (binary_point_names[i].find(nodePoint) != std::string::npos) {
                        ID_point.push_back(i);
			pointID.push_back(nodePoint);
                        std::cout << "Mapped point " << nodePoint << " to binary ID " << i << " (name: " << binary_point_names[i] << ")" << std::endl;
                        if (binary_point_names[i].find("sw") != std::string::npos || binary_point_names[i].find("switch") != std::string::npos) {
                            std::cout << "Note: Point ID " << i << " appears to be a switch, likely controllable in GridLAB-D." << std::endl;
			    found = true;
                        } else {
                            std::cout << "Warning: Point ID " << i << " may not be a controllable switch in GridLAB-D. Attack may fail if not supported." << std::endl;
                        }
                        break;
                    }
                }
            }
            if (!found) {
                std::cout << "Warning: Could not map point " << nodePoint << " to any ID in analog or binary point names. Attack may fail for this point." << std::endl;
            }
        }
        std::cout << "Mapped " << ID_point.size() << " points for attack processing." << std::endl;

        // Handle MITM attack if flag is active
        if (mitm_flag) {
            Json::Value configObject;
            std::map<std::string, std::string> attack;
            if (configFile.find("NA") == std::string::npos && !ID_point.empty()) {
                readMicroGridConfig(configFile, configObject);
                for (uint32_t j = 1; j < configObject["MIM"].size(); j++) {
                    for (const auto& item : configObject["MIM"][j].getMemberNames()) {
                        std::string ID = "MIM-" + std::to_string(j) + "-" + item;
                        std::string my_str = configObject["MIM"][j][item].asString();
                        my_str.erase(remove(my_str.begin(), my_str.end(), '"'), my_str.end());
                        attack.insert({ID, my_str});
                    }
                }
            }

            uint16_t dest = (buf.size() >= 6) ? (buf[5] << 0x08 | buf[4]) : 0;
            uint16_t src = (buf.size() >= 8) ? (buf[7] << 0x08 | buf[6]) : 0;

            // Initialize logging stream for attack status
            std::stringstream netStatsOut;
            bool hasLogData = false; // Flag to check if there's data to write to CSV

            std::vector<float> attackChance = GetVal(attack, "attack_chance");
            std::cout << "Retrieved " << attackChance.size() << " attack_chance values." << std::endl;
            for (size_t i = 0; i < attackChance.size(); i++) {
                std::cout << "attack_chance[" << i << "] = " << attackChance[i] << std::endl;
            }

            std::vector<float> start = GetVal(attack, "PointStart");
            std::cout << "Retrieved " << start.size() << " PointStart values." << std::endl;
            for (size_t i = 0; i < start.size(); i++) {
                std::cout << "PointStart[" << i << "] = " << start[i] << std::endl;
            }

            std::vector<float> stop = GetVal(attack, "PointStop");
            std::cout << "Retrieved " << stop.size() << " PointStop values." << std::endl;
            for (size_t i = 0; i < stop.size(); i++) {
                std::cout << "PointStop[" << i << "] = " << stop[i] << std::endl;
            }

            std::vector<float> attackType = GetVal(attack, "attack_type");
            std::cout << "Retrieved " << attackType.size() << " attack_type values." << std::endl;
            for (size_t i = 0; i < attackType.size(); i++) {
                std::cout << "attack_type[" << i << "] = " << attackType[i] << std::endl;
            }

	    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	    bool firstStat = false;
	    int num_point = 7;
	    int cur_point = 0;
            if (m_attack_on && start1 == 0x05 && 0x64 ) {
                Lpdu::UserData data;
                data.dest = dest;
                data.src = src;
                std::cout << "Attack active at time " << Simulator::Now().GetSeconds() << "s. Dest: " << data.dest << ", Src: " << data.src << std::endl;

                for (size_t qq = 0; qq < ID_point.size(); qq++) {
                    double currentTime = Simulator::Now().GetSeconds();
                    // float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                    // Ensure attackChance, start, and stop have enough entries
		    if (firstStat == true && qq > cur_point + num_point){
                       firstStat = false;
		       cur_point = 0;
		    }
		    if (pointID[qq].find("status") != std::string::npos and firstStat == false){
                        firstStat = true;
			cur_point = qq;
                        if (pointID[qq+1].find("phase") == std::string::npos){
                            firstStat = false;
			    cur_point = 0;
		        }
		    }
		    if (firstStat == false or qq > cur_point + num_point){
                        r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		    }

                    float chance = attackChance[qq]; //(qq < attackChance.size()) ? attackChance[qq] : 0.0f;
                    float startTime = start[qq]; //(qq < start.size()) ? start[qq] : 0.0f;
                    float stopTime = stop[qq]; // (qq < stop.size()) ? stop[qq] : 0.0f;
                    float type = attackType[qq]; //(qq < attackType.size()) ? attackType[qq] : 0.0f;

                    if (currentTime > startTime && currentTime < stopTime && chance > r) {
                        netStatsOut << currentTime << " " << ID_point[qq] << " SUCCESS " << type << std::endl;
                        hasLogData = true;
                        int attackTypeInt = static_cast<int>(type);
                        std::cout << "Applying attack type " << attackTypeInt << " on point " << ID_point[qq] << " at time " << currentTime << "s" << std::endl;

                        if (attackTypeInt == 2 && (testPack->GetSize() == 274 || testPack->GetSize() >= 195)) {
                            std::cout << "Attack Type 2: Modifying payload for point " << ID_point[qq] << std::endl;
                            float f = get_val(val, val_min, val_max, qq);
                            std::cout << "Attack value set to " << f << " for point " << ID_point[qq] << std::endl;
                            char* cc = (char*)&f;
                            int d = 0, four = 0, ind1 = 0, ind2 = 0;

                            four = start_byte(temp2, testPack);
                            for (size_t i = 0; i < buf2.size(); i++) {
                                if (i > 9) {
                                    if (i > 19) {
                                        if (temp2[i] != 0x01 && ind2 < 16) ind1++;
                                        if (temp2[i] == 0x01) four++;
                                        if (ind2 == 0) ind1 = 0;
                                    }
                                    ind2 = (ind2 < 18) ? ind2 + 1 : 0;
                                }
                                if (ind2 == 0 && i > 19) {
                                    std::cout << "Found data CRC check at index " << ind1 << " for point ID " << ID_point[qq] << std::endl;
                                }
                                if (four > 0 && four == ID_point[qq] && ind2 <= 18 && i > 19 && temp2[i] != 0x01) {
                                    temp2[i] = cc[d++];
                                    std::cout << "Modified byte at index " << i << " to value " << (int)temp2[i] << std::endl;
                                }
                            }
                            Bytes buf_temp;
                            for (size_t i = 0; i < buf2.size(); i++) {
                                appendUINT8(buf_temp, temp2[i]);
                            }
                            calc_crc(buf_temp, temp2, testPack);
                            Ptr<Packet> newPack = Create<Packet>(temp2, testPack->GetSize());
                            std::cout << "Sending modified packet for attack type 2 on point " << ID_point[qq] << " at time " << currentTime << "s" << std::endl;
                            send_directly(newPack);
                        } else if (attackTypeInt == 3) {
                            std::cout << "Attack Type 3: Sending control command for point " << ID_point[qq] << std::endl;
                            ControlOutputRelayBlock::Code code;
                            std::string commandStr;
                            if (val[qq].find("TRIP") != std::string::npos) {
                                code = ControlOutputRelayBlock::Code::TRIP;
                                commandStr = "TRIP";
                            } else if (val[qq].find("CLOSE") != std::string::npos) {
                                code = ControlOutputRelayBlock::Code::CLOSE;
                                commandStr = "CLOSE";
                            } else if (val[qq].find("LATCH_ON") != std::string::npos) {
                                code = ControlOutputRelayBlock::Code::LATCH_ON;
                                commandStr = "LATCH_ON";
                            } else if (val[qq].find("LATCH_OFF") != std::string::npos) {
                                code = ControlOutputRelayBlock::Code::LATCH_OFF;
                                commandStr = "LATCH_OFF";
                            } else {
                                std::cout << "Invalid command for attack type 3 on point " << ID_point[qq] << ". Skipping." << std::endl;
                                continue;
                            }
                            std::cout << "Command: " << commandStr << std::endl;
                            ControlOutputRelayBlock ao(code, ID_point[qq]);
                            m_p->direct_operate(false, ao);
                            std::cout << "Sent direct operate command for attack type 3 on point " << ID_point[qq] << " with command " << commandStr << " at time " << currentTime << "s. Check if GridLAB-D acknowledges this point and command." << std::endl;
                        } else if (attackTypeInt == 4) {
                            std::cout << "Attack Type 4: Sending false analog data for point " << ID_point[qq] << std::endl;
                            float f = get_val(val, val_min, val_max, qq);
                            std::cout << "Analog attack value: " << f << std::endl;
                            Bit32AnalogOutput ao(f * 1000, ID_point[qq]);
                            m_p->direct_operate(false, ao);
                            std::cout << "Sent direct operate analog value for attack type 4 on point " << ID_point[qq] << " at time " << currentTime << "s. Check if GridLAB-D acknowledges this point and value." << std::endl;
                        }
                    } else {
                        // Log reason for not applying attack
                        std::string status = (chance < r) ? "FAILED" : "NO ATTACK";
                        std::string reason = (currentTime <= startTime) ? "Before start time" : (currentTime >= stopTime) ? "After stop time" : "Chance failed";
                        netStatsOut << currentTime << " " << ID_point[qq] << " " << status << " " << type << std::endl;
                        hasLogData = true;
                        std::cout << "Attack not applied for point " << ID_point[qq] << " at time " << currentTime << "s. Status: " << status << ". Reason: " << reason << ". Scheduling reset." << std::endl;
                        // Delay reset slightly to ensure attack data (if any) is processed by GridLAB-D
			if (currentTime <= stopTime and currentTime >= startTime){
                            Simulator::Schedule(Seconds(0.1), &Dnp3ApplicationNew::resetToRealValue, this, ID_point[qq], real_val[qq]);
			}
                    }
                }

                // Forward packet if attack type does not modify data directly
                if (attackType.empty() || (static_cast<int>(attackType[0]) != 2 || (testPack->GetSize() != 274 && testPack->GetSize() < 195))) {
                    std::cout << "Forwarding unmodified packet at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
                    send_directly(packet);
                }
            } else {
                // Attack not active, log status for all points
                std::cout << "Attack not active at time " << Simulator::Now().GetSeconds() << "s (m_attack_on: " << (m_attack_on ? "true" : "false") << ", start1: " << start1 << "). Logging status for all points." << std::endl;
                double currentTime = Simulator::Now().GetSeconds();
                for (size_t qq = 0; qq < ID_point.size(); qq++) {
                    float type = (qq < attackType.size()) ? attackType[qq] : 0.0f;
                    netStatsOut << currentTime << " " << ID_point[qq] << " NO ATTACK " << type << std::endl;
                    hasLogData = true;
                    // Delay reset to ensure any prior attack data is processed by GridLAB-D
                    Simulator::Schedule(Seconds(0.1), &Dnp3ApplicationNew::resetToRealValue, this, ID_point[qq], real_val[qq]);
                }

                // Schedule attack start/stop if needed
                if (configFile.find("NA") == std::string::npos && !ID_point.empty()) {
                    if (std::find(StartVect.begin(), StartVect.end(), attack["MIM-" + std::to_string(MIM_ID) + "-Start"]) == StartVect.end()) {
                        Simulator::Schedule(Seconds(std::stoi(attack["MIM-" + std::to_string(MIM_ID) + "-Start"])), &Dnp3ApplicationNew::set_attack, this, true);
                        StartVect.push_back(attack["MIM-" + std::to_string(MIM_ID) + "-Start"]);
                        std::cout << "Scheduled attack start at " << attack["MIM-" + std::to_string(MIM_ID) + "-Start"] << "s" << std::endl;
                    }
                    if (std::find(StopVect.begin(), StopVect.end(), attack["MIM-" + std::to_string(MIM_ID) + "-End"]) == StopVect.end()) {
                        Simulator::Schedule(Seconds(std::stoi(attack["MIM-" + std::to_string(MIM_ID) + "-End"])), &Dnp3ApplicationNew::set_attack, this, false);
                        StopVect.push_back(attack["MIM-" + std::to_string(MIM_ID) + "-End"]);
                        std::cout << "Scheduled attack stop at " << attack["MIM-" + std::to_string(MIM_ID) + "-End"] << "s" << std::endl;
                    }
                }

                // Forward packet based on destination
                if (dest == 1) {
                    std::cout << "Forwarding packet directly at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
                    send_directly(packet);
                } else {
                    std::cout << "Forwarding packet to server at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
                    send_directly_server(packet);
                }
            }

            // Log attack statistics to CSV if there is data to write
            if (hasLogData) {
                std::string loc = std::getenv("RD2C");
                std::string out_csv = loc + "/integration/control/attack.csv";
                FILE* pFile = fopen(out_csv.c_str(), "a");
                if (pFile != nullptr) {
                    fprintf(pFile, netStatsOut.str().c_str());
                    fclose(pFile);
                    std::cout << "Logged attack stats to " << out_csv << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
                } else {
                    std::cout << "Failed to open " << out_csv << " for writing at time " << Simulator::Now().GetSeconds() << "s. Check path or permissions." << std::endl;
                }
            } else {
                std::cout << "No attack status data to log at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
            }
        } else {
            std::cout << "MIMServer::HandleRead >>> Man in the middle node is the final destination at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
        }

        // Clean up dynamically allocated memory
        delete[] temp;
        delete[] temp2;
    }
}

// Helper method to reset points to real values
void Dnp3ApplicationNew::resetToRealValue(int pointId, const std::string& realValue) {
    if (!realValue.empty()) {
        double currentTime = Simulator::Now().GetSeconds();
        if (std::find_if(realValue.begin(), realValue.end(), [](unsigned char c) { return !std::isdigit(c); }) == realValue.end()) {
            try {
                float f = std::stof(realValue);
                Bit32AnalogOutput ao(f * 1000, pointId);
                m_p->direct_operate(false, ao);
                std::cout << "Reset analog point " << pointId << " to value " << f << " at time " << currentTime << "s" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error resetting analog point " << pointId << ": " << e.what() << " at time " << currentTime << "s" << std::endl;
            }
        } else {
            ControlOutputRelayBlock::Code code;
            if (realValue.find("TRIP") != std::string::npos) {
                code = ControlOutputRelayBlock::Code::TRIP;
            } else if (realValue.find("CLOSE") != std::string::npos) {
                code = ControlOutputRelayBlock::Code::CLOSE;
            } else if (realValue.find("LATCH_ON") != std::string::npos) {
                code = ControlOutputRelayBlock::Code::LATCH_ON;
            } else if (realValue.find("LATCH_OFF") != std::string::npos) {
                code = ControlOutputRelayBlock::Code::LATCH_OFF;
            } else {
                std::cout << "Invalid real value for binary point " << pointId << ": " << realValue << " at time " << currentTime << "s" << std::endl;
                return;
            }
            ControlOutputRelayBlock ao(code, pointId);
            m_p->direct_operate(false, ao);
            std::cout << "Reset binary point " << pointId << " to state " << realValue << " at time " << currentTime << "s" << std::endl;
        }
    } else {
        std::cout << "No real value provided for point " << pointId << " to reset at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
    }
}

void Dnp3ApplicationNew::send_directly(Ptr<Packet> p)
{
  //NS_LOG_INFO ("MIMServer::HandleRead >>> Bypassing to destination: " << destAddr);
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

  if (Ipv4Address::IsMatchingType (m_remoteAddress))
    {
      std::cout << "HEEEEEEEEERRRRRRRRRREEEEEEEEEEEEEE" << std::endl;
      std::cout << "Remote addr: " << Ipv4Address::ConvertFrom(m_remoteAddress) << " remote port " << m_remotelPort << std::endl;
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

void
Dnp3ApplicationNew::Record (Ptr<Packet> packet, Address from)
{
 //NS_LOG_FUNCTION (this << socket);
 Address localAddress;
 if (packet)
 {
    m_rxTraces (packet);
    m_rxTraceWithAddresses (packet, from, localAddress);
    if (packet->GetSize () > 0)
    {
       uint32_t receivedSize = packet->GetSize ();
       std::ofstream outfile;
       if(not (access( "perf.txt", F_OK ) == 0)){
	     outfile.open("perf.txt", std::ios_base::app);
             outfile << "Timestamp : Bytes Received : From IP : Node ID : Packet UID\n";
             outfile.close();
        }

        outfile.open("perf.txt", std::ios_base::app); // append instead of overwrite
        outfile  << Simulator::Now () <<
        " : " << packet->GetSize () <<
        " : " << InetSocketAddress::ConvertFrom (from).GetIpv4 () <<
        " : " << m_remoteAddress <<
        " : " << packet->GetUid () << "\n";
        outfile.close();

      }
   }
}

void
Dnp3ApplicationNew::SetEndpointName (const std::string &name, bool is_global)
{
    NS_LOG_FUNCTION (this << name << is_global);
    SetName(name);
    if (is_global) {
         m_endpoint_id = helics_federate->registerGlobalEndpoint (name);
    }
    else {
         m_endpoint_id = helics_federate->registerEndpoint (name);
    }
    using std::placeholders::_1;
    using std::placeholders::_2;
    std::function<void(helics::Endpoint,helics::Time)> func;
    func = std::bind (&Dnp3ApplicationNew::EndpointCallback, this, _1, _2);
    helics_federate->setMessageNotificationCallback(m_endpoint_id, func);
}

void 
Dnp3ApplicationNew::EndpointCallback (helics::Endpoint id, helics::Time time)
{
	  NS_LOG_FUNCTION (this << m_name << id.getName() << time);
	  DoEndpoint (id, time);
}
 
void 
Dnp3ApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time)
{
	  NS_LOG_FUNCTION (this << id.getName() << time);
	  auto message = helics_federate->getMessage(id);
	  DoEndpoint (id, time, std::move (message));
}

void
Dnp3ApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message)
{
	  NS_LOG_FUNCTION (this << id.getName() << time << message->to_string());
	  NS_LOG_INFO("DOEndPoint");
          std::string text = message->data.to_string();
	  std::istringstream tifs(text);
	  Json::Reader datareader;
	  Json::Value parsedObject;
	  datareader.parse(tifs, parsedObject);
	  std::string delim = "$";

	  m_gld_federate_name = "GLD";
	  Json::Value root = parsedObject.isMember(m_gld_federate_name)? parsedObject[m_gld_federate_name]: parsedObject;
	  for (auto const& id : root.getMemberNames())
	  {
	     for (auto const& variable : root[id].getMemberNames())
	     {
		     if(root[id][variable].size() > 0){
		       for (auto const& var : root[id][variable].getMemberNames()){
		       //NS_LOG_INFO(root[id][variable][var]);
		       auto value = root[id][variable][var].asString();
		       if (variable=="status" || variable=="switchA" || variable=="switchB" || variable=="switchC" ||
		            variable=="phase_A_state" || variable=="phase_B_state" || variable=="phase_C_state") {
	  		     Store(id + delim + variable, value);
	  	       }else if (variable=="voltage_A" || variable=="voltage_B" || variable=="voltage_C" ||
		  	         variable=="current_out_A" || variable=="current_out_B" || variable=="current_out_C" ||
			         variable=="current_in_A" || variable=="current_in_B" || variable=="current_in_C" ||
			         variable=="VA_Out") {
			       float real,imag;
			       std::istringstream v_text_stream(value);
			       v_text_stream >> real >> imag;
			       Store(id + delim + variable + ".real", std::to_string(real));
			       Store(id + delim + variable + ".imag", std::to_string(imag));
		       }
		       else if (variable=="Pref" || variable=="Qref" || variable=="V_In" || 
	                       variable=="tap_A" || variable=="tap_B" || variable=="tap_C" || 
			       variable=="capacitor_A" || variable=="capacitor_B" || variable=="capacitor_C") {
			       float real;
			       std::istringstream v_text_stream(value);
			       v_text_stream >> real;
			       Store(id + delim + variable, std::to_string(real));
		       }
		       else {
		  	       NS_LOG_WARN("Unknown variable name " << variable);
		               Store(id + delim + variable, value);
		       }
		       }
		     }else{
			     //NS_LOG_INFO(root[id][variable]);
			     auto value = root[id][variable].asString();
			     if (variable=="status" || variable=="switchA" || variable=="switchB" || variable=="switchC" ||
					     variable=="phase_A_state" || variable=="phase_B_state" || variable=="phase_C_state") {
				     Store(id + delim + variable, value);
			     }else if (variable=="voltage_A" || variable=="voltage_B" || variable=="voltage_C" ||
					     variable=="current_out_A" || variable=="current_out_B" || variable=="current_out_C" ||
					     variable=="current_in_A" || variable=="current_in_B" || variable=="current_in_C" ||
					     variable=="VA_Out") {
				     float real,imag;
				     std::istringstream v_text_stream(value);
				     v_text_stream >> real >> imag;
				     Store(id + delim + variable + ".real", std::to_string(real));
				     Store(id + delim + variable + ".imag", std::to_string(imag));
			     }
			     else if (variable=="Pref" || variable=="Qref" || variable=="V_In" || 
					     variable=="tap_A" || variable=="tap_B" || variable=="tap_C" || 
					     variable=="capacitor_A" || variable=="capacitor_B" || variable=="capacitor_C") {
				     float real;
				     std::istringstream v_text_stream(value);
				     v_text_stream >> real;
				     Store(id + delim + variable, std::to_string(real));
			     }
			     else {
				     NS_LOG_WARN("Unknown variable name " << variable);
				     Store(id + delim + variable, value);
																		     		     }

		     }
	     }
	  }
}


void
Dnp3ApplicationNew::DoMessage (std::string target_endpoint, const std::string content, const std::string content_type)
{
	  NS_LOG_FUNCTION (this << target_endpoint << content);
	  std::string property_delimiter = "$";
	  std::vector<std::string> tokens = split2(target_endpoint, property_delimiter);

	  std::string property_name = tokens[1];

	  tokens = split2(tokens[0], "/");
	  std::string gld_obj = tokens[1];
		    
	  std::string new_target_endpoint = toEndpointName2(target_endpoint);
	  if (endsWith2(new_target_endpoint, ".imag") || endsWith2(new_target_endpoint,".real")) {
	     new_target_endpoint = new_target_endpoint.substr(0, new_target_endpoint.length() - 5);
	     property_name = property_name.substr(0, property_name.length() - 5);
	  }

	  auto msg = std::make_unique<helics::Message>();
	  std::string json_content;

	  if (content_type == "int") {
		  json_content = "{\"" + gld_obj + "\":{\"" + property_name + "\":" + content + "}}";
	  }
	  else if (content_type == "string") {
		  json_content = "{\"" + gld_obj + "\":{\"" + property_name + "\":\"" + content + "\"}}";
	  }
	  else {
		  json_content = content;
          }

	  msg->data = json_content;
	  msg->dest = m_gld_federate_name + "/" + new_target_endpoint;
	  msg->time = helics::Time::epsilon();
	  
	  DoRead(std::move(msg));
}
void
Dnp3ApplicationNew::DoRead (std::unique_ptr<helics::Message> message)
{
    NS_LOG_FUNCTION (this << message->to_string());
    NS_LOG_INFO ("sending message " << message->to_string() + " to " + message->dest);
    // Send message
    //m_endpoint_id = "mg1";
    helics_federate->sendMessage (m_endpoint_id, message->dest, message->data.data(), message->data.size());
    // helics_federate->sendMessage(m_endpoint_id, std::move(message));
}

} // Namespace ns3
