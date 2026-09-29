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
 * GOOSE (IEC 61850-8-1) application for NATIG co-simulation.
 * Structurally mirrors mms-application-new.h/modbus-application-new.h
 * (name-keyed dataset storage, attack injection, HELICS integration) to
 * keep cross-protocol behavior comparable, but diverges deliberately in
 * two places real GOOSE is fundamentally different from MMS/Modbus/DNP3:
 *
 *  - Transport: GOOSE is one-to-many publish, not client-server
 *    request/response. This implementation uses ns-3 UDP multicast
 *    (not the TCP makeTcpConnection pattern the other three protocols
 *    share) -- chosen over true L2-only Ethernet multicast
 *    (ns3::PacketSocket, no IP header at all) specifically so GOOSE
 *    traffic stays visible to NATIG's existing Ipv4FlowClassifier-based
 *    flow-monitoring/IDS-feature pipeline, which every other protocol's
 *    detectability comparison also depends on.
 *  - Timing: real GOOSE retransmits in a "burst on change" pattern --
 *    fast repeats immediately after a state change, decaying to a slow
 *    heartbeat until the next change -- not a fixed poll interval like
 *    periodic_poll or MMS's attack_data. Modeled here with a
 *    (burst count, burst interval, heartbeat interval) triple rather
 *    than the full multi-stage T0/T1/T2/T3 series real GOOSE profiles
 *    define, since the research-relevant property is the distinguishable
 *    burst-vs-heartbeat traffic *shape*, not exact spec timing.
 *
 * "isMaster" is kept as the attribute name (for the shared,
 * protocol-agnostic topology code that sets it identically across
 * protocols) but reinterpreted here as "is this instance the
 * publisher" rather than client/master.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#ifndef GOOSE_APPLICATION_NEW_H
#define GOOSE_APPLICATION_NEW_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/node.h"
#include "ns3/traced-callback.h"
#include "ns3/address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/address-utils.h"
#include "ns3/simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"

#include <list>
#include <map>
#include <vector>
#include <string>
#include <utility>
#include <cstdint>
#include <iostream>

#include "ns3/helics-application.h"
#include "ns3/helics.h"
#include "helics/helics.hpp"

// jsoncpp's include path differs between build environments -- see
// mms-application-new.h's identical guard.
#if defined(__has_include)
  #if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
    #include <jsoncpp/json/forwards.h>
    #include <jsoncpp/json/writer.h>
  #else
    #include <json/json.h>
    #include <json/forwards.h>
    #include <json/writer.h>
  #endif
#else
  #include <json/json.h>
  #include <json/forwards.h>
  #include <json/writer.h>
#endif

namespace ns3 {

class Address;
class Socket;
class Packet;

/**
 * \ingroup applications
 * \brief GOOSE (IEC 61850-8-1) application supporting publisher and
 * subscriber roles over UDP multicast, with rogue-publisher attack
 * injection scaffolding matching the DNP3/Modbus/MMS applications for
 * cross-protocol detectability comparison.
 */

// GOOSE carries no request/response service codes at all -- a
// publisher just periodically (burst-then-heartbeat) sends its full
// current dataset. isException/exceptionCode/hasValue-style fields
// from MMS's PDU have no equivalent here; a subscriber that receives a
// malformed frame simply discards it (see DecodePDU's isMalformed).
struct GoosePDU {
  std::string goID;      // GOOSE control block reference -- identifies
                          // which publisher/dataset this is, the
                          // dataset-level analog of MMS's per-point
                          // object reference.
  uint32_t stNum = 0;     // state number: increments only when a
                          // dataset value actually changes.
  uint32_t sqNum = 0;     // sequence number: increments on every
                          // transmission (including heartbeat repeats
                          // of an unchanged state).
  std::vector<std::pair<std::string, float>> analogValues;
  std::vector<std::pair<std::string, bool>> binaryValues;
  bool isMalformed = false;
};

// Replaces DNP3/Modbus/MMS's per-role device config -- GOOSE has no
// unit id / object reference concept at the wire level, just the
// dataset itself, name-keyed exactly like MMS (see mms-application-new.h's
// design note on why this sidesteps Modbus's address-collision bug class).
struct GooseDeviceConfig {
  std::map<std::string, float> analogValues;
  std::map<std::string, bool>  binaryValues;
};

class GooseApplicationNew : public Application
{
public:
  static TypeId GetTypeId (void);

  GooseApplicationNew ();
  virtual ~GooseApplicationNew ();

  uint32_t GetTotalRx () const;
  Ptr<Socket> GetListeningSocket (void) const;

  void SetName (const std::string &name);
  void SetLocal (Ipv4Address ip, uint16_t port);
  void SetLocal (Ipv6Address ip, uint16_t port);
  void SetLocal (Address ip, uint16_t port);

  void Store (std::string point, std::string value);
  std::string GetName (void) const;

  // -- HELICS integration (mirrors DNP3/Modbus/MMS's endpoint/message handling) --
  void SetEndpointName (const std::string &name, bool is_global);
  void EndpointCallback (helics::Endpoint id, helics::Time time);

  // -- Point access, name-keyed directly into m_deviceConfig --
  void SetAnalogPoint (const std::string &pointName, float value);
  float GetAnalogPoint (const std::string &pointName) const;
  void SetBinaryPoint (const std::string &pointName, bool value);
  bool GetBinaryPoint (const std::string &pointName) const;

  Address m_localAddress;
  uint16_t m_localPort;
  Address m_multicastGroup;   // replaces RemoteAddress -- the GOOSE
                               // multicast group this publisher sends
                               // to / this subscriber joins.
  uint16_t m_multicastPort;    // replaces RemotePort
  Address m_multicastGroup2;   // rogue-publisher's own multicast send
                                // address, mirrors RemoteAddress2's
                                // insider/MIM role in the other protocols.
  std::string m_name = "";
  std::string f_name;
  std::string points_filename;
  bool m_isMaster; // publisher (true) / subscriber (false); name kept
                    // for parity with the shared, protocol-agnostic
                    // topology code that sets this attribute identically
                    // across protocols -- see file header note.
  bool running;
  double m_attackChance = 0.0;
  double m_jitterMinNs;
  double m_jitterMaxNs;

  std::string m_gooseId;

  // -- Publisher-side: force an immediate publish, e.g. after a --
  // -- HELICS-driven state change (see attack_data's role here) --
  void PublishNow (void);

  // -- Attack scaffolding: kept structurally identical in shape to --
  // -- DNP3/Modbus/MMS's equivalents so cross-protocol attack --
  // -- comparisons are valid, but triggered by a rogue publisher --
  // -- forging state (see handle_rogue_publish), not by intercepting --
  // -- and mutating a response like handle_MIM does. --
  void attack_data (int freq);
  void GetStartStopArray ();
  std::vector<float> GetVal (std::map<std::string, std::string> attack, std::string tag);
  void set_respond (bool respond);
  void set_offline (bool offline);

protected:
  virtual void DoEndpoint (helics::Endpoint id, helics::Time time);
  virtual void DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message);
  virtual void DoRead (std::unique_ptr<helics::Message> message);
  virtual void DoMessage (std::string target_endpoint, const std::string content, const std::string content_type);

  std::string m_destination;
  helics::Endpoint m_endpoint_id;
  std::string m_gld_federate_name;
  virtual void DoDispose (void);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void startPublisher (void);
  void startSubscriber (void);

  void HandleRead (Ptr<Socket> socket);
  void Record (Ptr<Packet> packet, Address from);

  void store_points (std::string point, std::string value);
  float apply_fdi (const std::string& name, float realValue);
  void initConfig (void);
  void makeMulticastConnection (void);
  void send_directly (Ptr<Packet> packet);

  // -- GOOSE PDU encode/decode (from-scratch; see file header note) --
  GoosePDU DecodePDU (Ptr<Packet> packet);
  Ptr<Packet> EncodePDU (const GoosePDU& pdu);

  // -- Retransmission scheduling: burst-then-heartbeat on change --
  // -- (replaces periodic_poll/attack_data's fixed-interval scheduling) --
  void schedulePublish (void);
  bool datasetChangedSinceLastPublish (void);
  void publishAndReschedule (void);

  // -- Rogue-publisher attack: a separate publisher instance (an --
  // -- "Insider" node, matching the shared topology's existing --
  // -- naming convention) forges a competing GOOSE frame with an --
  // -- advanced stNum/sqNum so subscribers accept it as the latest --
  // -- legitimate state -- structurally the GOOSE-appropriate --
  // -- replacement for handle_MIM's intercept-and-modify pattern, --
  // -- since there is no in-path position to intercept in a --
  // -- multicast topology. --
  void handle_rogue_publish (void);
  // Gives the rogue role its own periodic schedule (reusing `freq` in
  // ms directly) for standalone tests / any scenario with no live
  // HELICS federate to otherwise drive handle_rogue_publish via
  // DoEndpoint -- see attack_data's dispatch-by-role logic.
  void scheduleRoguePublish (int freq);
  void readMicroGridConfig (std::string fpath, Json::Value& configobj);
  void set_attack (bool state);
  std::vector<std::string> get_val_vector (std::string delimiter, std::string m_attack_val);
  float get_val (std::vector<std::string> val, std::vector<std::string> val_min, std::vector<std::string> val_max, int index);

  Ptr<Socket> m_socket;

  Address m_local;
  uint32_t m_totalRx;
  TypeId m_tid;

  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
  int debugLevel;
  int respTimeout;
  int integrityPollInterval;

  GooseDeviceConfig m_deviceConfig;

  // Snapshot compared against on each scheduling tick to detect a real
  // state change (drives stNum increment / burst-mode reset) --
  // conceptually similar to the frozen-snapshot mechanism the other
  // protocols use for offline mode, but consulted every tick here
  // rather than only while offline.
  GooseDeviceConfig m_lastPublishedConfig;

  // Snapshot of m_deviceConfig immediately after the CSV load in
  // initConfig(), used to restore real values once an FDI attack
  // window ends (set_attack(false)) -- this is the only baseline
  // that has ever existed for a point, since the HELICS pipeline
  // that would otherwise refresh these values live never fires
  // (see natig-v2 research notes).
  GooseDeviceConfig m_preAttackAnalogValues;

  bool m_connected;
  Ptr<UniformRandomVariable> m_rand_delay_ns;

  TracedCallback<Ptr<const Packet> > m_txTrace;
  TracedCallback<Ptr<const Packet> > m_rxTraces;
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;

  static const int MAX_LEN = 260; // kept for structural parity with Modbus/MMS; unused there too

  std::vector<std::string> analog_point_names;
  std::vector<std::string> binary_point_names;

  bool mitm_flag = false;
  bool fdi_flag = false; //Compromised-endpoint FDI: the real publisher fabricates its own readings, no rogue instance needed (see DNP3/Modbus/MMS's identical addition)

  // -- Attack state: mirrors DNP3/Modbus/MMS's naming exactly for mergeability --
  std::string node_id;
  std::string point_id;
  std::string m_attack_point_val;
  std::string m_attack_max;
  std::string m_attack_min;
  uint16_t m_attackType;
  uint16_t MIM_ID;
  uint16_t FDI_ID;
  std::string m_attackStartTime;
  std::string m_attackEndTime;
  std::vector<std::string> StartVect;
  std::vector<std::string> StopVect;
  std::string RealVal;
  std::string configFile;
  bool m_attack_on;
  bool m_respond;
  bool m_offline;

  // -- Retransmission timing state --
  uint32_t m_stNum = 0;
  uint32_t m_sqNum = 0;
  uint32_t m_burstRemaining = 0;
  uint32_t m_burstCount = 3;        // BurstCount attribute default
  double m_burstIntervalMs = 4.0;   // BurstIntervalMs attribute default
  double m_heartbeatIntervalMs = 2000.0; // HeartbeatIntervalMs attribute default
  // Deadband for analog change detection (see datasetChangedSinceLastPublish):
  // exact floating-point equality against continuously-varying real
  // telemetry (e.g. live power-flow voltage from GridLAB-D) is
  // essentially never true between consecutive checks, which would
  // keep every publisher in perpetual burst mode forever -- a real
  // retransmission-storm problem discovered while validating this
  // production topology against live data (confirmed via tracing:
  // publishes stayed ~4ms apart, the burst interval, indefinitely,
  // rather than settling to the heartbeat interval). Real GOOSE
  // deployments apply the same kind of deadband for exactly this
  // reason. Binary points are unaffected -- they still use exact
  // equality, since a breaker position change is a real discrete event.
  double m_deadbandPct = 0.005;  // DeadbandPct attribute default (0.5% of magnitude)
  double m_deadbandAbs = 0.01;   // DeadbandAbs attribute default (floor, for near-zero values)
  EventId m_publishEvent; // tracks the next scheduled publishAndReschedule()
                          // call so a HELICS-driven change can cancel and
                          // re-trigger it immediately instead of waiting
                          // out the current heartbeat/burst interval.
};

} // namespace ns3

#endif /* GOOSE_APPLICATION_NEW_H */
