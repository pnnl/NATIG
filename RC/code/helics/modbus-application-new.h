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
 * Modbus TCP application for NATIG co-simulation.
 * Structurally mirrors dnp3-application-new.h (socket handling, attack
 * injection, and HELICS integration) to keep cross-protocol behavior
 * comparable. Protocol framing is implemented from scratch below, since
 * no Modbus library is currently vendored in this codebase (unlike the
 * DNP3 side, which wraps an external master/outstation library).
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#ifndef MODBUS_APPLICATION_NEW_H
#define MODBUS_APPLICATION_NEW_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/socket.h" // full Socket definition needed (not just the forward
                         // declaration below) since we call Socket methods
                         // directly (Bind, Connect, Listen, &Socket::Send,
                         // etc.) -- this was missed when initially porting
                         // DNP3's includes; caught by the first real compile
                         // attempt (incomplete-type errors on Socket usage
                         // in makeTcpConnection).
#include "ns3/socket-factory.h" // used alongside Socket::CreateSocket() /
                                 // TypeId::LookupByName("ns3::TcpSocketFactory")
#include "ns3/node.h" // full Node definition for GetNode()/AddApplication()
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
#include <cstdint>
#include <iostream> // std::cout is used directly in several places (e.g.
                    // initConfig), ported from DNP3 which includes this too

#include "ns3/helics-application.h"
#include "ns3/helics.h" // declares `extern std::shared_ptr<helics::CombinationFederate> helics_federate;`
                         // -- NOT pulled in transitively by helics-application.h, confirmed by inspection
#include "helics/helics.hpp"

// jsoncpp's include path differs between build environments (observed
// on the DNP3 side: <json/json.h> on one environment, <jsoncpp/json/json.h>
// on another -- this was previously handled by maintaining two nearly-
// identical file variants, which we're avoiding here since it created
// a real maintenance burden (a fix had to be manually ported to both
// copies). __has_include lets a single file adapt to whichever path
// actually exists at compile time instead.
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
  // Fallback for a pre-C++17/no-__has_include toolchain: default to
  // the non-jsoncpp-prefixed path (matches DNP3's non-Docker default).
  // If this build environment actually needs the jsoncpp/ prefix and
  // lacks __has_include support, this will need manual adjustment.
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
 * \brief Modbus TCP application supporting master (client) and outstation
 * (server) roles, with MIM/attack injection scaffolding matching the
 * DNP3 application for cross-protocol detectability comparison.
 */

// Modbus function codes we support in this first pass. Scope chosen to
// give read+write parity for both analog (holding register) and binary
// (coil) points -- matching DNP3's analog/binary poll+control coverage --
// without implementing the full Modbus spec (no Input Registers/Discrete
// Inputs yet; add only if a specific research need arises).
enum class ModbusFunctionCode : uint8_t {
  READ_COILS               = 0x01,
  READ_HOLDING_REGISTERS   = 0x03,
  WRITE_SINGLE_COIL        = 0x05,
  WRITE_SINGLE_REGISTER    = 0x06,
};

// Minimal Modbus TCP framing: MBAP header (transaction id, protocol id,
// length, unit id) + PDU (function code + data). Replaces DNP3's Lpdu
// link-layer type, which has no Modbus equivalent needed here.
struct ModbusMBAPHeader {
  uint16_t transactionId = 0;
  uint16_t protocolId    = 0;   // always 0 for Modbus TCP
  uint16_t length        = 0;   // byte count of unitId + PDU
  uint8_t  unitId        = 0;
};

struct ModbusPDU {
  ModbusFunctionCode functionCode;
  uint16_t address = 0;
  uint16_t value   = 0;      // used for single coil/register writes
  uint16_t quantity = 0;     // used for read requests
  std::vector<uint8_t> data; // raw payload for reads/responses
  // Set by a handler to signal EncodePDU should build a spec-compliant
  // exception response (function code | 0x80 + exceptionCode) instead
  // of a normal response, e.g. for an illegal value on an otherwise
  // valid function code (see WRITE_SINGLE_COIL's illegal-value check).
  bool isException = false;
  uint8_t exceptionCode = 0;
};

// Replaces DNP3's Master::MasterConfig / Outstation::OutstationConfig /
// RemoteDevice (library types). Holds just what Modbus needs: the point
// maps for this device and its slave/unit id.
struct ModbusDeviceConfig {
  uint8_t unitId = 1;
  std::map<uint16_t, uint16_t> holdingRegisters; // analog points
  std::map<uint16_t, bool>     coils;            // binary points
};

class ModbusApplicationNew : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  ModbusApplicationNew ();
  virtual ~ModbusApplicationNew ();

  uint32_t GetTotalRx () const;
  Ptr<Socket> GetListeningSocket (void) const;
  std::list<Ptr<Socket> > GetAcceptedSockets (void) const;

  void SetName (const std::string &name);
  void SetLocal (Ipv4Address ip, uint16_t port);
  void SetLocal (Ipv6Address ip, uint16_t port);
  void SetLocal (Address ip, uint16_t port);

  void Store (std::string point, std::string value);
  std::string GetName (void) const;

  // -- HELICS integration (mirrors DNP3's endpoint/message handling) --
  void SetEndpointName (const std::string &name, bool is_global);
  void EndpointCallback (helics::Endpoint id, helics::Time time);

  // -- Point access (replaces DNP3's changePoint/registerName, which --
  // -- existed to satisfy the external library's EventInterface;    --
  // -- Modbus has no such external library to satisfy)              --
  void SetHoldingRegister (uint16_t address, uint16_t value);
  uint16_t GetHoldingRegister (uint16_t address) const;
  void SetCoil (uint16_t address, bool value);
  bool GetCoil (uint16_t address) const;
  uint16_t GetFrozenHoldingRegister (uint16_t address) const;
  bool GetFrozenCoil (uint16_t address) const;

  Address m_localAddress;
  uint16_t m_localPort;
  Address m_remoteAddress;
  uint16_t m_remotePort;
  Address m_remoteAddress2; // used for insider/MIM socket, mirroring DNP3's equivalent
  uint16_t m_masterport;
  uint8_t m_unitId; // Modbus unit identifier (replaces DNP3's separate master/station device address attributes)
  std::string m_name = "";
  std::string f_name; // output file name, mirrors DNP3's OutFileName attribute
  std::string points_filename;
  bool m_isMaster;
  bool running;
  double m_attackChance = 0.0;
  double m_jitterMinNs;
  double m_jitterMaxNs;

  // -- Master (client) request methods --
  void ReadHoldingRegisters (uint16_t startAddress, uint16_t quantity);
  void ReadCoils (uint16_t startAddress, uint16_t quantity);
  void WriteSingleRegister (uint16_t address, uint16_t value);
  void WriteSingleCoil (uint16_t address, bool value);

  void periodic_poll (int count);

  // -- Attack scaffolding: kept structurally identical to DNP3's --
  // -- equivalents so cross-protocol attack comparisons are valid --
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
  void startMaster (void);
  void startOutstation (Ptr<Socket> sock);

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
  void resetToRealValue (int pointId, const std::string& realValue);
  void save_data (Ptr<Socket> socket, Ptr<Packet> packet, Address from);

  // Per-address analog register scale factor: raw register value =
  // round(engineering value / scale); engineering value = raw register
  // value * scale. Looks up m_registerScale, defaulting to 1 (unscaled)
  // for any address not present -- see m_registerScale below.
  uint16_t GetRegisterScale (uint16_t address) const;

  // -- Modbus PDU encode/decode (from-scratch, replaces DNP3's --
  // -- link-layer parsing done inside the vendored library) --
  ModbusPDU DecodePDU (Ptr<Packet> packet, bool isResponse = false);
  Ptr<Packet> EncodePDU (const ModbusPDU& pdu, uint8_t unitId);

  // -- MIM/attack handling: same shape as DNP3's handle_MIM/handle_normal --
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

  ModbusDeviceConfig m_deviceConfig;
  // Snapshot of m_deviceConfig taken at the moment the outstation
  // transitions offline (see set_offline), used to serve "last known
  // good" values while offline instead of either live values or a
  // hardcoded 0/false, mirroring DNP3's frozen_analog_points/
  // frozen_bin_points mechanism.
  ModbusDeviceConfig m_frozenDeviceConfig;

  // Per-address analog register scale (see GetRegisterScale). Populated
  // once in initConfig() at the point each analog point's address is
  // assigned: 4 for ordinary continuous engineering quantities (gives
  // headroom to 262,140 at +-2 unit register resolution -- comfortably
  // past known FDI attack magnitudes and real GridLAB-D voltage/current
  // values that overflow a raw 16-bit register), 1 (i.e. absent, see
  // default in GetRegisterScale) for tap_A/B/C and capacitor_A/B/C,
  // which are discrete regulator step positions that must stay exact
  // integers. Fixes truncation/overflow flagged by Oceane Bel (PNNL) on
  // PR #5.
  std::map<uint16_t, uint16_t> m_registerScale;

  // attack_type 5 (replay): last real value captured per address, frozen once
  // that point's attack window opens. Distinct from m_frozenDeviceConfig above
  // (that's a whole-device offline snapshot); this is per-point and keyed by
  // the attack config, not device state. Deliberately scale-agnostic (see
  // handle_MIM): moves raw register bits verbatim, same as a real replay
  // capture would.
  std::map<uint16_t, uint16_t> m_replayCaptureRegisters;
  std::map<uint16_t, bool> m_replayCaptureCoils;

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

  // Snapshot of every analog point's real register value, keyed by point
  // name (not address -- apply_fdi matches by name), taken at the end of
  // initConfig() before any attack can run. Modbus has no live name-keyed
  // analog value map like GOOSE/DNP3/MMS do (its live store is address-keyed
  // m_deviceConfig.holdingRegisters); this bridges via
  // analog_name_to_address so set_attack() can restore real values by name
  // once an FDI attack window ends -- see set_attack() for why restoring is
  // needed instead of relying on the next real update to overwrite it.
  std::map<std::string, float> m_preAttackAnalogValues;

  TracedCallback<Ptr<const Packet> > m_txTrace;
  TracedCallback<Ptr<const Packet> > m_rxTraces;
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;

  static const int MAX_LEN = 260; // Modbus TCP max ADU size

  // -- Point storage, populated by initConfig() from points_filename --
  // (same CSV format DNP3 uses: "ANALOG"/"BINARY", point name, initial
  // value). Point names are kept for handle_MIM's node/point matching
  // (ported from DNP3), and are separately assigned sequential numeric
  // addresses below, since Modbus addresses points only by number.
  std::map<std::string, float> analog_points;
  std::map<std::string, uint16_t> bin_points;
  std::map<std::string, float> frozen_analog_points;
  std::map<std::string, uint16_t> frozen_bin_points;
  std::vector<std::string> binary_point_names;
  std::vector<std::string> analog_point_names;

  // -- Name -> Modbus address maps (sequential by row order in the --
  // -- points file; analog rows -> holding register addresses,     --
  // -- binary rows -> coil addresses; separate address spaces) --
  std::map<std::string, uint16_t> analog_name_to_address;
  std::map<std::string, uint16_t> binary_name_to_address;

  bool mitm_flag = false;
  bool fdi_flag = false; //Compromised-endpoint FDI: outstation fabricates its own readings, no MITM position needed (see DNP3's identical addition)

  // -- Attack state: mirrors DNP3's naming exactly for mergeability --
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
};

} // namespace ns3

#endif /* MODBUS_APPLICATION_NEW_H */
