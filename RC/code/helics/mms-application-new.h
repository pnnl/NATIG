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
 * MMS (IEC 61850 client-server) application for NATIG co-simulation.
 * Structurally mirrors modbus-application-new.h/dnp3-application-new.h
 * (socket handling, attack injection, HELICS integration) to keep
 * cross-protocol behavior comparable. Protocol framing is implemented
 * from scratch below -- a simplified, flattened wire format capturing
 * MMS's essential request/response (and unsolicited Report) shape, not
 * a full ISO 8823/8327/COTP presentation-layer stack, which would be a
 * large undertaking with no bearing on cross-protocol attack
 * detectability (the actual research question).
 *
 * Key structural difference from Modbus: real MMS GetDataValues/
 * SetDataValues services address data BY NAME (an object reference
 * string, e.g. "Node1$XCBR1.Pos.stVal"), not by a flat numeric
 * address. Modbus needed analog_name_to_address/binary_name_to_address
 * translation maps *because* Modbus can only address registers/coils
 * by number -- and that translation is exactly what caused a real bug
 * there (an analog point and a binary point silently sharing the same
 * numeric address). MMS doesn't need that translation layer at all:
 * the point name from the points CSV *is* the wire-level address, so
 * m_deviceConfig below is name-keyed directly and that whole bug class
 * doesn't apply here by construction.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#ifndef MMS_APPLICATION_NEW_H
#define MMS_APPLICATION_NEW_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/socket.h" // full Socket definition needed (not just the forward
                         // declaration below) -- see modbus-application-new.h's
                         // identical note; we call Socket methods directly
                         // (Bind, Connect, Listen, &Socket::Send, etc.)
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
// modbus-application-new.h's identical guard, and the fatal error this
// avoids (a hardcoded, environment-specific #include path was a real
// bug in ns3-modbus-helics-grid.cc's initial port).
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
 * \brief MMS (IEC 61850 client-server) application supporting client and
 * server roles, with MIM/attack injection scaffolding matching the
 * DNP3/Modbus applications for cross-protocol detectability comparison.
 */

// MMS services in scope for this pass. Scope chosen to give read+write
// parity for both analog and binary points (matching DNP3/Modbus's
// analog/binary poll+control coverage) PLUS unsolicited Report -- a
// real IEC 61850 capability (Report Control Blocks) that Modbus's
// strictly polled request/response model has no equivalent for at all
// (see ModbusApplicationNew::attack_data, a permanent no-op stub).
// Real MMS/IEC 61850 also defines GetNameList, GetVariableAccessAttributes,
// file services, control-block configuration, etc.; not in scope here,
// same "don't implement the full spec, only what the research needs"
// precedent Modbus set for its function-code coverage.
enum class MmsServiceCode : uint8_t {
  READ    = 0x01,   // ~ GetDataValues request/response
  WRITE   = 0x02,   // ~ SetDataValues request/response
  REPORT  = 0x03,   // ~ unsolicited Report (server -> client, no request)
};

// MMS-side exception codes, mirroring Modbus's ILLEGAL_FUNCTION/
// ILLEGAL_DATA_ADDRESS/ILLEGAL_DATA_VALUE scheme but named for MMS's
// name-addressed, service-oriented shape.
enum class MmsException : uint8_t {
  ILLEGAL_SERVICE  = 0x01,  // unsupported/unrecognized service code
  UNKNOWN_OBJECT   = 0x02,  // object reference not found in either point map
  ILLEGAL_VALUE    = 0x03,  // value/type mismatch for the target point
};

// Wire-level PDU. READ/WRITE reference a single named point (address
// space collision between analog/binary is structurally impossible
// here, unlike Modbus, since the map key is the point's own name).
// REPORT carries a batch snapshot of every current point instead --
// real Report Control Blocks push a configured dataset; since dataset
// modeling is out of scope, a Report here is the full current point
// set, which also mirrors how HELICS data already arrives in bulk via
// DoEndpoint's per-node JSON payload.
struct MmsPDU {
  MmsServiceCode serviceCode;
  std::string objectReference;   // point name; used by READ/WRITE only
  bool isAnalog = false;         // which value field below is meaningful
  float analogValue = 0.0f;
  bool binaryValue = false;
  bool isException = false;
  uint8_t exceptionCode = 0;
  // Set by DecodePDU on a truncated/malformed packet -- a distinct flag
  // from isException (which represents a well-formed *error response*,
  // not a parse failure), so callers can tell "server said no" from
  // "packet was garbage" without relying on ambiguous sentinel values.
  bool isMalformed = false;
  // For READ only: true if analogValue/binaryValue/isAnalog hold a real
  // value to encode (a response), false for a bare request (just
  // objectReference). WRITE and REPORT are unambiguous without this --
  // WRITE always carries a value (request and echo-response alike), and
  // REPORT is never request/response at all (see EncodePDU/DecodePDU).
  bool hasValue = false;

  // REPORT only
  std::vector<std::pair<std::string, float>> reportAnalogValues;
  std::vector<std::pair<std::string, bool>> reportBinaryValues;
};

// Replaces DNP3's Master::MasterConfig/Outstation::OutstationConfig and
// Modbus's ModbusDeviceConfig. No unit-id equivalent: an MMS server is
// already uniquely identified by its TCP association, unlike Modbus TCP
// which multiplexes multiple RTU addresses over one connection.
struct MmsDeviceConfig {
  std::map<std::string, float> analogValues;
  std::map<std::string, bool>  binaryValues;
};

class MmsApplicationNew : public Application
{
public:
  static TypeId GetTypeId (void);

  MmsApplicationNew ();
  virtual ~MmsApplicationNew ();

  uint32_t GetTotalRx () const;
  Ptr<Socket> GetListeningSocket (void) const;
  std::list<Ptr<Socket> > GetAcceptedSockets (void) const;

  void SetName (const std::string &name);
  void SetLocal (Ipv4Address ip, uint16_t port);
  void SetLocal (Ipv6Address ip, uint16_t port);
  void SetLocal (Address ip, uint16_t port);

  void Store (std::string point, std::string value);
  std::string GetName (void) const;

  // -- HELICS integration (mirrors DNP3/Modbus's endpoint/message handling) --
  void SetEndpointName (const std::string &name, bool is_global);
  void EndpointCallback (helics::Endpoint id, helics::Time time);

  // -- Point access, name-keyed directly into m_deviceConfig (see file --
  // -- header note on why no address-translation layer is needed) --
  void SetAnalogPoint (const std::string &objectReference, float value);
  float GetAnalogPoint (const std::string &objectReference) const;
  void SetBinaryPoint (const std::string &objectReference, bool value);
  bool GetBinaryPoint (const std::string &objectReference) const;
  float GetFrozenAnalogPoint (const std::string &objectReference) const;
  bool GetFrozenBinaryPoint (const std::string &objectReference) const;

  Address m_localAddress;
  uint16_t m_localPort;
  Address m_remoteAddress;
  uint16_t m_remotePort;
  Address m_remoteAddress2; // used for insider/MIM socket, mirroring Modbus/DNP3's equivalent
  uint16_t m_masterport;
  std::string m_name = "";
  std::string f_name;
  std::string points_filename;
  bool m_isMaster; // client (true) / server (false); name kept for parity
                    // with the shared, protocol-agnostic topology code
                    // that sets this attribute identically across protocols
  bool running;
  double m_attackChance = 0.0;
  double m_jitterMinNs;
  double m_jitterMaxNs;

  // -- Client-side request methods --
  void ReadDataValue (const std::string &objectReference);
  void WriteAnalogValue (const std::string &objectReference, float value);
  void WriteBinaryValue (const std::string &objectReference, bool value);

  void periodic_poll (int count);

  // -- Attack scaffolding: kept structurally identical to DNP3/Modbus's --
  // -- equivalents so cross-protocol attack comparisons are valid --
  // NOTE: unlike Modbus's permanent no-op stub, attack_data here is a
  // real implementation -- see the enum comment on MmsServiceCode::REPORT.
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
  void startClient (void);
  void startServer (Ptr<Socket> sock);

  void HandleRead (Ptr<Socket> socket);
  void Record (Ptr<Packet> packet, Address from);
  void HandleAccept (Ptr<Socket> socket, const Address& from);
  void HandlePeerClose (Ptr<Socket> socket);
  void HandlePeerError (Ptr<Socket> socket);
  void ConnectToPeer (Ptr<Socket> localSocket, uint16_t servPort);
  void HandleConnectionSucceeded (Ptr<Socket> socket);
  void HandleConnectionFailed (Ptr<Socket> socket);

  void store_points (std::string point, std::string value);
  float apply_fdi (const std::string& name, float realValue);
  void initConfig (void);
  void makeTcpConnection (void);
  // NOTE: takes the object reference directly rather than a numeric
  // index (unlike DNP3/Modbus's pointId int) -- MMS has no numeric
  // point address to index by (see file header note), so forcing an
  // artificial index here purely for signature parity would cost real
  // correctness for no benefit; the two-class/attack-scaffolding shape
  // is what needs to mirror DNP3/Modbus, not every parameter type.
  void resetToRealValue (const std::string &objectReference, bool isAnalog, const std::string& realValue);
  void save_data (Ptr<Socket> socket, Ptr<Packet> packet, Address from);

  // -- MMS PDU encode/decode (from-scratch; see file header note) --
  MmsPDU DecodePDU (Ptr<Packet> packet, bool isResponse = false);
  Ptr<Packet> EncodePDU (const MmsPDU& pdu);

  // -- MIM/attack handling: same shape as DNP3/Modbus's handle_MIM/handle_normal --
  void handle_MIM (Ptr<Socket> socket);
  void handle_normal (Ptr<Socket> socket);
  void readMicroGridConfig (std::string fpath, Json::Value& configobj);
  void set_attack (bool state);
  void send_directly (Ptr<Packet> packet);
  void send_directly_server (Ptr<Socket> sock, Ptr<Packet> packet);
  std::vector<std::string> get_val_vector (std::string delimiter, std::string m_attack_val);
  float get_val (std::vector<std::string> val, std::vector<std::string> val_min, std::vector<std::string> val_max, int index);

  Ptr<Socket> m_socket;
  Ptr<Socket> mim_socket;
  std::list<Ptr<Socket> > m_socketList;

  Address m_local;
  uint32_t m_totalRx;
  TypeId m_tid;

  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
  int debugLevel;
  int respTimeout;
  int integrityPollInterval;

  MmsDeviceConfig m_deviceConfig;
  // Snapshot of m_deviceConfig taken at the moment the server transitions
  // offline (see set_offline), mirroring DNP3/Modbus's frozen-point
  // mechanism -- serves "last known good" values while offline.
  MmsDeviceConfig m_frozenDeviceConfig;

  bool m_enableTcp;
  bool m_connected;
  Ptr<UniformRandomVariable> m_rand_delay_ns;

  // Uses ns-3's own seeded RNG stream (respects --RngRun for real
  // reproducibility/variation across runs), unlike apply_fdi's
  // original bare rand()/RAND_MAX, which was never seeded via
  // srand() anywhere in this codebase -- meaning AttackChance's
  // roll was silently deterministic (identical outcome every run,
  // regardless of RngRun) across all four protocols until this fix.
  Ptr<UniformRandomVariable> m_fdiRand;

  // Snapshot of m_deviceConfig.analogValues taken at the end of initConfig()
  // (before any attack can run), used by set_attack() to restore real values
  // once an FDI attack window ends -- see set_attack() for why this is
  // needed instead of relying on the next real update to overwrite it.
  std::map<std::string, float> m_preAttackAnalogValues;

  TracedCallback<Ptr<const Packet> > m_txTrace;
  TracedCallback<Ptr<const Packet> > m_rxTraces;
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;

  static const int MAX_LEN = 260; // kept for structural parity with Modbus; unused there too (vestigial from DNP3 port)

  // -- Point name lists, populated by initConfig() from points_filename --
  // (same CSV format DNP3/Modbus use: "ANALOG"/"BINARY", point name,
  // initial value). Used by handle_MIM's node/point substring matching.
  // Unlike Modbus, no analog_name_to_address/binary_name_to_address maps
  // exist here -- the name itself is the address (see file header note).
  std::vector<std::string> analog_point_names;
  std::vector<std::string> binary_point_names;

  bool mitm_flag = false;
  bool fdi_flag = false; //Compromised-endpoint FDI: outstation fabricates its own readings, no MITM position needed (see DNP3/Modbus's identical addition)

  // -- Attack state: mirrors DNP3/Modbus's naming exactly for mergeability --
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

  // -- Report scheduling (server-side unsolicited push; see attack_data) --
  uint32_t m_reportIntervalMs = 5000;
};

} // namespace ns3

#endif /* MMS_APPLICATION_NEW_H */
