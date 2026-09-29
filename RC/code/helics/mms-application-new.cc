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
 * Structurally mirrors modbus-application-new.cc, which itself mirrors
 * dnp3-application-new.cc -- see mms-application-new.h for the design
 * rationale (name-keyed point storage instead of Modbus's numeric
 * address translation, and a real Report/unsolicited-push
 * implementation where Modbus's attack_data is a permanent no-op).
 *
 * KNOWN OPEN ITEMS AT TIME OF ASSEMBLY:
 *   - Not yet compiled or tested against ns-3's actual build (this was
 *     written without local access to the ns-3/HELICS toolchain --
 *     Docker/Unity verification, same as the Modbus session, still
 *     needs to happen before this is considered done).
 *   - No wscript entry or build_ns3.sh/build_helics.sh copy lines yet
 *     (see modbus's equivalent addition -- same pattern needed here).
 *   - No scenario file (ns3-iec61850-helics-grid.cc) yet exists to
 *     instantiate an MMS topology.
 *   - No MMS points file (CSV) yet exists for any test topology --
 *     though the format is byte-for-byte identical to DNP3/Modbus's,
 *     so any existing points_*.csv is structurally valid input; only
 *     the point *names* would ideally look like MMS object references
 *     (e.g. "Node1$XCBR1.Pos.stVal") rather than Modbus's flatter names,
 *     though nothing in this file enforces that convention.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#include "mms-application-new.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/address.h"
#include "ns3/trace-source-accessor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include "ns3/packet.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/simulator.h"
#include <unistd.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MmsApplicationNew");

// CRITICAL: forces MmsApplicationNew::GetTypeId() to run at
// static-initialization time -- see modbus-application-new.cc's
// identical comment and the real bug (segfault inside IidManager) this
// guards against when missing.
NS_OBJECT_ENSURE_REGISTERED (MmsApplicationNew);

TypeId
MmsApplicationNew::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MmsApplicationNew")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<MmsApplicationNew> ()
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket. "
                   "Kept registered but unused, mirroring Modbus's identical "
                   "attribute (see ModbusApplicationHelperNew's constructor "
                   "comment) -- the helper's two-argument constructor never "
                   "calls Set() on this, since m_tid is never read anywhere "
                   "else; MMS, like Modbus, always uses TCP explicitly via "
                   "TypeId::LookupByName in makeTcpConnection.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&MmsApplicationNew::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&MmsApplicationNew::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddAttribute ("LocalAddress",
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&MmsApplicationNew::m_localAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort",
                   "The source port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MmsApplicationNew::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&MmsApplicationNew::m_remoteAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemoteAddress2",
                   "The source of the outbound packets for the insider",
                   AddressValue (Ipv4Address ("10.0.0.0")),
                   MakeAddressAccessor (&MmsApplicationNew::m_remoteAddress2),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort",
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MmsApplicationNew::m_remotePort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MasterPort", "The client's destination port",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MmsApplicationNew::m_masterport),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("isMaster",
                   "client or server (attribute name kept identical to "
                   "DNP3/Modbus so the shared, protocol-agnostic topology "
                   "code that sets it can be reused unchanged)",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MmsApplicationNew::m_isMaster),
                   MakeBooleanChecker ())
    .AddAttribute ("PointsFilename",
                   "Input Points Definitions",
                   StringValue (),
                   MakeStringAccessor (&MmsApplicationNew::points_filename),
                   MakeStringChecker ())
    .AddAttribute ("JitterMinNs",
                   "Minimum jitter delay (ns) for packet transmission",
                   DoubleValue (1000),
                   MakeDoubleAccessor (&MmsApplicationNew::m_jitterMinNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("JitterMaxNs",
                   "Maximum jitter delay (ns) for packet transmission",
                   DoubleValue (100000),
                   MakeDoubleAccessor (&MmsApplicationNew::m_jitterMaxNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableTCP", "Enable TCP connection",
                   BooleanValue (true),
                   MakeBooleanAccessor (&MmsApplicationNew::m_enableTcp),
                   MakeBooleanChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&MmsApplicationNew::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx2", "A packet has been received",
                     MakeTraceSourceAccessor (&MmsApplicationNew::m_rxTraces),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&MmsApplicationNew::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddAttribute ("AttackSelection", "Select the type of attack. Disconnect or send 0 payload",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MmsApplicationNew::m_attackType),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Value_attck", "Select a value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&MmsApplicationNew::m_attack_point_val),
                   MakeStringChecker ())
    .AddAttribute ("Value_attck_max", "Select the max value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&MmsApplicationNew::m_attack_max),
                   MakeStringChecker ())
    .AddAttribute ("Value_attck_min", "Select the min value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&MmsApplicationNew::m_attack_min),
                   MakeStringChecker ())
    .AddAttribute ("PointID", "The ID of the point that is being modified for nodeX ex:Pref, Qref",
                   StringValue (),
                   MakeStringAccessor (&MmsApplicationNew::point_id),
                   MakeStringChecker ())
    .AddAttribute ("NodeID", "The ID of the node that has a point being modified, note before the $",
                   StringValue (),
                   MakeStringAccessor (&MmsApplicationNew::node_id),
                   MakeStringChecker ())
    .AddAttribute ("RealVal", "The value that the victim should be set back to after the attack ends",
                   StringValue ("NA"),
                   MakeStringAccessor (&MmsApplicationNew::RealVal),
                   MakeStringChecker ())
    .AddAttribute ("AttackConf", "The config file that contains the attack parameters",
                   StringValue ("NA"),
                   MakeStringAccessor (&MmsApplicationNew::configFile),
                   MakeStringChecker ())
    .AddAttribute ("AttackStartTime", "Attack start time in seconds",
                   StringValue ("0"),
                   MakeStringAccessor (&MmsApplicationNew::m_attackStartTime),
                   MakeStringChecker ())
    .AddAttribute ("AttackEndTime", "Attack end time in seconds",
                   StringValue ("0"),
                   MakeStringAccessor (&MmsApplicationNew::m_attackEndTime),
                   MakeStringChecker ())
    .AddAttribute ("AttackChance", "Attack chance in percentage (0 to 1)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&MmsApplicationNew::m_attackChance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Name",
                   "The name of the application",
                   StringValue (),
                   MakeStringAccessor (&MmsApplicationNew::m_name),
                   MakeStringChecker ())
    .AddAttribute ("ID", "Int representing the ID of the MIM attacker",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MmsApplicationNew::MIM_ID),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OutFileName",
                   "The name of the output file",
                   StringValue (),
                   MakeStringAccessor (&MmsApplicationNew::f_name),
                   MakeStringChecker ())
    .AddAttribute ("mitmFlag", "Man in the middle flag",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MmsApplicationNew::mitm_flag),
                   MakeBooleanChecker ())
    .AddAttribute ("FdiFlag", "Compromised-endpoint false-data-injection flag: this outstation fabricates its own readings, no MITM position involved",
                   BooleanValue (false),
                   MakeBooleanAccessor (&MmsApplicationNew::fdi_flag),
                   MakeBooleanChecker ())
    .AddAttribute ("FdiID", "Int representing the ID of the FDI attacker, indexes into the config's FDI array",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MmsApplicationNew::FDI_ID),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ReportIntervalMs",
                   "Interval, in milliseconds, at which a server-role instance "
                   "pushes an unsolicited Report of its full current point set. "
                   "MMS-specific: no Modbus equivalent exists (Modbus is "
                   "strictly polled), see attack_data().",
                   UintegerValue (5000),
                   MakeUintegerAccessor (&MmsApplicationNew::m_reportIntervalMs),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

MmsApplicationNew::MmsApplicationNew ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  mim_socket = 0;
  m_rand_delay_ns = CreateObject<UniformRandomVariable> ();
  m_rand_delay_ns->SetAttribute ("Min", DoubleValue (m_jitterMinNs));
  m_rand_delay_ns->SetAttribute ("Max", DoubleValue (m_jitterMaxNs));
  m_fdiRand = CreateObject<UniformRandomVariable> ();
}

MmsApplicationNew::~MmsApplicationNew ()
{
  NS_LOG_FUNCTION (this);
}

uint32_t
MmsApplicationNew::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
MmsApplicationNew::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
MmsApplicationNew::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void
MmsApplicationNew::SetName (const std::string &name)
{
  m_name = name;
}

std::string
MmsApplicationNew::GetName (void) const
{
  return m_name;
}

// -------------------------------------------------------------------
// CSVRow -- ported verbatim from modbus-application-new.cc (itself
// ported verbatim from DNP3). Fully generic comma-split line reader.
// -------------------------------------------------------------------
class CSVRow
{
public:
  std::string const& geti (std::size_t index) const
  {
    return m_data[index];
  }
  std::size_t size () const
  {
    return m_data.size ();
  }
  void readNextRow (std::istream& str)
  {
    std::string line;
    std::getline (str, line);

    std::stringstream lineStream (line);
    std::string cell;

    m_data.clear ();
    while (std::getline (lineStream, cell, ','))
      {
        m_data.push_back (cell);
      }
  }
private:
  std::vector<std::string> m_data;
};

void
MmsApplicationNew::readMicroGridConfig (std::string fpath, Json::Value& configobj)
{
  std::ifstream tifs (fpath);
  Json::Reader configreader;
  configreader.parse (tifs, configobj);
}

// Dead code in DNP3/Modbus too (defined, never called). No-op stub
// rather than a faithful port -- see modbus-application-new.cc's
// identical reasoning.
void
MmsApplicationNew::GetStartStopArray ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("MmsApplication::GetStartStopArray: no-op (unused in DNP3/Modbus source; not ported)");
}

std::vector<std::string>
MmsApplicationNew::get_val_vector (std::string delimiter, std::string m_attack_val)
{
  size_t pos = 0;
  std::vector<std::string> val;
  std::string token;
  std::string vi = m_attack_val;
  while ((pos = vi.find (delimiter)) != std::string::npos)
    {
      token = vi.substr (0, pos);
      val.push_back (token);
      vi.erase (0, pos + delimiter.length ());
    }
  val.push_back (vi);
  return val;
}

float
MmsApplicationNew::get_val (std::vector<std::string> val, std::vector<std::string> val_min,
                              std::vector<std::string> val_max, int index)
{
  float f = 0.0;
  if (static_cast<size_t>(index) < val_min.size () && static_cast<size_t>(index) < val_max.size ())
    {
      bool minIsNumeric = !val_min[index].empty ()
        && std::find_if (val_min[index].begin (), val_min[index].end (),
                          [](unsigned char c) { return !std::isdigit (c); }) == val_min[index].end ();
      bool maxIsNumeric = !val_max[index].empty ()
        && std::find_if (val_max[index].begin (), val_max[index].end (),
                          [](unsigned char c) { return !std::isdigit (c); }) == val_max[index].end ();

      if (minIsNumeric && maxIsNumeric)
        {
          float r = (rand () % 10) + 1;
          NS_LOG_INFO ("MmsApplication::get_val: random selector " << r);
          f = (r > 5) ? std::stof (val_min[index]) : std::stof (val_max[index]);
        }
      else
        {
          f = std::stof (val[index]);
        }
    }
  else
    {
      f = std::stof (val[index]);
    }
  return f;
}

// -------------------------------------------------------------------
// GetVal -- same crash guard as Modbus's version (attack.find(key)
// instead of operator[], to avoid std::stof() on an empty string when
// a config key is missing).
// -------------------------------------------------------------------
std::vector<float>
MmsApplicationNew::GetVal (std::map<std::string, std::string> attack, std::string tag)
{
  std::vector<float> timer;
  std::string delimiter = ",";
  size_t pos = 0;
  std::string token;
  std::string key = "MIM-" + std::to_string (MIM_ID) + "-" + tag;

  auto it = attack.find (key);
  if (it == attack.end () || it->second.empty ())
    {
      NS_LOG_WARN ("MmsApplication::GetVal: no value for key '" << key
                   << "' (MIM_ID=" << MIM_ID << ", tag=" << tag << "). Returning empty vector.");
      return timer;
    }

  while ((pos = attack[key].find (delimiter)) != std::string::npos)
    {
      token = attack[key].substr (0, pos);
      timer.push_back (std::stof (token));
      attack[key].erase (0, pos + delimiter.length ());
    }
  timer.push_back (std::stof (attack[key]));
  return timer;
}

// -------------------------------------------------------------------
// Point-map accessors -- m_deviceConfig is name-keyed directly (no
// address translation layer; see file header note in the .h). Single
// source of truth used by both the MMS protocol handlers
// (handle_normal/handle_MIM) and the HELICS-driven update path
// (store_points, below).
// -------------------------------------------------------------------
void
MmsApplicationNew::SetAnalogPoint (const std::string &objectReference, float value)
{
  m_deviceConfig.analogValues[objectReference] = value;
}

float
MmsApplicationNew::GetAnalogPoint (const std::string &objectReference) const
{
  auto it = m_deviceConfig.analogValues.find (objectReference);
  return (it != m_deviceConfig.analogValues.end ()) ? it->second : 0.0f;
}

void
MmsApplicationNew::SetBinaryPoint (const std::string &objectReference, bool value)
{
  m_deviceConfig.binaryValues[objectReference] = value;
}

bool
MmsApplicationNew::GetBinaryPoint (const std::string &objectReference) const
{
  auto it = m_deviceConfig.binaryValues.find (objectReference);
  return (it != m_deviceConfig.binaryValues.end ()) ? it->second : false;
}

float
MmsApplicationNew::GetFrozenAnalogPoint (const std::string &objectReference) const
{
  auto it = m_frozenDeviceConfig.analogValues.find (objectReference);
  return (it != m_frozenDeviceConfig.analogValues.end ()) ? it->second : 0.0f;
}

bool
MmsApplicationNew::GetFrozenBinaryPoint (const std::string &objectReference) const
{
  auto it = m_frozenDeviceConfig.binaryValues.find (objectReference);
  return (it != m_frozenDeviceConfig.binaryValues.end ()) ? it->second : false;
}

// safeStof/safeStod -- FDI config values (Value_attck, AttackStartTime,
// AttackEndTime) come from grid.json/AttackConf and can be malformed or
// empty; std::stof/std::stod throw std::invalid_argument/std::out_of_range
// uncaught on that, crashing the whole simulation. This is the same crash
// class the MIM GetVal guard above closes -- see PR review from Oceane Bel
// (PNNL) on the FDI PR. Duplicated per protocol file, matching this file's
// existing convention (e.g. get_val_vector) of no shared helics helper header.
static float
safeStof (const std::string& s, float defaultValue, const std::string& context)
{
  try
    {
      return std::stof (s);
    }
  catch (const std::invalid_argument&)
    {
      std::cerr << "[WARN] " << context << ": could not parse '" << s << "' as a float, using default "
                   << defaultValue << std::endl;
      return defaultValue;
    }
  catch (const std::out_of_range&)
    {
      std::cerr << "[WARN] " << context << ": value '" << s << "' out of range for float, using default "
                   << defaultValue << std::endl;
      return defaultValue;
    }
}

static double
safeStod (const std::string& s, double defaultValue, const std::string& context)
{
  try
    {
      return std::stod (s);
    }
  catch (const std::invalid_argument&)
    {
      std::cerr << "[WARN] " << context << ": could not parse '" << s << "' as a double, using default "
                   << defaultValue << std::endl;
      return defaultValue;
    }
  catch (const std::out_of_range&)
    {
      std::cerr << "[WARN] " << context << ": value '" << s << "' out of range for double, using default "
                   << defaultValue << std::endl;
      return defaultValue;
    }
}

// Compromised-endpoint FDI: same shape as DNP3/Modbus's apply_fdi -- resolves
// node_id/point_id/Value_attck/AttackChance attributes (set once at topology
// build time, no runtime JSON re-read) and returns a fabricated value when
// they match and the chance roll fires. m_attack_on (window) is checked by
// the caller. Restoration is automatic: once the window closes, the next
// real HELICS update simply overwrites the point again.
float
MmsApplicationNew::apply_fdi (const std::string& name, float realValue)
{
  std::string delimiter = ",";
  std::vector<std::string> nodes = get_val_vector (delimiter, node_id);
  std::vector<std::string> points = get_val_vector (delimiter, point_id);
  std::vector<std::string> vals = get_val_vector (delimiter, m_attack_point_val);

  for (size_t i = 0; i < nodes.size () && i < points.size () && i < vals.size (); i++)
    {
      std::string nodePoint = nodes[i] + "$" + points[i];
      if (name.find (nodePoint) == std::string::npos)
        {
          continue;
        }

      float r = m_fdiRand->GetValue (0.0, 1.0);
      if (m_attackChance <= r)
        {
          return realValue;
        }

      float fabricated = safeStof (vals[i], realValue, "apply_fdi Value_attck");
      std::cout << "FDI: outstation " << m_name << " fabricating point " << name << " -> " << fabricated
                 << " (real value " << realValue << ") at time " << Simulator::Now ().GetSeconds () << "s" << std::endl;
      return fabricated;
    }

  return realValue;
}

// -------------------------------------------------------------------
// store_points -- called via HELICS's Store() (see DoEndpoint below).
// Unlike Modbus, no name->address translation step: writes
// m_deviceConfig directly by name.
// -------------------------------------------------------------------
void
MmsApplicationNew::store_points (std::string name, std::string value)
{
  // A name is only ever populated into exactly one of the two maps (by
  // initConfig, from the ANALOG/BINARY tag in the points CSV), so try
  // analog first, then binary -- same two-step shape as Modbus's
  // analog_name_to_address/binary_name_to_address lookup, but against
  // the live value maps directly since there's no separate address
  // space to consult first.
  if (m_deviceConfig.analogValues.find (name) != m_deviceConfig.analogValues.end ())
    {
      float v = std::atof (value.c_str ());
      if (fdi_flag && m_attack_on)
        {
          v = apply_fdi (name, v);
        }
      SetAnalogPoint (name, v);
      return;
    }

  if (m_deviceConfig.binaryValues.find (name) != m_deviceConfig.binaryValues.end ())
    {
      SetBinaryPoint (name, (value.compare ("CLOSED") == 0));
      return;
    }

  NS_LOG_INFO ("MmsApplication::store_points: point not found: " << name);
}

void
MmsApplicationNew::Store (std::string point, std::string value)
{
  if (!m_isMaster)
    {
      store_points (point, value);
    }
}

void
MmsApplicationNew::set_attack (bool state)
{
  NS_LOG_INFO ("MmsApplication::set_attack >>> Start Attack Mode: " << m_attackType);
  m_attack_on = state;

  // Option B fix (Sep 2026): apply_fdi was only ever wired into
  // store_points(), reached exclusively via the HELICS DoEndpoint path --
  // which never fires in this codebase (same fix already shipped for
  // GOOSE/DNP3). Apply FDI directly to this server's own analogValues at
  // the moment the attack window opens, so a fabricated value actually
  // reaches a read response. Restoring on attack-end matters because
  // nothing else ever refreshes these values absent a real Store() --
  // without it the fabricated value would persist past the window. Guarded
  // on fdi_flag specifically since set_attack is shared with the separate
  // rogue/MIM attack mechanism, which must not have its analogValues
  // touched.
  if (fdi_flag)
    {
      if (state)
        {
          for (auto& entry : m_deviceConfig.analogValues)
            {
              entry.second = apply_fdi (entry.first, entry.second);
            }
        }
      else
        {
          m_deviceConfig.analogValues = m_preAttackAnalogValues;
        }
    }
}

void
MmsApplicationNew::set_respond (bool respond)
{
  NS_LOG_INFO ("MmsApplication::set_respond");
  m_respond = respond;
}

void
MmsApplicationNew::set_offline (bool offline)
{
  NS_LOG_INFO ("MmsApplication::set_offline");
  if (m_isMaster)
    {
      NS_LOG_INFO ("Error: tried to set an MMS client application offline; "
                   "only valid for server applications");
    }
  else
    {
      // Snapshot live values into m_frozenDeviceConfig at the moment of
      // the actual online->offline transition -- see Modbus's identical
      // guard (!m_offline) and reasoning.
      if (offline && !m_offline)
        {
          m_frozenDeviceConfig = m_deviceConfig;
          NS_LOG_INFO ("MmsApplication::set_offline: snapshotted "
                       << m_frozenDeviceConfig.analogValues.size ()
                       << " analog points and " << m_frozenDeviceConfig.binaryValues.size ()
                       << " binary points for offline mode");
        }
      m_offline = offline;
    }
}

void
MmsApplicationNew::SetLocal (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void
MmsApplicationNew::SetLocal (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void
MmsApplicationNew::SetLocal (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = Address (ip);
  m_localPort = port;
}

void
MmsApplicationNew::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  mim_socket = 0;
  m_socketList.clear ();
  Application::DoDispose ();
}

// -------------------------------------------------------------------
// resetToRealValue -- same design as Modbus's version: the attack
// already mutated m_deviceConfig locally (see handle_MIM), so
// resetting is just writing the real value back directly, no outbound
// packet needed (the next poll/read picks up the restored value).
// -------------------------------------------------------------------
void
MmsApplicationNew::resetToRealValue (const std::string &objectReference, bool isAnalog, const std::string& realValue)
{
  double currentTime = Simulator::Now ().GetSeconds ();

  if (realValue.empty ())
    {
      NS_LOG_INFO ("MmsApplication::resetToRealValue: no real value provided for "
                   << objectReference << " at time " << currentTime << "s");
      return;
    }

  if (isAnalog)
    {
      try
        {
          float f = std::stof (realValue);
          SetAnalogPoint (objectReference, f);
          NS_LOG_INFO ("MmsApplication::resetToRealValue: reset " << objectReference
                       << " to " << f << " at time " << currentTime << "s");
        }
      catch (const std::exception& e)
        {
          NS_LOG_WARN ("MmsApplication::resetToRealValue: error resetting " << objectReference
                       << ": " << e.what () << " at time " << currentTime << "s");
        }
    }
  else
    {
      // Binary point: same TRIP/CLOSE/LATCH collapsing simplification
      // Modbus's version applies (see its identical resetToRealValue).
      bool state = (realValue.find ("CLOSE") != std::string::npos
                    || realValue.find ("LATCH_ON") != std::string::npos);
      SetBinaryPoint (objectReference, state);
      NS_LOG_INFO ("MmsApplication::resetToRealValue: reset " << objectReference
                   << " to " << (state ? "ON" : "OFF") << " (from \"" << realValue
                   << "\") at time " << currentTime << "s");
    }
}

void
MmsApplicationNew::initConfig (void)
{
  NS_LOG_FUNCTION (this);
  std::cout << points_filename << std::endl;
  std::ifstream pointsFile (points_filename, std::ifstream::in);

  if (pointsFile)
    {
      CSVRow row;
      while (pointsFile.good ())
        {
          row.readNextRow (pointsFile);
          if (row.size () > 0)
            {
              if (row.geti (0).compare ("ANALOG") == 0)
                {
                  NS_LOG_INFO ("Adding Analog: " << ((std::string) row.geti (1)));
                  std::string pointName = (std::string) row.geti (1);
                  analog_point_names.push_back (pointName);
                  m_deviceConfig.analogValues[pointName] = std::stof (row.geti (2));
                }
              else if (row.geti (0).compare ("BINARY") == 0)
                {
                  NS_LOG_INFO ("Adding Binary: " << ((std::string) row.geti (1)));
                  std::string pointName = (std::string) row.geti (1);
                  binary_point_names.push_back (pointName);
                  m_deviceConfig.binaryValues[pointName] = (std::stoi (row.geti (2)) != 0);
                }
              else
                {
                  NS_LOG_INFO ("Invalid row " << row.geti (1).c_str ());
                }
            }
        }
    }
  else
    {
      NS_LOG_INFO ("Unable to open points file:" << points_filename);
      exit (-1);
    }

  m_preAttackAnalogValues = m_deviceConfig.analogValues;

  NS_LOG_INFO ("MmsApplication::initConfig: loaded " << analog_point_names.size ()
               << " analog points and " << binary_point_names.size () << " binary points");
}

// ==================== PDU codec ====================
namespace {

uint16_t ReadU16BE (const uint8_t* p)
{
  return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

void WriteU16BE (std::vector<uint8_t>& buf, uint16_t val)
{
  buf.push_back (static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back (static_cast<uint8_t>(val & 0xFF));
}

uint32_t ReadU32BE (const uint8_t* p)
{
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
       | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void WriteU32BE (std::vector<uint8_t>& buf, uint32_t val)
{
  buf.push_back (static_cast<uint8_t>((val >> 24) & 0xFF));
  buf.push_back (static_cast<uint8_t>((val >> 16) & 0xFF));
  buf.push_back (static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back (static_cast<uint8_t>(val & 0xFF));
}

// Float values are carried on the wire as their raw 32-bit
// representation, big-endian -- memcpy through a uint32_t rather than
// reinterpret_cast, to avoid strict-aliasing UB.
float ReadFloatBE (const uint8_t* p)
{
  uint32_t bits = ReadU32BE (p);
  float f;
  std::memcpy (&f, &bits, sizeof (f));
  return f;
}

void WriteFloatBE (std::vector<uint8_t>& buf, float f)
{
  uint32_t bits;
  std::memcpy (&bits, &f, sizeof (bits));
  WriteU32BE (buf, bits);
}

// Writes a length-prefixed string (2-byte BE length + bytes) -- used
// for object reference names throughout.
void WriteRef (std::vector<uint8_t>& buf, const std::string& ref)
{
  WriteU16BE (buf, static_cast<uint16_t>(ref.size ()));
  buf.insert (buf.end (), ref.begin (), ref.end ());
}

} // anonymous namespace

// -------------------------------------------------------------------
// DecodePDU
//
// Parses a raw ns-3 Packet containing a full MMS-over-TCP frame
// (4-byte length header + service code + service-specific body) into
// an MmsPDU struct. See mms-application-new.h for the wire format
// rationale (flattened, name-addressed, not a real ISO 8823 stack).
// -------------------------------------------------------------------
MmsPDU
MmsApplicationNew::DecodePDU (Ptr<Packet> packet, bool isResponse)
{
  MmsPDU pdu;
  uint32_t size = packet->GetSize ();

  // Minimum valid frame: 4-byte length header + 1-byte service code
  if (size < 5)
    {
      NS_LOG_WARN ("DecodePDU: packet too short (" << size << " bytes), discarding");
      pdu.isMalformed = true;
      return pdu;
    }

  std::vector<uint8_t> buf (size);
  packet->CopyData (buf.data (), size);

  uint32_t declaredLen = ReadU32BE (&buf[0]);
  if (declaredLen + 4 != size)
    {
      NS_LOG_WARN ("DecodePDU: length header (" << declaredLen
                   << ") inconsistent with packet size (" << size << "), discarding");
      pdu.isMalformed = true;
      return pdu;
    }

  uint8_t rawServiceCode = buf[4];
  const uint8_t* data = &buf[5];
  uint32_t dataLen = size - 5;

  bool exception = (rawServiceCode & 0x80) != 0;
  uint8_t serviceCode = rawServiceCode & 0x7F;

  if (exception)
    {
      if (dataLen < 1)
        {
          NS_LOG_WARN ("DecodePDU: exception response too short, discarding");
          pdu.isMalformed = true;
          return pdu;
        }
      pdu.serviceCode = static_cast<MmsServiceCode>(serviceCode);
      pdu.isException = true;
      pdu.exceptionCode = data[0];
      return pdu;
    }

  switch (serviceCode)
    {
      case static_cast<uint8_t>(MmsServiceCode::READ):
        {
          pdu.serviceCode = MmsServiceCode::READ;
          if (isResponse)
            {
              // Response: typeTag(1) + refLen(2) + ref + value
              if (dataLen < 3)
                {
                  NS_LOG_WARN ("DecodePDU: read response too short, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              uint8_t typeTag = data[0];
              uint16_t refLen = ReadU16BE (&data[1]);
              if (dataLen < static_cast<uint32_t>(3 + refLen + (typeTag == 0 ? 4 : 1)))
                {
                  NS_LOG_WARN ("DecodePDU: read response truncated, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              pdu.objectReference.assign (reinterpret_cast<const char*>(&data[3]), refLen);
              const uint8_t* valuePtr = &data[3 + refLen];
              pdu.isAnalog = (typeTag == 0);
              pdu.hasValue = true;
              if (pdu.isAnalog)
                {
                  pdu.analogValue = ReadFloatBE (valuePtr);
                }
              else
                {
                  pdu.binaryValue = (valuePtr[0] != 0);
                }
            }
          else
            {
              // Request: refLen(2) + ref (type is unknown until the
              // server looks the name up -- see file header note)
              if (dataLen < 2)
                {
                  NS_LOG_WARN ("DecodePDU: read request too short, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              uint16_t refLen = ReadU16BE (&data[0]);
              if (dataLen < static_cast<uint32_t>(2 + refLen))
                {
                  NS_LOG_WARN ("DecodePDU: read request truncated, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              pdu.objectReference.assign (reinterpret_cast<const char*>(&data[2]), refLen);
            }
          break;
        }

      case static_cast<uint8_t>(MmsServiceCode::WRITE):
        {
          // Same shape for request and response (echo), like Modbus's
          // write-single-coil/register echo: typeTag(1) + refLen(2) + ref + value
          pdu.serviceCode = MmsServiceCode::WRITE;
          if (dataLen < 3)
            {
              NS_LOG_WARN ("DecodePDU: write PDU too short, discarding");
              pdu.isMalformed = true;
              return pdu;
            }
          uint8_t typeTag = data[0];
          uint16_t refLen = ReadU16BE (&data[1]);
          if (dataLen < static_cast<uint32_t>(3 + refLen + (typeTag == 0 ? 4 : 1)))
            {
              NS_LOG_WARN ("DecodePDU: write PDU truncated, discarding");
              pdu.isMalformed = true;
              return pdu;
            }
          pdu.objectReference.assign (reinterpret_cast<const char*>(&data[3]), refLen);
          const uint8_t* valuePtr = &data[3 + refLen];
          pdu.isAnalog = (typeTag == 0);
          if (pdu.isAnalog)
            {
              pdu.analogValue = ReadFloatBE (valuePtr);
            }
          else
            {
              pdu.binaryValue = (valuePtr[0] != 0);
            }
          break;
        }

      case static_cast<uint8_t>(MmsServiceCode::REPORT):
        {
          // numAnalog(2) + [refLen(2)+ref+value(4)]* + numBinary(2) + [refLen(2)+ref+value(1)]*
          pdu.serviceCode = MmsServiceCode::REPORT;
          uint32_t offset = 0;
          if (dataLen < 2)
            {
              NS_LOG_WARN ("DecodePDU: report too short for analog count, discarding");
              pdu.isMalformed = true;
              return pdu;
            }
          uint16_t numAnalog = ReadU16BE (&data[offset]);
          offset += 2;
          for (uint16_t i = 0; i < numAnalog; i++)
            {
              if (dataLen < offset + 2)
                {
                  NS_LOG_WARN ("DecodePDU: report truncated in analog section, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              uint16_t refLen = ReadU16BE (&data[offset]);
              offset += 2;
              if (dataLen < offset + refLen + 4)
                {
                  NS_LOG_WARN ("DecodePDU: report truncated in analog entry, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              std::string ref (reinterpret_cast<const char*>(&data[offset]), refLen);
              offset += refLen;
              float val = ReadFloatBE (&data[offset]);
              offset += 4;
              pdu.reportAnalogValues.emplace_back (ref, val);
            }

          if (dataLen < offset + 2)
            {
              NS_LOG_WARN ("DecodePDU: report truncated before binary count, discarding");
              pdu.isMalformed = true;
              return pdu;
            }
          uint16_t numBinary = ReadU16BE (&data[offset]);
          offset += 2;
          for (uint16_t i = 0; i < numBinary; i++)
            {
              if (dataLen < offset + 2)
                {
                  NS_LOG_WARN ("DecodePDU: report truncated in binary section, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              uint16_t refLen = ReadU16BE (&data[offset]);
              offset += 2;
              if (dataLen < offset + refLen + 1)
                {
                  NS_LOG_WARN ("DecodePDU: report truncated in binary entry, discarding");
                  pdu.isMalformed = true;
                  return pdu;
                }
              std::string ref (reinterpret_cast<const char*>(&data[offset]), refLen);
              offset += refLen;
              bool val = (data[offset] != 0);
              offset += 1;
              pdu.reportBinaryValues.emplace_back (ref, val);
            }
          break;
        }

      default:
        NS_LOG_WARN ("DecodePDU: unsupported service code 0x"
                     << std::hex << static_cast<int>(serviceCode) << std::dec);
        pdu.isMalformed = true;
        return pdu;
    }

  return pdu;
}

// -------------------------------------------------------------------
// EncodePDU
//
// Builds a full MMS-over-TCP frame (length header + service code +
// body) from an MmsPDU struct. transactionId-style correlation is not
// tracked, matching Modbus's single-outstanding-request assumption.
// -------------------------------------------------------------------
Ptr<Packet>
MmsApplicationNew::EncodePDU (const MmsPDU& pdu)
{
  std::vector<uint8_t> body;

  if (pdu.isException)
    {
      body.push_back (static_cast<uint8_t>(pdu.serviceCode) | 0x80);
      body.push_back (pdu.exceptionCode);
      std::vector<uint8_t> frame;
      WriteU32BE (frame, static_cast<uint32_t>(body.size ()));
      frame.insert (frame.end (), body.begin (), body.end ());
      return Create<Packet> (frame.data (), frame.size ());
    }

  switch (pdu.serviceCode)
    {
      case MmsServiceCode::READ:
        {
          body.push_back (static_cast<uint8_t>(MmsServiceCode::READ));
          if (pdu.hasValue)
            {
              // Response: typeTag(1) + refLen(2) + ref + value
              body.push_back (pdu.isAnalog ? 0 : 1);
              WriteRef (body, pdu.objectReference);
              if (pdu.isAnalog)
                {
                  WriteFloatBE (body, pdu.analogValue);
                }
              else
                {
                  body.push_back (pdu.binaryValue ? 1 : 0);
                }
            }
          else
            {
              // Request: refLen(2) + ref only -- see hasValue's doc
              // comment on MmsPDU for why this is unambiguous.
              WriteRef (body, pdu.objectReference);
            }
          break;
        }

      case MmsServiceCode::WRITE:
        {
          // Same shape for request and echo-response: typeTag(1) + refLen(2) + ref + value
          body.push_back (static_cast<uint8_t>(MmsServiceCode::WRITE));
          body.push_back (pdu.isAnalog ? 0 : 1);
          WriteRef (body, pdu.objectReference);
          if (pdu.isAnalog)
            {
              WriteFloatBE (body, pdu.analogValue);
            }
          else
            {
              body.push_back (pdu.binaryValue ? 1 : 0);
            }
          break;
        }

      case MmsServiceCode::REPORT:
        {
          body.push_back (static_cast<uint8_t>(MmsServiceCode::REPORT));
          WriteU16BE (body, static_cast<uint16_t>(pdu.reportAnalogValues.size ()));
          for (const auto& entry : pdu.reportAnalogValues)
            {
              WriteRef (body, entry.first);
              WriteFloatBE (body, entry.second);
            }
          WriteU16BE (body, static_cast<uint16_t>(pdu.reportBinaryValues.size ()));
          for (const auto& entry : pdu.reportBinaryValues)
            {
              WriteRef (body, entry.first);
              body.push_back (entry.second ? 1 : 0);
            }
          break;
        }

      default:
        NS_LOG_WARN ("EncodePDU: unsupported service code, sending exception response");
        body.clear ();
        body.push_back (static_cast<uint8_t>(pdu.serviceCode) | 0x80);
        body.push_back (static_cast<uint8_t>(MmsException::ILLEGAL_SERVICE));
        break;
    }

  std::vector<uint8_t> frame;
  WriteU32BE (frame, static_cast<uint32_t>(body.size ()));
  frame.insert (frame.end (), body.begin (), body.end ());
  return Create<Packet> (frame.data (), frame.size ());
}

// ==================== lifecycle ====================
void
MmsApplicationNew::StartApplication ()
{
  NS_LOG_FUNCTION (this);
  running = true;
  m_attack_on = false;
  if (fdi_flag) {
    // Unlike handle_MIM (reactive to packet arrival), FDI has a fixed window on the
    // outstation itself, so it can be scheduled once here instead of deduped via
    // StartVect/StopVect on every store_points() call. See DNP3/Modbus's identical addition.
    double attackStart = safeStod (m_attackStartTime, -1.0, "FDI AttackStartTime");
    double attackEnd = safeStod (m_attackEndTime, -1.0, "FDI AttackEndTime");
    if (attackStart < 0.0 || attackEnd < 0.0)
      {
        std::cerr << "[WARN] " << "MmsApplicationNew::StartApplication: bad FDI AttackStartTime/"
                     "AttackEndTime config value for node " << m_name
                     << " -- FDI disabled for this instance." << std::endl;
      }
    else
      {
        Simulator::Schedule(Seconds(attackStart), &MmsApplicationNew::set_attack, this, true);
        Simulator::Schedule(Seconds(attackEnd), &MmsApplicationNew::set_attack, this, false);
      }
  }

  if (!m_enableTcp)
    {
      NS_LOG_WARN ("MmsApplication: EnableTCP=false requested, but this MMS "
                   "implementation is TCP-only -- proceeding with TCP anyway.");
    }
  makeTcpConnection ();
}

void
MmsApplicationNew::makeTcpConnection (void)
{
  NS_LOG_FUNCTION (this);

  // mim_socket is only constructed/connected for Inside/MIM-role
  // instances. On the Modbus side, building and Connect()-ing this
  // socket unconditionally for every instance caused every ordinary
  // node to generate a spurious TCP handshake to a bogus default
  // RemoteAddress2, polluting FlowMonitor stats -- applying that fix
  // from the start here rather than re-discovering it.
  bool isInsiderOrMim = (m_name.find ("Inside") != std::string::npos
                          || m_name.find ("MIM") != std::string::npos);

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);

      if (Ipv4Address::IsMatchingType (m_remoteAddress))
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_localPort);
          m_socket->Bind (local);
        }
      else if (Ipv6Address::IsMatchingType (m_remoteAddress))
        {
          Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_localPort);
          m_socket->Bind (local);
        }
      else
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_localPort);
          m_socket->Bind (local);
        }

      if (isInsiderOrMim)
        {
          TypeId tid2 = TypeId::LookupByName ("ns3::TcpSocketFactory");
          mim_socket = Socket::CreateSocket (GetNode (), tid2);

          if (Ipv4Address::IsMatchingType (m_remoteAddress2))
            {
              InetSocketAddress local2 = InetSocketAddress (Ipv4Address::GetAny (), m_remotePort);
              mim_socket->Bind (local2);
            }
          else if (Ipv6Address::IsMatchingType (m_remoteAddress2))
            {
              Inet6SocketAddress local2 = Inet6SocketAddress (Ipv6Address::GetAny (), m_remotePort);
              mim_socket->Bind (local2);
            }
          else
            {
              InetSocketAddress local2 = InetSocketAddress (Ipv4Address::GetAny (), m_remotePort);
              mim_socket->Bind (local2);
            }
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&MmsApplicationNew::HandleRead, this));
  if (mim_socket)
    {
      mim_socket->SetRecvCallback (MakeCallback (&MmsApplicationNew::HandleRead, this));
    }

  // -- Attack start/end time scheduling: generic string parsing, no --
  // -- protocol-specific dependency (ported near-verbatim from Modbus) --
  std::vector<float> timer;
  std::string delimiter = ",";
  size_t pos = 0;
  std::string token;
  while ((pos = m_attackStartTime.find (delimiter)) != std::string::npos)
    {
      token = m_attackStartTime.substr (0, pos);
      timer.push_back (std::stof (token));
      m_attackStartTime.erase (0, pos + delimiter.length ());
    }
  timer.push_back (std::stof (m_attackStartTime));

  std::vector<float> timer_end;
  size_t pos1 = 0;
  std::string token1;
  while ((pos1 = m_attackEndTime.find (delimiter)) != std::string::npos)
    {
      token1 = m_attackEndTime.substr (0, pos1);
      timer_end.push_back (std::stof (token1));
      m_attackEndTime.erase (0, pos1 + delimiter.length ());
    }
  timer_end.push_back (std::stof (m_attackEndTime));

  for (size_t index = 0; index < timer.size (); index++)
    {
      if (m_isMaster)
        {
          startClient ();
          if (isInsiderOrMim)
            {
              NS_LOG_UNCOND ("MmsApplication: I'm MIM or Insider client");
              if (timer[index])
                {
                  Simulator::Schedule (Seconds (timer[index]), &MmsApplicationNew::set_attack, this, true);
                  StartVect.push_back (std::to_string (timer[index]));
                }
              if (timer_end[index])
                {
                  Simulator::Schedule (Seconds (timer_end[index]), &MmsApplicationNew::set_attack, this, false);
                  StopVect.push_back (std::to_string (timer_end[index]));
                }
            }
        }
      else
        {
          if (isInsiderOrMim)
            {
              startClient ();
              NS_LOG_UNCOND ("MmsApplication: I'm MIM or Insider server");
              if (timer[index])
                {
                  Simulator::Schedule (Seconds (timer[index]), &MmsApplicationNew::set_attack, this, true);
                  StartVect.push_back (std::to_string (timer[index]));
                }
              if (timer_end[index])
                {
                  Simulator::Schedule (Seconds (timer_end[index]), &MmsApplicationNew::set_attack, this, false);
                  StopVect.push_back (std::to_string (timer_end[index]));
                }
            }

          // The server's m_socket must only ever Listen()/accept; it
          // never actively Connect()s out itself -- calling both
          // Connect() and Listen() on the same TCP socket is invalid
          // usage. Learned on the Modbus side; applying that fix from
          // the start here rather than re-discovering it.
          if (mim_socket)
            {
              mim_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_remoteAddress2), m_localPort));
            }
          startServer (m_socket);
          if (isInsiderOrMim)
            {
              startServer (mim_socket);
            }
        }
    }

  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&MmsApplicationNew::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&MmsApplicationNew::HandlePeerClose, this),
    MakeCallback (&MmsApplicationNew::HandlePeerError, this));

  if (m_isMaster)
    {
      // Connection delay kept short -- see Modbus's identical note on
      // why an arbitrary 10s delay broke early poll attempts; well
      // before any reasonable poll-start time.
      Simulator::Schedule (MilliSeconds (100), &MmsApplicationNew::ConnectToPeer, this, m_socket, m_remotePort);
    }
  else
    {
      int listenResult = m_socket->Listen ();
      if (listenResult != 0)
        {
          NS_LOG_WARN ("MmsApplication: '" << m_name << "' Listen() failed");
        }
    }
}

void
MmsApplicationNew::ConnectToPeer (Ptr<Socket> localSocket, uint16_t servPort)
{
  NS_LOG_INFO ("MmsApplication: connecting to remote " << m_remoteAddress);

  m_socket->SetConnectCallback (
    MakeCallback (&MmsApplicationNew::HandleConnectionSucceeded, this),
    MakeCallback (&MmsApplicationNew::HandleConnectionFailed, this));

  m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_remoteAddress), m_remotePort));
  if (mim_socket)
    {
      mim_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_remoteAddress2), m_localPort));
    }
}

void
MmsApplicationNew::HandleConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_INFO ("MmsApplication: '" << m_name << "' TCP connection succeeded at t="
               << Simulator::Now ().GetSeconds () << "s");
}

void
MmsApplicationNew::HandleConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_WARN ("MmsApplication: '" << m_name << "' TCP connection failed at t="
               << Simulator::Now ().GetSeconds () << "s");
}

void
MmsApplicationNew::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  running = false;
  NS_LOG_INFO ("MmsApplication: closing application");

  while (!m_socketList.empty ())
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
  if (mim_socket)
    {
      mim_socket->Close ();
      mim_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
MmsApplicationNew::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&MmsApplicationNew::HandleRead, this));
  m_socketList.push_back (s);
  startServer (s);
  NS_LOG_INFO ("MmsApplication: in HandleAccept");
}

void
MmsApplicationNew::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << m_name << socket);
}

void
MmsApplicationNew::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

// -------------------------------------------------------------------
// startClient / startServer
//
// Same minimal role-marking-hook shape as Modbus's startMaster/
// startOutstation (no vendored library object to construct -- see
// modbus-application-new.cc's identical note on why).
// -------------------------------------------------------------------
void
MmsApplicationNew::startClient ()
{
  NS_LOG_FUNCTION (this);
  debugLevel = 0;
  // CRITICAL: initConfig() must be called here too, not just in
  // startServer(). Without it the client's m_deviceConfig stays empty
  // forever, so periodic_poll has nothing to poll -- exact class of
  // bug Modbus hit; applying the fix from the start here.
  initConfig ();
}

void
MmsApplicationNew::startServer (Ptr<Socket> sock)
{
  NS_LOG_FUNCTION (this << sock);
  // initConfig() and the m_respond/m_offline defaults are not optional
  // bookkeeping -- see Modbus's identical note on what silently breaks
  // without them (handle_normal's "if (!m_respond) ignore" check).
  initConfig ();
  m_respond = true;
  m_offline = false;
}

// ==================== read/send plumbing ====================
void
MmsApplicationNew::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("MmsApplication::HandleRead for " << m_name);

  if (m_name.find ("MIM") != std::string::npos)
    {
      handle_MIM (socket);
    }
  else
    {
      handle_normal (socket);
    }
}

void
MmsApplicationNew::send_directly (Ptr<Packet> p)
{
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

  int (Socket::*fp)(Ptr<Packet>, uint32_t) = &Socket::Send;
  Simulator::Schedule (NanoSeconds (delay_ns), fp, m_socket, p, 0);
}

void
MmsApplicationNew::send_directly_server (Ptr<Socket> sock, Ptr<Packet> p)
{
  // Must reply on the accepted-connection socket the request arrived
  // on, not the insider/MIM socket -- same class of bug Modbus hit and
  // fixed (see send_directly_server's comment there); applying that
  // fix from the start here.
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

  int (Socket::*fp)(Ptr<Packet>, uint32_t) = &Socket::Send;
  Simulator::Schedule (NanoSeconds (delay_ns), fp, sock, p, 0);
}

void
MmsApplicationNew::Record (Ptr<Packet> packet, Address from)
{
  Address localAddress;
  if (packet)
    {
      m_rxTraces (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
      if (packet->GetSize () > 0)
        {
          std::ofstream outfile;
          if (!(access ("perf.txt", F_OK) == 0))
            {
              outfile.open ("perf.txt", std::ios_base::app);
              outfile << "Timestamp : Bytes Received : From IP : Node ID : Packet UID\n";
              outfile.close ();
            }

          outfile.open ("perf.txt", std::ios_base::app);
          outfile << Simulator::Now () << " : " << packet->GetSize ()
                  << " : " << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                  << " : " << m_remoteAddress
                  << " : " << packet->GetUid () << "\n";
          outfile.close ();
        }
    }
}

// ==================== handle_normal ====================
void
MmsApplicationNew::handle_normal (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;

  NS_LOG_LOGIC ("MmsApplication:: handle_normal");

  while ((packet = socket->RecvFrom (from)))
    {
      m_txTrace (packet);
      NS_LOG_INFO ("MmsApplication:: >>> address: "
                   << InetSocketAddress::ConvertFrom (from).GetIpv4 ());

      if (m_isMaster)
        {
          // Client side: this packet is either a response to a prior
          // request, or an unsolicited Report push (see attack_data).
          // Single-outstanding-request assumption, same as Modbus/DNP3.
          MmsPDU responsePdu = DecodePDU (packet, /* isResponse */ true);

          if (responsePdu.isMalformed)
            {
              NS_LOG_WARN ("MmsApplication (client): malformed or unsupported response, discarding");
              Record (packet, from);
              continue;
            }

          if (responsePdu.isException)
            {
              NS_LOG_WARN ("MmsApplication (client): server returned exception 0x"
                           << std::hex << static_cast<int>(responsePdu.exceptionCode) << std::dec);
              Record (packet, from);
              continue;
            }

          switch (responsePdu.serviceCode)
            {
              case MmsServiceCode::READ:
                {
                  if (responsePdu.isAnalog)
                    {
                      NS_LOG_INFO ("MmsApplication (client): received " << responsePdu.objectReference
                                   << " = " << responsePdu.analogValue);
                    }
                  else
                    {
                      NS_LOG_INFO ("MmsApplication (client): received " << responsePdu.objectReference
                                   << " = " << (responsePdu.binaryValue ? "true" : "false"));
                    }
                  break;
                }
              case MmsServiceCode::WRITE:
                {
                  NS_LOG_INFO ("MmsApplication (client): write confirmed for " << responsePdu.objectReference);
                  break;
                }
              case MmsServiceCode::REPORT:
                {
                  NS_LOG_INFO ("MmsApplication (client): received unsolicited Report with "
                               << responsePdu.reportAnalogValues.size () << " analog and "
                               << responsePdu.reportBinaryValues.size () << " binary values");
                  break;
                }
              default:
                break;
            }
        }
      else
        {
          // Server side: this packet is a request.
          if (!m_respond)
            {
              NS_LOG_LOGIC ("MmsApplication (server): m_respond is false, ignoring request");
              Record (packet, from);
              continue;
            }

          MmsPDU requestPdu = DecodePDU (packet, /* isResponse */ false);

          if (requestPdu.isMalformed)
            {
              NS_LOG_WARN ("MmsApplication (server): malformed or unsupported request, no response sent");
              Record (packet, from);
              continue;
            }

          MmsPDU responsePdu;
          responsePdu.serviceCode = requestPdu.serviceCode;
          responsePdu.objectReference = requestPdu.objectReference;

          switch (requestPdu.serviceCode)
            {
              case MmsServiceCode::READ:
                {
                  bool isAnalog = (m_deviceConfig.analogValues.find (requestPdu.objectReference)
                                    != m_deviceConfig.analogValues.end ());
                  bool isBinary = (!isAnalog
                                    && m_deviceConfig.binaryValues.find (requestPdu.objectReference)
                                       != m_deviceConfig.binaryValues.end ());

                  if (!isAnalog && !isBinary)
                    {
                      NS_LOG_WARN ("MmsApplication (server): unknown object reference "
                                   << requestPdu.objectReference);
                      responsePdu.isException = true;
                      responsePdu.exceptionCode = static_cast<uint8_t>(MmsException::UNKNOWN_OBJECT);
                    }
                  else
                    {
                      responsePdu.hasValue = true;
                      responsePdu.isAnalog = isAnalog;
                      if (isAnalog)
                        {
                          responsePdu.analogValue = (m_offline)
                            ? GetFrozenAnalogPoint (requestPdu.objectReference)
                            : GetAnalogPoint (requestPdu.objectReference);
                        }
                      else
                        {
                          responsePdu.binaryValue = (m_offline)
                            ? GetFrozenBinaryPoint (requestPdu.objectReference)
                            : GetBinaryPoint (requestPdu.objectReference);
                        }
                    }
                  break;
                }

              case MmsServiceCode::WRITE:
                {
                  responsePdu.isAnalog = requestPdu.isAnalog;
                  if (requestPdu.isAnalog)
                    {
                      SetAnalogPoint (requestPdu.objectReference, requestPdu.analogValue);
                      responsePdu.analogValue = requestPdu.analogValue; // echo
                    }
                  else
                    {
                      SetBinaryPoint (requestPdu.objectReference, requestPdu.binaryValue);
                      responsePdu.binaryValue = requestPdu.binaryValue; // echo
                    }
                  break;
                }

              default:
                NS_LOG_WARN ("MmsApplication (server): reached default case unexpectedly");
                responsePdu.isException = true;
                responsePdu.exceptionCode = static_cast<uint8_t>(MmsException::ILLEGAL_SERVICE);
                break;
            }

          Ptr<Packet> responsePacket = EncodePDU (responsePdu);
          send_directly_server (socket, responsePacket);
        }

      Record (packet, from);
    }
}

// ==================== handle_MIM ====================
void
MmsApplicationNew::handle_MIM (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  Address sourceAddr;
  socket->GetSockName (sourceAddr);

  while ((packet = socket->RecvFrom (from)))
    {
      m_txTrace (packet);
      NS_LOG_INFO ("MmsApplication::handle_MIM >>> processing packet at time "
                   << Simulator::Now ().GetSeconds () << "s");

      // MIM/insider instances are installed as server-role (see the
      // topology file's install call sites) and always decode the
      // incoming packet as a request -- same assumption Modbus's
      // handle_MIM makes (it has no master-side response-decoding path
      // either).
      MmsPDU requestPdu = DecodePDU (packet, /* isResponse */ false);

      if (requestPdu.isMalformed)
        {
          Record (packet, from);
          continue;
        }

      // -- Parse attack config and node/point mappings (ported --
      // -- near-verbatim from Modbus/DNP3's handle_MIM -- generic --
      // -- string/JSON parsing with no protocol-specific dependency) --
      std::string delimiter = ",";
      std::vector<std::string> val = get_val_vector (delimiter, m_attack_point_val);
      std::vector<std::string> val_min = get_val_vector (delimiter, m_attack_min);
      std::vector<std::string> val_max = get_val_vector (delimiter, m_attack_max);
      std::vector<std::string> nodes = get_val_vector (delimiter, node_id);
      std::vector<std::string> points = get_val_vector (delimiter, point_id);
      std::vector<std::string> real_val = get_val_vector (delimiter, RealVal);

      std::vector<std::string> nodesPoints;
      for (size_t xx = 0; xx < nodes.size (); xx++)
        {
          nodesPoints.push_back (nodes[xx] + "$" + points[xx]);
        }

      // Determine which address space this request actually targets.
      // Unlike Modbus, no numeric-address collision is possible here --
      // objectReference IS the point name -- so a direct map lookup is
      // sufficient and unambiguous by construction (see the file header
      // note on why MMS sidesteps the whole address-space-collision bug
      // class Modbus needed a dedicated fix for). WRITE requests already
      // carry their type from the client (see DecodePDU), so prefer that
      // over a map lookup there.
      bool requestIsAnalog;
      bool requestIsBinary;
      if (requestPdu.serviceCode == MmsServiceCode::WRITE)
        {
          requestIsAnalog = requestPdu.isAnalog;
          requestIsBinary = !requestPdu.isAnalog;
        }
      else
        {
          requestIsAnalog = (m_deviceConfig.analogValues.find (requestPdu.objectReference)
                              != m_deviceConfig.analogValues.end ());
          requestIsBinary = (!requestIsAnalog
                              && m_deviceConfig.binaryValues.find (requestPdu.objectReference)
                                 != m_deviceConfig.binaryValues.end ());
        }

      bool attackApplied = false;

      if (mitm_flag && (requestIsAnalog || requestIsBinary))
        {
          Json::Value configObject;
          std::map<std::string, std::string> attack;
          // BUG FIX (found during Docker validation of this file): the
          // inherited Modbus/DNP3 pattern here was a *substring* check
          // (configFile.find("NA") == npos) meant to detect the "AttackConf"
          // attribute's unset sentinel default ("NA"). A substring search
          // is wrong for this purpose -- any real path containing "NA"
          // anywhere (e.g. this very repo's own directory name, NATIG)
          // makes the check think the attribute looks like the sentinel
          // and silently skips reading the real config, leaving `attack`
          // permanently empty and every chance/timing value defaulting to
          // 0 -- meaning the MIM attack can never actually fire. Caught
          // because attackChance.size()/start.size()/etc were all 0 in a
          // real test run despite AttackConf being set to a real,
          // existing file. Fixed here with an exact-equality check; the
          // same latent bug is very likely present in Modbus's and
          // DNP3's identical handle_MIM, worth flagging/fixing there too.
          if (configFile != "NA")
            {
              readMicroGridConfig (configFile, configObject);
              for (uint32_t j = 1; j < configObject["MIM"].size (); j++)
                {
                  for (const auto& item : configObject["MIM"][j].getMemberNames ())
                    {
                      std::string ID = "MIM-" + std::to_string (j) + "-" + item;
                      std::string myStr = configObject["MIM"][j][item].asString ();
                      myStr.erase (remove (myStr.begin (), myStr.end (), '"'), myStr.end ());
                      attack.insert ({ID, myStr});
                    }
                }
            }

          std::vector<float> attackChance = GetVal (attack, "attack_chance");
          std::vector<float> start = GetVal (attack, "PointStart");
          std::vector<float> stop = GetVal (attack, "PointStop");
          std::vector<float> attackType = GetVal (attack, "attack_type");

          double currentTime = Simulator::Now ().GetSeconds ();
          float r = static_cast<float>(rand ()) / static_cast<float>(RAND_MAX);

          // -- Find which configured MIM entry (if any) targets this --
          // -- request's actual object reference, then decide whether --
          // -- to attack it --
          for (size_t qq = 0; qq < nodesPoints.size (); qq++)
            {
              if (requestPdu.objectReference.find (nodesPoints[qq]) == std::string::npos)
                {
                  continue; // this MIM entry doesn't apply to the point being requested
                }

              float chance = (qq < attackChance.size ()) ? attackChance[qq] : 0.0f;
              float startTime = (qq < start.size ()) ? start[qq] : 0.0f;
              float stopTime = (qq < stop.size ()) ? stop[qq] : 0.0f;
              int attackTypeInt = (qq < attackType.size ()) ? static_cast<int>(attackType[qq]) : 0;

              if (currentTime > startTime && currentTime < stopTime && chance > r)
                {
                  NS_LOG_INFO ("MmsApplication::handle_MIM: applying attack type "
                               << attackTypeInt << " on " << requestPdu.objectReference
                               << " at time " << currentTime << "s");

                  // Same attack-type numbering as DNP3/Modbus (2/4 =
                  // false data injection on an analog point, 3 = forced
                  // control command on a binary point), so cross-
                  // protocol attack comparisons stay valid.
                  if ((attackTypeInt == 2 || attackTypeInt == 4) && requestIsAnalog)
                    {
                      float f = get_val (val, val_min, val_max, qq);
                      SetAnalogPoint (requestPdu.objectReference, f);
                      NS_LOG_INFO ("MmsApplication::handle_MIM: injected false value " << f
                                   << " for " << requestPdu.objectReference);
                    }
                  else if (attackTypeInt == 3 && requestIsBinary)
                    {
                      bool forcedState = !(val[qq].find ("TRIP") != std::string::npos
                                            || val[qq].find ("LATCH_OFF") != std::string::npos);
                      SetBinaryPoint (requestPdu.objectReference, forcedState);
                      NS_LOG_INFO ("MmsApplication::handle_MIM: forced " << requestPdu.objectReference
                                   << " to " << (forcedState ? "ON" : "OFF"));
                    }

                  attackApplied = true;

                  if (currentTime <= stopTime && currentTime >= startTime)
                    {
                      Simulator::Schedule (Seconds (0.1), &MmsApplicationNew::resetToRealValue, this,
                                            requestPdu.objectReference, requestIsAnalog,
                                            (qq < real_val.size ()) ? real_val[qq] : "");
                    }
                }
              break; // found the matching point; no need to keep scanning
            }
        }

      // -- Build and send the response, whether or not an attack was --
      // -- applied (an unattacked point still needs a normal response) --
      MmsPDU responsePdu;
      responsePdu.serviceCode = requestPdu.serviceCode;
      responsePdu.objectReference = requestPdu.objectReference;

      switch (requestPdu.serviceCode)
        {
          case MmsServiceCode::READ:
            {
              if (!requestIsAnalog && !requestIsBinary)
                {
                  responsePdu.isException = true;
                  responsePdu.exceptionCode = static_cast<uint8_t>(MmsException::UNKNOWN_OBJECT);
                }
              else
                {
                  responsePdu.hasValue = true;
                  responsePdu.isAnalog = requestIsAnalog;
                  responsePdu.analogValue = requestIsAnalog ? GetAnalogPoint (requestPdu.objectReference) : 0.0f;
                  responsePdu.binaryValue = requestIsBinary ? GetBinaryPoint (requestPdu.objectReference) : false;
                }
              break;
            }
          case MmsServiceCode::WRITE:
            {
              // Same echo-or-attacked-value semantics as Modbus's
              // handle_MIM: a write that isn't attacked is echoed back
              // as requested, without being separately committed here
              // (this MIM socket is a side channel, not the point
              // store of record -- see Modbus's identical shape).
              responsePdu.isAnalog = requestIsAnalog;
              responsePdu.analogValue = requestIsAnalog
                ? (attackApplied ? GetAnalogPoint (requestPdu.objectReference) : requestPdu.analogValue)
                : 0.0f;
              responsePdu.binaryValue = requestIsBinary
                ? (attackApplied ? GetBinaryPoint (requestPdu.objectReference) : requestPdu.binaryValue)
                : false;
              break;
            }
          default:
            responsePdu.isException = true;
            responsePdu.exceptionCode = static_cast<uint8_t>(MmsException::ILLEGAL_SERVICE);
            break;
        }

      Ptr<Packet> responsePacket = EncodePDU (responsePdu);
      send_directly_server (socket, responsePacket);

      Record (packet, from);
    }
}

// ==================== client-side request methods ====================
void
MmsApplicationNew::ReadDataValue (const std::string &objectReference)
{
  NS_LOG_FUNCTION (this << objectReference);
  MmsPDU pdu;
  pdu.serviceCode = MmsServiceCode::READ;
  pdu.objectReference = objectReference;
  pdu.hasValue = false;

  Ptr<Packet> packet = EncodePDU (pdu);
  send_directly (packet);
}

void
MmsApplicationNew::WriteAnalogValue (const std::string &objectReference, float value)
{
  NS_LOG_FUNCTION (this << objectReference << value);
  MmsPDU pdu;
  pdu.serviceCode = MmsServiceCode::WRITE;
  pdu.objectReference = objectReference;
  pdu.isAnalog = true;
  pdu.analogValue = value;

  Ptr<Packet> packet = EncodePDU (pdu);
  send_directly (packet);
}

void
MmsApplicationNew::WriteBinaryValue (const std::string &objectReference, bool value)
{
  NS_LOG_FUNCTION (this << objectReference << value);
  MmsPDU pdu;
  pdu.serviceCode = MmsServiceCode::WRITE;
  pdu.objectReference = objectReference;
  pdu.isAnalog = false;
  pdu.binaryValue = value;

  Ptr<Packet> packet = EncodePDU (pdu);
  send_directly (packet);
}

// -------------------------------------------------------------------
// periodic_poll -- the client-side scheduling loop. Unlike Modbus's
// ReadHoldingRegisters(0, count)/ReadCoils(0, count) (one message
// covering a contiguous numeric address range), MMS has no such range
// to request in bulk -- each named point is its own READ. `count` is
// reused as the poll interval in milliseconds, matching DNP3/Modbus's
// parameter-naming convention.
// -------------------------------------------------------------------
void
MmsApplicationNew::periodic_poll (int count)
{
  if (running)
    {
      for (const auto& name : analog_point_names)
        {
          ReadDataValue (name);
        }
      for (const auto& name : binary_point_names)
        {
          ReadDataValue (name);
        }

      Simulator::Schedule (MilliSeconds (count), &MmsApplicationNew::periodic_poll, this, count);
    }
}

// ==================== HELICS integration ====================
namespace {

// -- Small string helpers, ported verbatim from Modbus/DNP3's --
// -- file-local free functions (fully generic, no protocol dependency) --

std::string
toEndpointName (const std::string &name)
{
  std::string copy = name;
  std::replace (copy.begin (), copy.end (), '/', '_');
  return copy;
}

std::vector<std::string>
splitOn (std::string s, std::string delimiter)
{
  size_t pos_start = 0, pos_end, delim_len = delimiter.length ();
  std::string token;
  std::vector<std::string> res;

  while ((pos_end = s.find (delimiter, pos_start)) != std::string::npos)
    {
      token = s.substr (pos_start, pos_end - pos_start);
      pos_start = pos_end + delim_len;
      res.push_back (token);
    }

  res.push_back (s.substr (pos_start));
  return res;
}

bool
endsWithSuffix (const std::string& str, const std::string& suffix)
{
  return str.size () >= suffix.size ()
         && 0 == str.compare (str.size () - suffix.size (), suffix.size (), suffix);
}

} // anonymous namespace

void
MmsApplicationNew::SetEndpointName (const std::string &name, bool is_global)
{
  NS_LOG_FUNCTION (this << name << is_global);
  SetName (name);

  // Guard against running without a HELICS federate set up -- see
  // Modbus's identical guard and the segfault this avoids in a
  // socket-level-only test scenario that doesn't stand up HELICS.
  if (!helics_federate)
    {
      NS_LOG_WARN ("MmsApplicationNew::SetEndpointName: helics_federate is not set; "
                   "skipping HELICS endpoint registration for '" << name << "'. "
                   "This is expected if running without a HELICS federate/broker.");
      return;
    }

  if (is_global)
    {
      m_endpoint_id = helics_federate->registerGlobalEndpoint (name);
    }
  else
    {
      m_endpoint_id = helics_federate->registerEndpoint (name);
    }
  using std::placeholders::_1;
  using std::placeholders::_2;
  std::function<void(helics::Endpoint, helics::Time)> func;
  func = std::bind (&MmsApplicationNew::EndpointCallback, this, _1, _2);
  helics_federate->setMessageNotificationCallback (m_endpoint_id, func);
}

void
MmsApplicationNew::EndpointCallback (helics::Endpoint id, helics::Time time)
{
  NS_LOG_FUNCTION (this << m_name << id.getName () << time);
  DoEndpoint (id, time);
}

void
MmsApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time)
{
  NS_LOG_FUNCTION (this << id.getName () << time);
  auto message = helics_federate->getMessage (id);
  DoEndpoint (id, time, std::move (message));
}

void
MmsApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time,
                                 std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << id.getName () << time);
  NS_LOG_INFO ("MmsApplication::DoEndpoint");
  std::string text = message->data.to_string ();
  std::istringstream tifs (text);
  Json::Reader datareader;
  Json::Value parsedObject;
  datareader.parse (tifs, parsedObject);
  std::string delim = "$";

  m_gld_federate_name = "GLD";
  Json::Value root = parsedObject.isMember (m_gld_federate_name)
    ? parsedObject[m_gld_federate_name] : parsedObject;

  for (auto const& objId : root.getMemberNames ())
    {
      for (auto const& variable : root[objId].getMemberNames ())
        {
          if (root[objId][variable].size () > 0)
            {
              for (auto const& var : root[objId][variable].getMemberNames ())
                {
                  auto value = root[objId][variable][var].asString ();
                  if (variable == "status" || variable == "switchA" || variable == "switchB"
                      || variable == "switchC" || variable == "phase_A_state"
                      || variable == "phase_B_state" || variable == "phase_C_state")
                    {
                      Store (objId + delim + variable, value);
                    }
                  else if (variable == "voltage_A" || variable == "voltage_B" || variable == "voltage_C"
                           || variable == "current_out_A" || variable == "current_out_B"
                           || variable == "current_out_C" || variable == "current_in_A"
                           || variable == "current_in_B" || variable == "current_in_C"
                           || variable == "VA_Out")
                    {
                      float real, imag;
                      std::istringstream v_text_stream (value);
                      v_text_stream >> real >> imag;
                      Store (objId + delim + variable + ".real", std::to_string (real));
                      Store (objId + delim + variable + ".imag", std::to_string (imag));
                    }
                  else if (variable == "Pref" || variable == "Qref" || variable == "V_In"
                           || variable == "tap_A" || variable == "tap_B" || variable == "tap_C"
                           || variable == "capacitor_A" || variable == "capacitor_B"
                           || variable == "capacitor_C")
                    {
                      float real;
                      std::istringstream v_text_stream (value);
                      v_text_stream >> real;
                      Store (objId + delim + variable, std::to_string (real));
                    }
                  else
                    {
                      NS_LOG_WARN ("MmsApplication::DoEndpoint: unknown variable name " << variable);
                      Store (objId + delim + variable, value);
                    }
                }
            }
          else
            {
              auto value = root[objId][variable].asString ();
              if (variable == "status" || variable == "switchA" || variable == "switchB"
                  || variable == "switchC" || variable == "phase_A_state"
                  || variable == "phase_B_state" || variable == "phase_C_state")
                {
                  Store (objId + delim + variable, value);
                }
              else if (variable == "voltage_A" || variable == "voltage_B" || variable == "voltage_C"
                       || variable == "current_out_A" || variable == "current_out_B"
                       || variable == "current_out_C" || variable == "current_in_A"
                       || variable == "current_in_B" || variable == "current_in_C"
                       || variable == "VA_Out")
                {
                  float real, imag;
                  std::istringstream v_text_stream (value);
                  v_text_stream >> real >> imag;
                  Store (objId + delim + variable + ".real", std::to_string (real));
                  Store (objId + delim + variable + ".imag", std::to_string (imag));
                }
              else if (variable == "Pref" || variable == "Qref" || variable == "V_In"
                       || variable == "tap_A" || variable == "tap_B" || variable == "tap_C"
                       || variable == "capacitor_A" || variable == "capacitor_B"
                       || variable == "capacitor_C")
                {
                  float real;
                  std::istringstream v_text_stream (value);
                  v_text_stream >> real;
                  Store (objId + delim + variable, std::to_string (real));
                }
              else
                {
                  NS_LOG_WARN ("MmsApplication::DoEndpoint: unknown variable name " << variable);
                  Store (objId + delim + variable, value);
                }
            }
        }
    }
}

void
MmsApplicationNew::DoMessage (std::string target_endpoint, const std::string content,
                                const std::string content_type)
{
  NS_LOG_FUNCTION (this << target_endpoint << content);
  std::string property_delimiter = "$";
  std::vector<std::string> tokens = splitOn (target_endpoint, property_delimiter);

  std::string property_name = tokens[1];

  tokens = splitOn (tokens[0], "/");
  std::string gld_obj = tokens[1];

  std::string new_target_endpoint = toEndpointName (target_endpoint);
  if (endsWithSuffix (new_target_endpoint, ".imag") || endsWithSuffix (new_target_endpoint, ".real"))
    {
      new_target_endpoint = new_target_endpoint.substr (0, new_target_endpoint.length () - 5);
      property_name = property_name.substr (0, property_name.length () - 5);
    }

  auto msg = std::make_unique<helics::Message> ();
  std::string json_content;

  if (content_type == "int")
    {
      json_content = "{\"" + gld_obj + "\":{\"" + property_name + "\":" + content + "}}";
    }
  else if (content_type == "string")
    {
      json_content = "{\"" + gld_obj + "\":{\"" + property_name + "\":\"" + content + "\"}}";
    }
  else
    {
      json_content = content;
    }

  msg->data = json_content;
  msg->dest = m_gld_federate_name + "/" + new_target_endpoint;
  msg->time = helics::Time::epsilon ();

  DoRead (std::move (msg));
}

void
MmsApplicationNew::DoRead (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string ());
  NS_LOG_INFO ("MmsApplication::DoRead: sending message " << message->to_string ()
               << " to " << message->dest);
  helics_federate->sendMessage (m_endpoint_id, message->dest, message->data.data (), message->data.size ());
}

// -------------------------------------------------------------------
// attack_data -- REAL implementation for MMS, unlike Modbus's
// permanent no-op stub (Modbus's request/response function codes have
// no unsolicited-reporting equivalent at all; see the identical
// function there). MMS/IEC 61850's Report Control Blocks give servers
// a genuine spontaneous-push capability: `freq` is the interval in
// milliseconds, matching DNP3/Modbus's periodic_poll parameter-naming
// convention. Intended to be scheduled once (e.g. from the topology
// file, the same way periodic_poll is externally kicked off for the
// client role -- see ns3-modbus-helics-grid.cc's install call sites);
// it reschedules itself thereafter.
//
// This is a genuinely new attack surface for the cross-protocol
// comparison: handle_MIM can poison a Report's content the same way
// it poisons a READ/WRITE response, but the *traffic shape*
// (unsolicited push vs. solicited poll response) has no DNP3/Modbus
// equivalent among NATIG's protocols so far.
// -------------------------------------------------------------------
void
MmsApplicationNew::attack_data (int freq)
{
  if (running && !m_isMaster && m_respond && !m_offline)
    {
      MmsPDU pdu;
      pdu.serviceCode = MmsServiceCode::REPORT;

      for (const auto& entry : m_deviceConfig.analogValues)
        {
          pdu.reportAnalogValues.emplace_back (entry.first, entry.second);
        }
      for (const auto& entry : m_deviceConfig.binaryValues)
        {
          pdu.reportBinaryValues.emplace_back (entry.first, entry.second);
        }

      // Report Control Blocks push to subscribed clients; subscription
      // isn't modeled here, so broadcast to every currently-accepted
      // connection instead. Encode a fresh Packet per destination --
      // ns-3 Packets carry their own UID, and Record()/FlowMonitor key
      // on that UID per-flow; reusing one Packet object across several
      // Send() calls to different sockets would make logically
      // distinct Report messages collapse onto a single UID, silently
      // corrupting the per-flow trace data this framework exists to
      // produce.
      for (auto& sock : m_socketList)
        {
          Ptr<Packet> packet = EncodePDU (pdu);
          send_directly_server (sock, packet);
        }

      Simulator::Schedule (MilliSeconds (freq), &MmsApplicationNew::attack_data, this, freq);
    }
}

void
MmsApplicationNew::save_data (Ptr<Socket> socket, Ptr<Packet> packet, Address from)
{
  // Dead code in DNP3/Modbus too (defined, never called). Stub kept
  // only to satisfy the header declaration.
  NS_LOG_INFO ("MmsApplication::save_data: no-op (unused in DNP3/Modbus source; not ported)");
}

} // namespace ns3
