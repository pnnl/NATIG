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

#ifndef DNP3_APPLICATION_NEW_H
#define DNP3_APPLICATION_NEW_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/address-utils.h"
#include "ns3/simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/ptr.h"
#include "ns3/application.h"

#include <list>
#include <map>
#include "ns3/endpoint.hpp"
#include "ns3/station.hpp"
#include "ns3/common.hpp"
#include "ns3/master.hpp"
#include "ns3/lpdu.hpp"
#include "ns3/outstation.hpp"
#include "ns3/timer_interface.hpp"
#include "ns3/traced-callback.h"
#include "ns3/dummy_timer.hpp"
#include "ns3/event_interface.hpp"
#include "ns3/traced-callback.h"
#include "ns3/helics-application.h"
#include "helics/helics.hpp"

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/writer.h>


namespace ns3 {

class Address;
class Socket;
class Packet;

/**
 * \ingroup applications
 * \defgroup packetsink PacketSink
 *
 * This application was written to complement OnOffApplication, but it
 * is more general so a PacketSink name was selected.  Functionally it is
 * important to use in multicast situations, so that reception of the layer-2
 * multicast frames of interest are enabled, but it is also useful for
 * unicast as an example of how you can write something simple to receive
 * packets at the application layer.  Also, if an IP stack generates
 * ICMP Port Unreachable errors, receiving applications will be needed.
 */

/**
 * \ingroup packetsink
 *
 * \brief Receive and consume traffic generated to an IP address and port
 *
 * This application was written to complement OnOffApplication, but it
 * is more general so a PacketSink name was selected.  Functionally it is
 * important to use in multicast situations, so that reception of the layer-2
 * multicast frames of interest are enabled, but it is also useful for
 * unicast as an example of how you can write something simple to receive
 * packets at the application layer.  Also, if an IP stack generates
 * ICMP Port Unreachable errors, receiving applications will be needed.
 *
 * The constructor specifies the Address (IP address and port) and the
 * transport protocol to use.   A virtual Receive () method is installed
 * as a callback on the receiving socket.  By default, when logging is
 * enabled, it prints out the size of packets and their address.
 * A tracing source to Receive() is also available.
 */
class Dnp3ApplicationNew : public Application, public EventInterface
{
public:
    //Type of control for send
    enum ControlType  {
	SELECT_OPERATE                               = 0x00,
	DIRECT                                   = 0x01,
	DIRECT_NO_RESP                                 = 0x02
    };

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  Dnp3ApplicationNew ();

  virtual ~Dnp3ApplicationNew ();

  /**
   * \return the total bytes received in this sink app
   */
  uint32_t GetTotalRx () const;

  /**
   * \return pointer to listening socket
   */
  Ptr<Socket> GetListeningSocket (void) const;

  /**
   * \return list of pointers to accepted sockets
   */
  std::list<Ptr<Socket> > GetAcceptedSockets (void) const;

  /**
   * \brief set the name
   * \param name name
   */
  void SetName (const std::string &name);
  /**
   * \brief set the local address and port
   * \param ip local IPv4 address
   * \param port local port
   */
  void SetLocal (Ipv4Address ip, uint16_t port);
  /**
   * \brief set the local address and port
   * \param ip local IPv6 address
   * \param port local port
   */
  void SetLocal (Ipv6Address ip, uint16_t port);
  /**
   * \brief set the local address and port
   * \param ip local IP address
   * \param port local port
   */
  void SetLocal (Address ip, uint16_t port);

  void Store(std::string point, std::string value);

  // implementation of EventInterface
    void changePoint(DnpAddr_t addr, DnpIndex_t index,
                 PointType_t    pointType,
                 int value, DnpTime_t timestamp=0);

    void registerName(       DnpAddr_t      addr,
                 DnpIndex_t index,
                 EventInterface::PointType_t    pointType,
                 char*          name,
                 int initialValue );

    std::string GetName (void) const;
    Address m_localAddress; //!< Local address
    uint16_t m_localPort; //!< Local port
    Address m_remoteAddress; //!< Local address
    Address m_remoteAddress2;
    uint16_t m_remotelPort; //!< Local port
    uint16_t m_masterport; //port of the master
    std::string m_name = ""; //!< name of this application
    std::string f_name; //!< name of the output file
    std::string points_filename; //!< Name of input file that defines the points for this application
    bool m_isMaster; //Flag to indicate master or outstation
    bool running;
    int count = 0;
    int four = 0;
    bool mitm_flag = false;
    double m_attackChance = 0.0;
    double m_jitterMinNs; //!<minimum jitter delay time for packets sent via FNCS
    double m_jitterMaxNs; //!<maximum jitter delay time for packets sent via FNCS
    void send_control_binary(Dnp3ApplicationNew::ControlType type, DnpIndex_t index, ControlOutputRelayBlock::Code code);
    void send_control_analog(Dnp3ApplicationNew::ControlType type, DnpIndex_t index, double value);
    void send_device_attribute_request(DnpIndex_t index);
    void periodic_poll(int count);
    void attack_data(int freq);
    void GetStartStopArray();
    std::vector<float> GetVal(std::map<std::string, std::string> attack, std::string tag);
    void periodic_poll_Integrity(int pollRate); //create perodic integrity polls
    void periodic_poll_Binary(int pollRate); //create perodic binary polls
    void periodic_poll_Analog(int pollRate); //crete perodic analog polls
    void periodic_poll_Class(int pollRate); //create perodic class polls
    void set_respond(bool respond);
    void set_offline(bool offline);
    void set_multiplier(float multiplier);
    void calc_crc(Bytes buf_temp, uint8_t * temp2, Ptr<Packet> testPack);
    std::vector<std::string> get_val_vector (std::string delimiter, std::string m_attack_val);
    int start_byte(uint8_t * temp2, Ptr<Packet> testPack);
    float get_val(std::vector<std::string> val, std::vector<std::string> val_min, std::vector<std::string> val_max, int index);
    void SetEndpointName (const std::string &name, bool is_global);
    void EndpointCallback (helics::Endpoint id, helics::Time time);
protected:
   virtual void DoEndpoint (helics::Endpoint id, helics::Time time);
   virtual void DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message);
   virtual void DoRead (std::unique_ptr<helics::Message> message);
   virtual void DoMessage (std::string target_endpoint, const std::string content, const std::string content_type);

   std::string m_destination; //!< name of this application
   helics::Endpoint m_endpoint_id;
   std::string m_gld_federate_name; // helics federate name of GridLAB-D
   virtual void DoDispose (void);
private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop
  void startMaster (void);
  void startOutstation (Ptr<Socket> sock);
  /**
   * \brief Handle a packet received by the application
   * \param socket the receiving socket
   */
  void HandleRead (Ptr<Socket> socket);
  void Record (Ptr<Packet> packet, Address from);
  /**
   * \brief Handle an incoming connection
   * \param socket the incoming connection socket
   * \param from the address the connection is from
   */
  void HandleAccept (Ptr<Socket> socket, const Address& from);
  /**
   * \brief Handle an connection close
   * \param socket the connected socket
   */
  void HandlePeerClose (Ptr<Socket> socket);
  /**
   * \brief Handle an connection error
   * \param socket the connected socket
   */
  void HandlePeerError (Ptr<Socket> socket);

  void ConnectToPeer(Ptr<Socket> localSocket, uint16_t servPort);


  void store_points(std::string point, std::string value);
  void initConfig(void);
  void makeTcpConnection(void);
  void makeUdpConnection(void);
  void resetToRealValue(int pointId, const std::string& realValue);
  void save_data(Ptr<Socket> socket, Ptr<Packet> packet, Address from);
  //void handle_inside(Ptr<Socket> socket);
  void handle_MIM(Ptr<Socket> socket);
  void readMicroGridConfig(std::string fpath, Json::Value& configobj);
  void handle_normal(Ptr<Socket> socket);
  void set_attack(bool state);
  void send_directly(Ptr<Packet> packet);
  void send_directly_server(Ptr<Packet> packet);

  // In the case of TCP, each socket accept returns a new socket, so the
  // listening socket is stored separately from the accepted sockets
  Ptr<Socket>     m_socket;       //!< Listening socket
  Ptr<Socket> mim_socket;
  Ptr<Socket> ins_socket; 
  std::list<Ptr<Socket> > m_socketList; //!< the accepted sockets

  Address         m_local;        //!< Local address to bind to
  uint32_t        m_totalRx;      //!< Total bytes received
  TypeId          m_tid;          //!< Protocol TypeId

  /// Traced Callback: received packets, source address.
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
  int debugLevel;
  int respTimeout;
  int integrityPollInterval;

  uint16_t m_integrityInterval;
  uint16_t m_master_device_addr;
  uint16_t m_station_device_addr;
  Master::MasterConfig          masterConfig;
  Station::StationConfig        stationConfig;
  Datalink::DatalinkConfig      datalinkConfig;
  Endpoint::EndpointConfig      endpointConfig;
  Outstation::OutstationConfig  outstationConfig;
  RemoteDevice remoteDevice;
  std::map<uint16_t, RemoteDevice>  deviceMap;
  Master* m_p;
  Outstation* o_p;
  DummyTimer ti;
  bool m_enableTcp;
  bool m_connected;
  int m_input_select;
  int m_victim;
  Ptr<UniformRandomVariable> m_rand_delay_ns; //!<random value used to add jitter to packet transmission
  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet> > m_txTrace;
  TracedCallback<Ptr<const Packet> > m_rxTraces;
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;
  map<string, float> analog_points;
  map<string, uint16_t> bin_points;
  map<string, float> frozen_analog_points;
  map<string, uint16_t> frozen_bin_points;
  vector<string> binary_point_names;
  vector<string> analog_point_names;
  static const int MAX_LEN = 77;
  std::string node_id;
  std::string point_id;
  std::string m_attack_point_val;
  std::string m_attack_max;
  std::string m_attack_min;
  uint16_t m_attackType;
  uint16_t MIM_ID;
  std::string m_attackStartTime;
  std::string m_attackEndTime;
  std::vector<string> StartVect;
  std::vector<string> StopVect;
  std::string RealVal;
  std::string configFile; 
  bool m_attack_on;
  bool m_respond;
  bool m_offline;
};

} // namespace ns3

#endif /* DNP3_APPLICATION_H */
