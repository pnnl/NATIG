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
 *
 * This is the assembled, single-translation-unit version of the
 * Modbus application, combining pieces drafted and reviewed across
 * several separate files during development (see project history for
 * the individual pieces and the reasoning behind each design
 * decision). Section markers below indicate which drafted piece each
 * block of code came from, for traceability back to that discussion.
 *
 * KNOWN OPEN ITEMS AT TIME OF ASSEMBLY (see full project discussion
 * for details on each):
 *   - Not yet compiled or tested against ns-3's actual build.
 *   - Build-system wiring not yet done: no wscript entry, no
 *     build_ns3.sh/build_helics.sh copy lines, no decision yet on
 *     whether a "-Docker" variant is needed (see the DNP3 side's
 *     equivalent, where forgetting this caused a real bug).
 *   - No scenario file (e.g. ns3-helics-grid-modbus.cc) yet exists to
 *     actually instantiate a Modbus topology or kick off
 *     periodic_poll (which, like DNP3's equivalent, is never
 *     self-invoked from within this file).
 *   - No Modbus points file (CSV) yet exists for any test topology.
 *   - Several scoped simplifications relative to DNP3 are called out
 *     inline where they occur (binary TRIP/CLOSE/LATCH collapsed to
 *     boolean, single-outstanding-request assumption on the master
 *     side, minimal exception-response coverage, etc).
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 */

#include "modbus-application-new.h"
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
#include <cmath>
#include <stdexcept>
#include "ns3/packet.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/simulator.h"
#include <unistd.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ModbusApplicationNew");

// CRITICAL: this macro forces ModbusApplicationNew::GetTypeId() to run at
// static-initialization time (before main() executes), registering the
// TypeId by name in ns-3's IidManager singleton. Without it,
// ModbusApplicationHelperNew's constructor -- which looks up
// "ns3::ModbusApplicationNew" by string name via
// m_factory.SetTypeId(...) -- finds nothing, and the very next
// m_factory.Set(...) call segfaults inside IidManager::GetAttributeN
// dereferencing an uninitialized/invalid type-info pointer. This was
// missing entirely from the initial port and caught via gdb backtrace
// on the first runtime test -- DNP3's file has the equivalent macro
// (NS_OBJECT_ENSURE_REGISTERED (Dnp3ApplicationNew);) right after its
// own NS_LOG_COMPONENT_DEFINE, which we failed to carry over.
NS_OBJECT_ENSURE_REGISTERED (ModbusApplicationNew);

// ==================== from modbus-typeid-ctor.cc ====================
TypeId
ModbusApplicationNew::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ModbusApplicationNew")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<ModbusApplicationNew> ()
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&ModbusApplicationNew::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&ModbusApplicationNew::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddAttribute ("LocalAddress",
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&ModbusApplicationNew::m_localAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort",
                   "The source port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ModbusApplicationNew::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("UnitId",
                   "Modbus unit identifier for this device (replaces DNP3's "
                   "separate MasterDeviceAddress/StationDeviceAddress attributes, "
                   "since Modbus TCP addresses a device with a single unit id)",
                   UintegerValue (1),
                   MakeUintegerAccessor (&ModbusApplicationNew::m_unitId),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("RemoteAddress",
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&ModbusApplicationNew::m_remoteAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemoteAddress2",
                   "The source of the outbound packets for the insider",
                   AddressValue (Ipv4Address ("10.0.0.0")),
                   MakeAddressAccessor (&ModbusApplicationNew::m_remoteAddress2),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort",
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ModbusApplicationNew::m_remotePort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("MasterPort", "The Master's destination port",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ModbusApplicationNew::m_masterport),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("isMaster",
                   "master or outstation",
                   BooleanValue (false),
                   MakeBooleanAccessor (&ModbusApplicationNew::m_isMaster),
                   MakeBooleanChecker ())
    .AddAttribute ("PointsFilename",
                   "Input Points Definitions",
                   StringValue (),
                   MakeStringAccessor (&ModbusApplicationNew::points_filename),
                   MakeStringChecker ())
    .AddAttribute ("JitterMinNs",
                   "Minimum jitter delay (ns) for packet transmission",
                   DoubleValue (1000),
                   MakeDoubleAccessor (&ModbusApplicationNew::m_jitterMinNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("JitterMaxNs",
                   "Maximum jitter delay (ns) for packet transmission",
                   DoubleValue (100000),
                   MakeDoubleAccessor (&ModbusApplicationNew::m_jitterMaxNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableTCP", "Enable TCP connection",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ModbusApplicationNew::m_enableTcp),
                   MakeBooleanChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&ModbusApplicationNew::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx2", "A packet has been received",
                     MakeTraceSourceAccessor (&ModbusApplicationNew::m_rxTraces),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&ModbusApplicationNew::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddAttribute ("AttackSelection", "Select the type of attack. Disconnect or send 0 payload",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ModbusApplicationNew::m_attackType),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Value_attck", "Select a value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&ModbusApplicationNew::m_attack_point_val),
                   MakeStringChecker ())
    .AddAttribute ("Value_attck_max", "Select the max value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&ModbusApplicationNew::m_attack_max),
                   MakeStringChecker ())
    .AddAttribute ("Value_attck_min", "Select the min value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&ModbusApplicationNew::m_attack_min),
                   MakeStringChecker ())
    .AddAttribute ("PointID", "The ID of the point that is being modified for nodeX ex:Pref, Qref",
                   StringValue (),
                   MakeStringAccessor (&ModbusApplicationNew::point_id),
                   MakeStringChecker ())
    .AddAttribute ("NodeID", "The ID of the node that has a point being modified, note before the $",
                   StringValue (),
                   MakeStringAccessor (&ModbusApplicationNew::node_id),
                   MakeStringChecker ())
    .AddAttribute ("RealVal", "The value that the victim should be set back to after the attack ends",
                   StringValue ("NA"),
                   MakeStringAccessor (&ModbusApplicationNew::RealVal),
                   MakeStringChecker ())
    .AddAttribute ("AttackConf", "The config file that contains the attack parameters",
                   StringValue ("NA"),
                   MakeStringAccessor (&ModbusApplicationNew::configFile),
                   MakeStringChecker ())
    .AddAttribute ("AttackStartTime", "Attack start time in seconds",
                   StringValue ("0"),
                   MakeStringAccessor (&ModbusApplicationNew::m_attackStartTime),
                   MakeStringChecker ())
    .AddAttribute ("AttackEndTime", "Attack end time in seconds",
                   StringValue ("0"),
                   MakeStringAccessor (&ModbusApplicationNew::m_attackEndTime),
                   MakeStringChecker ())
    .AddAttribute ("AttackChance", "Attack chance in percentage (0 to 1)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&ModbusApplicationNew::m_attackChance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Name",
                   "The name of the application",
                   StringValue (),
                   MakeStringAccessor (&ModbusApplicationNew::m_name),
                   MakeStringChecker ())
    .AddAttribute ("ID", "Int representing the ID of the MIM attacker",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ModbusApplicationNew::MIM_ID),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OutFileName",
                   "The name of the output file",
                   StringValue (),
                   MakeStringAccessor (&ModbusApplicationNew::f_name),
                   MakeStringChecker ())
    .AddAttribute ("mitmFlag", "Man in the middle flag",
                   BooleanValue (false),
                   MakeBooleanAccessor (&ModbusApplicationNew::mitm_flag),
                   MakeBooleanChecker ())
    .AddAttribute ("FdiFlag", "Compromised-endpoint false-data-injection flag: this outstation fabricates its own readings, no MITM position involved",
                   BooleanValue (false),
                   MakeBooleanAccessor (&ModbusApplicationNew::fdi_flag),
                   MakeBooleanChecker ())
    .AddAttribute ("FdiID", "Int representing the ID of the FDI attacker, indexes into the config's FDI array",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ModbusApplicationNew::FDI_ID),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

ModbusApplicationNew::ModbusApplicationNew ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  mim_socket = 0;
  m_rand_delay_ns = CreateObject<UniformRandomVariable> ();
  m_rand_delay_ns->SetAttribute ("Min", DoubleValue (m_jitterMinNs));
  m_rand_delay_ns->SetAttribute ("Max", DoubleValue (m_jitterMaxNs));
  m_fdiRand = CreateObject<UniformRandomVariable> ();
}

ModbusApplicationNew::~ModbusApplicationNew ()
{
  NS_LOG_FUNCTION (this);
}

uint32_t
ModbusApplicationNew::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
ModbusApplicationNew::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket> >
ModbusApplicationNew::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void
ModbusApplicationNew::SetName (const std::string &name)
{
  m_name = name;
}

std::string
ModbusApplicationNew::GetName (void) const
{
  return m_name;
}

// ==================== from modbus-utilities.cc ====================
// -------------------------------------------------------------------
// CSVRow -- ported verbatim from dnp3-application-new.cc. Fully
// generic comma-split line reader; no DNP3-specific dependency.
// Must be defined before initConfig()'s usage in the final assembled
// .cc (file order matters if these stay as separate translation
// units glued together; a single header-declared class would avoid
// this, but DNP3 itself keeps it file-local, so we match that).
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
ModbusApplicationNew::readMicroGridConfig (std::string fpath, Json::Value& configobj)
{
  std::ifstream tifs (fpath);
  Json::Reader configreader;
  configreader.parse (tifs, configobj);
}

// GetStartStopArray: dead code in DNP3 too (defined, never called).
// No-op stub rather than a faithful port, since porting unused logic
// wastes effort and risks introducing bugs in code that will never
// run. Revisit if a real caller emerges.
void
ModbusApplicationNew::GetStartStopArray ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("ModbusApplication::GetStartStopArray: no-op (unused in DNP3 source; not ported)");
}

std::vector<std::string>
ModbusApplicationNew::get_val_vector (std::string delimiter, std::string m_attack_val)
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
ModbusApplicationNew::get_val (std::vector<std::string> val, std::vector<std::string> val_min,
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
          NS_LOG_INFO ("ModbusApplication::get_val: random selector " << r);
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
// GetVal -- same crash guard already applied to DNP3's version (see
// fix/dnp3-getval-crash: attack.find(key) instead of operator[], to
// avoid std::stof() on an empty string when a config key is missing).
// Written fresh here since this is a separate class, not shared code.
// -------------------------------------------------------------------
std::vector<float>
ModbusApplicationNew::GetVal (std::map<std::string, std::string> attack, std::string tag)
{
  std::vector<float> timer;
  std::string delimiter = ",";
  size_t pos = 0;
  std::string token;
  std::string key = "MIM-" + std::to_string (MIM_ID) + "-" + tag;

  auto it = attack.find (key);
  if (it == attack.end () || it->second.empty ())
    {
      NS_LOG_WARN ("ModbusApplication::GetVal: no value for key '" << key
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
// Point-map accessors -- m_deviceConfig is the single source of
// truth (address-keyed), used by both the Modbus protocol handlers
// (handle_normal/handle_MIM) and the HELICS-driven update path
// (store_points, below).
// -------------------------------------------------------------------
void
ModbusApplicationNew::SetHoldingRegister (uint16_t address, uint16_t value)
{
  m_deviceConfig.holdingRegisters[address] = value;
}

uint16_t
ModbusApplicationNew::GetHoldingRegister (uint16_t address) const
{
  auto it = m_deviceConfig.holdingRegisters.find (address);
  return (it != m_deviceConfig.holdingRegisters.end ()) ? it->second : 0;
}

void
ModbusApplicationNew::SetCoil (uint16_t address, bool value)
{
  m_deviceConfig.coils[address] = value;
}

bool
ModbusApplicationNew::GetCoil (uint16_t address) const
{
  auto it = m_deviceConfig.coils.find (address);
  return (it != m_deviceConfig.coils.end ()) ? it->second : false;
}

uint16_t
ModbusApplicationNew::GetRegisterScale (uint16_t address) const
{
  auto it = m_registerScale.find (address);
  return (it != m_registerScale.end ()) ? it->second : 1;
}

uint16_t
ModbusApplicationNew::GetFrozenHoldingRegister (uint16_t address) const
{
  auto it = m_frozenDeviceConfig.holdingRegisters.find (address);
  return (it != m_frozenDeviceConfig.holdingRegisters.end ()) ? it->second : 0;
}

bool
ModbusApplicationNew::GetFrozenCoil (uint16_t address) const
{
  auto it = m_frozenDeviceConfig.coils.find (address);
  return (it != m_frozenDeviceConfig.coils.end ()) ? it->second : false;
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

// Compromised-endpoint FDI: same shape as DNP3's apply_fdi -- resolves
// node_id/point_id/Value_attck/AttackChance attributes (set once at topology
// build time, no runtime JSON re-read) and returns a fabricated value when
// they match and the chance roll fires. m_attack_on (window) is checked by
// the caller. Restoration is automatic: once the window closes, the next
// real HELICS update simply overwrites the register again.
float
ModbusApplicationNew::apply_fdi (const std::string& name, float realValue)
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
// store_points -- called via HELICS's Store() (see DoEndpoint in the
// HELICS-layer file). DIVERGES from DNP3: translates point name to a
// Modbus address via analog_name_to_address/binary_name_to_address,
// then writes m_deviceConfig directly, rather than a separate
// name-keyed map (see file header note on why).
// -------------------------------------------------------------------
void
ModbusApplicationNew::store_points (std::string name, std::string value)
{
  auto analogIt = analog_name_to_address.find (name);
  if (analogIt != analog_name_to_address.end ())
    {
      uint16_t addr = analogIt->second;
      float v = static_cast<float>(std::atof (value.c_str ()));
      if (fdi_flag && m_attack_on)
        {
          v = apply_fdi (name, v);
        }
      SetHoldingRegister (addr, static_cast<uint16_t>(std::round (v / GetRegisterScale (addr))));
      return;
    }

  auto binaryIt = binary_name_to_address.find (name);
  if (binaryIt != binary_name_to_address.end ())
    {
      SetCoil (binaryIt->second, (value.compare ("CLOSED") == 0));
      return;
    }

  NS_LOG_INFO ("ModbusApplication::store_points: point not found: " << name);
}

void
ModbusApplicationNew::Store (std::string point, std::string value)
{
  if (!m_isMaster)
    {
      store_points (point, value);
    }
}

void
ModbusApplicationNew::set_attack (bool state)
{
  NS_LOG_INFO ("ModbusApplication::set_attack >>> Start Attack Mode: " << m_attackType);
  m_attack_on = state;

  // Option B fix (Sep 2026): apply_fdi was only ever wired into
  // store_points(), reached exclusively via the HELICS DoEndpoint path --
  // which never fires in this codebase (same fix already shipped for
  // GOOSE/DNP3). Apply FDI directly to this outstation's own holding
  // registers at the moment the attack window opens, so a fabricated value
  // actually reaches a poll response. Restoring on attack-end matters
  // because nothing else ever refreshes these values absent a real Store()
  // -- without it the fabricated value would persist past the window.
  // Guarded on fdi_flag specifically since set_attack is shared with the
  // separate rogue/MIM attack mechanism, which must not have its registers
  // touched.
  //
  // Unlike GOOSE/DNP3/MMS, Modbus has no live name-keyed analog value map
  // -- the real store is address-keyed (m_deviceConfig.holdingRegisters),
  // so analog_name_to_address bridges name (what apply_fdi matches on) to
  // address (what Get/SetHoldingRegister operate on).
  if (fdi_flag)
    {
      if (state)
        {
          for (const auto& entry : analog_name_to_address)
            {
              uint16_t addr = entry.second;
              uint16_t scale = GetRegisterScale (addr);
              float real = static_cast<float> (GetHoldingRegister (addr)) * scale;
              float v = apply_fdi (entry.first, real);
              SetHoldingRegister (addr, static_cast<uint16_t> (std::round (v / scale)));
              if (v != real)
                {
                  // Scale-fix validation evidence for the FDI fabrication path (see
                  // handle_MIM's identical readback check): confirms the register,
                  // once read back and re-scaled, reproduces the fabricated value
                  // apply_fdi actually returned, not a truncated/overflowed one.
                  float reconstructed = static_cast<float> (GetHoldingRegister (addr)) * scale;
                  std::cout << "ModbusApplication::set_attack: FDI register readback for "
                            << entry.first << " reconstructs to " << reconstructed
                            << " (scale " << scale << ")" << std::endl;
                }
            }
        }
      else
        {
          for (const auto& entry : m_preAttackAnalogValues)
            {
              auto it = analog_name_to_address.find (entry.first);
              if (it != analog_name_to_address.end ())
                {
                  uint16_t addr = it->second;
                  SetHoldingRegister (addr, static_cast<uint16_t> (std::round (entry.second / GetRegisterScale (addr))));
                }
            }
        }
    }
}

void
ModbusApplicationNew::set_respond (bool respond)
{
  NS_LOG_INFO ("ModbusApplication::set_respond");
  m_respond = respond;
}

void
ModbusApplicationNew::set_offline (bool offline)
{
  NS_LOG_INFO ("ModbusApplication::set_offline");
  if (m_isMaster)
    {
      NS_LOG_INFO ("Error: tried to set a master Modbus application offline; "
                   "only valid for outstation applications");
    }
  else
    {
      // Snapshot live values into m_frozenDeviceConfig at the moment of
      // the actual online->offline transition (guarded by !m_offline so
      // a redundant set_offline(true) call while already offline
      // doesn't overwrite the frozen snapshot with a later live state).
      // This mirrors DNP3's frozen_analog_points/frozen_bin_points
      // snapshot mechanism, using our own m_deviceConfig-shaped
      // storage instead of DNP3's separate name-keyed maps.
      if (offline && !m_offline)
        {
          m_frozenDeviceConfig = m_deviceConfig;
          NS_LOG_INFO ("ModbusApplication::set_offline: snapshotted "
                       << m_frozenDeviceConfig.holdingRegisters.size ()
                       << " registers and " << m_frozenDeviceConfig.coils.size ()
                       << " coils for offline mode");
        }
      m_offline = offline;
    }
}

void
ModbusApplicationNew::SetLocal (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void
ModbusApplicationNew::SetLocal (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void
ModbusApplicationNew::SetLocal (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = Address (ip);
  m_localPort = port;
}

void
ModbusApplicationNew::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  mim_socket = 0;
  m_socketList.clear ();
  Application::DoDispose ();
}

// -------------------------------------------------------------------
// resetToRealValue -- DIVERGES from DNP3's version, which calls
// m_p->direct_operate(...) (issuing an actual outbound library control
// command). Modbus has no such library; since the attack already
// mutated m_deviceConfig locally (see handle_MIM), resetting the real
// value is just writing it back to m_deviceConfig directly -- no
// outbound packet needed, since the *next* poll/read will pick up the
// restored value naturally. This is a simpler mechanism than DNP3's,
// enabled by not needing a library round-trip; flagged as a real
// design choice, not an oversight.
// -------------------------------------------------------------------
void
ModbusApplicationNew::resetToRealValue (int pointId, const std::string& realValue)
{
  double currentTime = Simulator::Now ().GetSeconds ();
  uint16_t address = static_cast<uint16_t>(pointId);

  if (realValue.empty ())
    {
      NS_LOG_INFO ("ModbusApplication::resetToRealValue: no real value provided for point "
                   << pointId << " at time " << currentTime << "s");
      return;
    }

  bool isNumeric = std::find_if (realValue.begin (), realValue.end (),
                                  [](unsigned char c) { return !std::isdigit (c); }) == realValue.end ();

  if (isNumeric)
    {
      try
        {
          float f = std::stof (realValue);
          SetHoldingRegister (address, static_cast<uint16_t>(std::round (f / GetRegisterScale (address))));
          NS_LOG_INFO ("ModbusApplication::resetToRealValue: reset register " << pointId
                       << " to " << f << " at time " << currentTime << "s");
        }
      catch (const std::exception& e)
        {
          NS_LOG_WARN ("ModbusApplication::resetToRealValue: error resetting register "
                       << pointId << ": " << e.what () << " at time " << currentTime << "s");
        }
    }
  else
    {
      // Binary point: DNP3 distinguishes TRIP/CLOSE/LATCH_ON/LATCH_OFF
      // via ControlOutputRelayBlock::Code (a library type). Modbus
      // coils are inherently binary, so we collapse to a boolean --
      // same simplification already flagged in modbus-handle-mim.cc
      // for attack type 3. CLOSE and LATCH_ON both map to true;
      // TRIP and LATCH_OFF both map to false.
      bool coilState = (realValue.find ("CLOSE") != std::string::npos
                         || realValue.find ("LATCH_ON") != std::string::npos);
      SetCoil (address, coilState);
      NS_LOG_INFO ("ModbusApplication::resetToRealValue: reset coil " << pointId
                   << " to " << (coilState ? "ON" : "OFF") << " (from \"" << realValue
                   << "\") at time " << currentTime << "s");
    }
}

// -- Analog points that must stay exact integers (discrete regulator --
// -- step positions), rather than the x4-scaled continuous quantities --
// -- everything else gets -- see m_registerScale / GetRegisterScale. --
// -- Point names are "objId$variable" (optionally "$variable.real"/  --
// -- ".imag"); tap/capacitor points are never suffixed, so a plain    --
// -- suffix match on the substring after the last '$' is sufficient. --
static bool
IsUnscaledAnalogPoint (const std::string& pointName)
{
  auto delimPos = pointName.rfind ('$');
  std::string variable = (delimPos != std::string::npos) ? pointName.substr (delimPos + 1) : pointName;
  return variable == "tap_A" || variable == "tap_B" || variable == "tap_C"
      || variable == "capacitor_A" || variable == "capacitor_B" || variable == "capacitor_C";
}

// ==================== from modbus-init-config.cc ====================
void
ModbusApplicationNew::initConfig (void)
{
  NS_LOG_FUNCTION (this);
  std::cout << points_filename << std::endl;
  std::ifstream pointsFile (points_filename, std::ifstream::in);

  uint16_t nextAnalogAddress = 0;
  uint16_t nextBinaryAddress = 0;

  if (pointsFile)
    {
      bool isValid;
      CSVRow row;
      int i = 0;
      while (pointsFile.good ())
        {
          row.readNextRow (pointsFile);
          if (row.size () > 0)
            {
              if (row.geti (0).compare ("ANALOG") == 0)
                {
                  NS_LOG_INFO ("Adding Analog: " << ((std::string) row.geti (1)));
                  isValid = true;
                  std::string pointName = (std::string) row.geti (1);
                  analog_points.insert (std::make_pair (pointName, std::stof (row.geti (2))));
                  analog_point_names.push_back (pointName);

                  uint16_t addr = nextAnalogAddress++;
                  analog_name_to_address[pointName] = addr;
                  if (!IsUnscaledAnalogPoint (pointName))
                    {
                      m_registerScale[addr] = 4;
                    }
                  float initialValue = std::stof (row.geti (2));
                  m_deviceConfig.holdingRegisters[addr] =
                    static_cast<uint16_t>(std::round (initialValue / GetRegisterScale (addr)));
                }
              else if (row.geti (0).compare ("BINARY") == 0)
                {
                  NS_LOG_INFO ("Adding Binary: " << ((std::string) row.geti (1)));
                  isValid = true;
                  std::string pointName = (std::string) row.geti (1);
                  bin_points[pointName] = std::stoi (row.geti (2));
                  binary_point_names.push_back (pointName);

                  uint16_t addr = nextBinaryAddress++;
                  binary_name_to_address[pointName] = addr;
                  m_deviceConfig.coils[addr] = (std::stoi (row.geti (2)) != 0);
                }
              else
                {
                  NS_LOG_INFO ("Invalid row " << row.geti (1).c_str ());
                  isValid = false;
                }

              // NOTE: DNP3's version has additional logic here deriving a
              // HELICS endpoint name from points_filename (extracting a
              // node name between two '-' characters) for
              // helics_federate->registerEndpoint(...), currently
              // commented out in the DNP3 source itself. Not porting
              // that here until we confirm it's actually live/needed --
              // flagging so it isn't silently dropped if it turns out
              // to matter for HELICS federation setup.
              (void) isValid; // suppress unused-variable warning until the above is resolved

              i++;
            }
        }
    }
  else
    {
      NS_LOG_INFO ("Unable to open points file:" << points_filename);
      exit (-1);
    }

  for (const auto& entry : analog_name_to_address)
    {
      m_preAttackAnalogValues[entry.first] =
        static_cast<float> (GetHoldingRegister (entry.second)) * GetRegisterScale (entry.second);
    }

  NS_LOG_INFO ("ModbusApplication::initConfig: loaded " << analog_point_names.size ()
               << " analog points (holding registers 0-" << (nextAnalogAddress > 0 ? nextAnalogAddress - 1 : 0)
               << ") and " << binary_point_names.size ()
               << " binary points (coils 0-" << (nextBinaryAddress > 0 ? nextBinaryAddress - 1 : 0) << ")");
}

// ==================== from modbus-pdu-codec.cc ====================
namespace {

// -- big-endian helpers: Modbus is big-endian, host may not be --
uint16_t ReadU16BE (const uint8_t* p)
{
  return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

void WriteU16BE (std::vector<uint8_t>& buf, uint16_t val)
{
  buf.push_back (static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back (static_cast<uint8_t>(val & 0xFF));
}

// Modbus exception codes (subset relevant to our supported function codes)
enum class ModbusException : uint8_t {
  ILLEGAL_FUNCTION      = 0x01,
  ILLEGAL_DATA_ADDRESS  = 0x02,
  ILLEGAL_DATA_VALUE    = 0x03,
};

} // anonymous namespace

// -------------------------------------------------------------------
// DecodePDU
//
// Parses a raw ns-3 Packet containing a full Modbus TCP ADU (MBAP +
// PDU) into a ModbusPDU struct. Returns a PDU with functionCode set to
// the request's function code; on malformed input, sets quantity=0 and
// leaves data empty as a signal to the caller to not process further.
//
// NOTE: unlike DNP3's link-layer framing, Modbus TCP has no CRC/escape
// byte scheme -- TCP already guarantees byte-accurate delivery, so this
// function only needs to interpret field boundaries, not validate
// checksums.
// -------------------------------------------------------------------
ModbusPDU
ModbusApplicationNew::DecodePDU (Ptr<Packet> packet, bool isResponse)
{
  ModbusPDU pdu;
  uint32_t size = packet->GetSize ();

  // Minimum valid ADU: 7-byte MBAP + 1-byte function code = 8 bytes
  if (size < 8)
    {
      NS_LOG_WARN ("DecodePDU: packet too short (" << size << " bytes), discarding");
      pdu.quantity = 0;
      return pdu;
    }

  std::vector<uint8_t> buf (size);
  packet->CopyData (buf.data (), size);

  // MBAP header
  // bytes 0-1: transactionId, bytes 2-3: protocolId, bytes 4-5: length, byte 6: unitId
  uint16_t protocolId = ReadU16BE (&buf[2]);
  uint16_t length = ReadU16BE (&buf[4]);

  if (protocolId != 0)
    {
      NS_LOG_WARN ("DecodePDU: unexpected protocolId " << protocolId << " (Modbus TCP requires 0), discarding");
      pdu.quantity = 0;
      return pdu;
    }

  // length covers unitId + PDU; sanity-check it against actual packet size
  if (static_cast<uint32_t>(length) + 6 != size)
    {
      NS_LOG_WARN ("DecodePDU: MBAP length field (" << length
                   << ") inconsistent with packet size (" << size << "), discarding");
      pdu.quantity = 0;
      return pdu;
    }

  uint8_t functionCode = buf[7];
  const uint8_t* data = &buf[8];
  uint32_t dataLen = size - 8;

  switch (functionCode)
    {
      case static_cast<uint8_t>(ModbusFunctionCode::READ_COILS):
      case static_cast<uint8_t>(ModbusFunctionCode::READ_HOLDING_REGISTERS):
        {
          pdu.functionCode = static_cast<ModbusFunctionCode>(functionCode);

          if (isResponse)
            {
              // BUG FIX: response wire format is completely different
              // from the request -- per spec section 6.1/6.3, a
              // Read Coils / Read Holding Registers RESPONSE is
              // byteCount(1) + data(N bytes), NOT address+quantity.
              // Previously this function always assumed the request
              // shape regardless of direction, so the master's
              // handle_normal branch always got an empty pdu.data
              // when decoding a response -- no crash, just silently
              // zero register/coil values ever extracted. Caught via
              // byte-level cross-check against the official Modbus
              // Application Protocol Specification V1.1b3.
              if (dataLen < 1)
                {
                  NS_LOG_WARN ("DecodePDU: response too short for byte count, discarding");
                  pdu.quantity = 0;
                  return pdu;
                }
              uint8_t byteCount = data[0];
              if (dataLen < static_cast<uint32_t>(1 + byteCount))
                {
                  NS_LOG_WARN ("DecodePDU: response byte count (" << static_cast<int>(byteCount)
                               << ") exceeds actual data available, discarding");
                  pdu.quantity = 0;
                  return pdu;
                }
              pdu.data.assign (&data[1], &data[1 + byteCount]);
              // quantity isn't carried in the response wire format;
              // set it to the decoded byte count so callers can still
              // use "quantity == 0" as a rough malformed-response
              // signal without misinterpreting it as an address field.
              pdu.quantity = byteCount;
            }
          else
            {
              // Request form: starting address (2) + quantity (2)
              if (dataLen < 4)
                {
                  NS_LOG_WARN ("DecodePDU: read request too short, discarding");
                  pdu.quantity = 0;
                  return pdu;
                }
              pdu.address = ReadU16BE (&data[0]);
              pdu.quantity = ReadU16BE (&data[2]);
            }
          break;
        }

      case static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_COIL):
        {
          // Per spec section 6.5, the response is an exact echo of the
          // request (address(2) + value(2)) -- same shape either way,
          // no isResponse branching needed here.
          if (dataLen < 4)
            {
              NS_LOG_WARN ("DecodePDU: write-coil request too short, discarding");
              pdu.quantity = 0;
              return pdu;
            }
          pdu.functionCode = ModbusFunctionCode::WRITE_SINGLE_COIL;
          pdu.address = ReadU16BE (&data[0]);
          pdu.value = ReadU16BE (&data[2]);
          break;
        }

      case static_cast<uint8_t>(ModbusFunctionCode::WRITE_SINGLE_REGISTER):
        {
          // Per spec section 6.6, same echo-shape reasoning as above.
          if (dataLen < 4)
            {
              NS_LOG_WARN ("DecodePDU: write-register request too short, discarding");
              pdu.quantity = 0;
              return pdu;
            }
          pdu.functionCode = ModbusFunctionCode::WRITE_SINGLE_REGISTER;
          pdu.address = ReadU16BE (&data[0]);
          pdu.value = ReadU16BE (&data[2]);
          break;
        }

      default:
        NS_LOG_WARN ("DecodePDU: unsupported function code 0x"
                     << std::hex << static_cast<int>(functionCode) << std::dec
                     << " -- only Read Coils (0x01), Read Holding Registers (0x03),"
                     << " Write Single Coil (0x05), Write Single Register (0x06) are implemented");
        pdu.quantity = 0;
        return pdu;
    }

  return pdu;
}

// -------------------------------------------------------------------
// EncodePDU
//
// Builds a full Modbus TCP ADU (MBAP + PDU) from a ModbusPDU struct.
// Used both for requests (master -> outstation) and responses
// (outstation -> master); the caller is responsible for populating
// `pdu.data` with the correct response payload before calling this
// (e.g. register values already read from m_deviceConfig).
//
// transactionId is not tracked here -- caller should set it via a
// wrapper if strict request/response matching is needed; defaulting to
// 0 is acceptable for now since our topology is a single master/
// outstation pair per socket, not a multiplexed one.
// -------------------------------------------------------------------
Ptr<Packet>
ModbusApplicationNew::EncodePDU (const ModbusPDU& pdu, uint8_t unitId)
{
  std::vector<uint8_t> adu;

  // Placeholder MBAP header -- transactionId and length filled in below
  // once we know the PDU body size.
  std::vector<uint8_t> pduBytes;

  if (pdu.isException)
    {
      // Per spec: exception response is original function code | 0x80,
      // followed by a single exception code byte. This is checked
      // before the normal switch below so a handler can flag an
      // otherwise-valid function code as illegal for this specific
      // request (e.g. WRITE_SINGLE_COIL with a value other than
      // 0xFF00/0x0000) without needing to fall through the
      // unsupported-function-code default case, which always uses
      // ILLEGAL_FUNCTION and would report the wrong exception type.
      pduBytes.push_back (static_cast<uint8_t>(pdu.functionCode) | 0x80);
      pduBytes.push_back (pdu.exceptionCode);

      uint16_t length = static_cast<uint16_t>(1 + pduBytes.size ());
      WriteU16BE (adu, 0);
      WriteU16BE (adu, 0);
      WriteU16BE (adu, length);
      adu.push_back (unitId);
      adu.insert (adu.end (), pduBytes.begin (), pduBytes.end ());
      return Create<Packet> (adu.data (), adu.size ());
    }
  pduBytes.push_back (static_cast<uint8_t>(pdu.functionCode));

  switch (pdu.functionCode)
    {
      case ModbusFunctionCode::READ_COILS:
      case ModbusFunctionCode::READ_HOLDING_REGISTERS:
        {
          if (!pdu.data.empty ())
            {
              // Response form: byte count (1) + packed data
              pduBytes.push_back (static_cast<uint8_t>(pdu.data.size ()));
              pduBytes.insert (pduBytes.end (), pdu.data.begin (), pdu.data.end ());
            }
          else
            {
              // Request form: starting address (2) + quantity (2)
              WriteU16BE (pduBytes, pdu.address);
              WriteU16BE (pduBytes, pdu.quantity);
            }
          break;
        }

      case ModbusFunctionCode::WRITE_SINGLE_COIL:
      case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
        {
          // Same wire form for request and response (echo): address (2) + value (2)
          WriteU16BE (pduBytes, pdu.address);
          WriteU16BE (pduBytes, pdu.value);
          break;
        }

      default:
        NS_LOG_WARN ("EncodePDU: unsupported function code, sending exception response");
        pduBytes.clear ();
        pduBytes.push_back (static_cast<uint8_t>(pdu.functionCode) | 0x80);
        pduBytes.push_back (static_cast<uint8_t>(ModbusException::ILLEGAL_FUNCTION));
        break;
    }

  uint16_t length = static_cast<uint16_t>(1 + pduBytes.size ()); // unitId + PDU

  WriteU16BE (adu, 0);        // transactionId (default 0; see note above)
  WriteU16BE (adu, 0);        // protocolId, always 0 for Modbus TCP
  WriteU16BE (adu, length);
  adu.push_back (unitId);
  adu.insert (adu.end (), pduBytes.begin (), pduBytes.end ());

  return Create<Packet> (adu.data (), adu.size ());
}

// ==================== from modbus-lifecycle.cc ====================
void
ModbusApplicationNew::StartApplication ()
{
  NS_LOG_FUNCTION (this);
  running = true;
  m_attack_on = false;
  if (fdi_flag) {
    // Unlike handle_MIM (reactive to packet arrival), FDI has a fixed window on the
    // outstation itself, so it can be scheduled once here instead of deduped via
    // StartVect/StopVect on every store_points() call. See DNP3's identical addition.
    double attackStart = safeStod (m_attackStartTime, -1.0, "FDI AttackStartTime");
    double attackEnd = safeStod (m_attackEndTime, -1.0, "FDI AttackEndTime");
    if (attackStart < 0.0 || attackEnd < 0.0)
      {
        std::cerr << "[WARN] " << "ModbusApplicationNew::StartApplication: bad FDI AttackStartTime/"
                     "AttackEndTime config value for node " << m_name
                     << " -- FDI disabled for this instance." << std::endl;
      }
    else
      {
        Simulator::Schedule(Seconds(attackStart), &ModbusApplicationNew::set_attack, this, true);
        Simulator::Schedule(Seconds(attackEnd), &ModbusApplicationNew::set_attack, this, false);
      }
  }

  // Modbus TCP is TCP by definition; the EnableTCP attribute is kept
  // for interface consistency with DNP3 but always resolves to TCP
  // here (no "Modbus UDP" concept exists in the spec, unlike DNP3
  // which genuinely supports both transports).
  if (!m_enableTcp)
    {
      NS_LOG_WARN ("ModbusApplication: EnableTCP=false requested, but Modbus TCP "
                   "has no UDP variant -- proceeding with TCP anyway.");
    }
  makeTcpConnection ();
}

void
ModbusApplicationNew::makeTcpConnection (void)
{
  NS_LOG_FUNCTION (this);

  // BUG FIX: mim_socket used to be built and Connect()'d for every
  // application instance, master and outstation alike. That's fine for
  // DNP3/UDP (Connect() on a UDP socket just sets a default destination,
  // effectively a no-op if unused) but not for Modbus/TCP, where
  // Connect() is a real handshake attempt -- the exact distinction
  // called out in the outstation Connect()/Listen() fix below. Plain
  // master/outstation instances never set RemoteAddress2, so it silently
  // defaulted to 10.0.0.0 (see GetTypeId), meaning every ordinary node
  // in a real topology was generating a spurious SYN to a bogus address
  // on every run -- traffic that ends up in FlowMonitor stats alongside
  // genuine flow-level IDS features. Only Inside/MIM-role instances
  // actually use this socket, so gate its construction on the same role
  // check already used elsewhere in this function.
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

      // MIM/insider socket, ported from DNP3's makeUdpConnection (see
      // file header note on why this logic moved here from DNP3's UDP
      // path). Bound to m_remoteAddress2/m_remotePort, same pattern.
      // Only built for Inside/MIM-role instances -- see isInsiderOrMim
      // note above.
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

  m_socket->SetRecvCallback (MakeCallback (&ModbusApplicationNew::HandleRead, this));
  if (mim_socket)
    {
      mim_socket->SetRecvCallback (MakeCallback (&ModbusApplicationNew::HandleRead, this));
    }

  // -- Attack start/end time scheduling, ported near-verbatim from --
  // -- DNP3's makeUdpConnection (generic string parsing + Simulator:: --
  // -- Schedule calls; no protocol-specific dependency) --
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
          startMaster ();
          if (isInsiderOrMim)
            {
              NS_LOG_UNCOND ("ModbusApplication: I'm MIM or Insider Master");
              if (timer[index])
                {
                  Simulator::Schedule (Seconds (timer[index]), &ModbusApplicationNew::set_attack, this, true);
                  StartVect.push_back (std::to_string (timer[index]));
                }
              if (timer_end[index])
                {
                  Simulator::Schedule (Seconds (timer_end[index]), &ModbusApplicationNew::set_attack, this, false);
                  StopVect.push_back (std::to_string (timer_end[index]));
                }
            }
        }
      else
        {
          if (isInsiderOrMim)
            {
              startMaster ();
              NS_LOG_UNCOND ("ModbusApplication: I'm MIM or Insider Outstation");
              if (timer[index])
                {
                  Simulator::Schedule (Seconds (timer[index]), &ModbusApplicationNew::set_attack, this, true);
                  StartVect.push_back (std::to_string (timer[index]));
                }
              if (timer_end[index])
                {
                  Simulator::Schedule (Seconds (timer_end[index]), &ModbusApplicationNew::set_attack, this, false);
                  StopVect.push_back (std::to_string (timer_end[index]));
                }
            }

          // BUG FIX: the outstation must NOT call m_socket->Connect()
          // here. This was inherited from porting DNP3's UDP-based
          // connection logic, where Connect() on a UDP socket just sets
          // a default destination address (harmless, no real
          // handshake). For TCP, Connect() actively initiates a 3-way
          // handshake -- calling both Connect() AND Listen() on the
          // same socket object (Listen() happens a few lines below,
          // unconditionally, for the outstation) is invalid TCP socket
          // usage. The outstation's m_socket must only ever Listen()
          // and accept inbound connections; it never actively connects
          // out itself. Caught via std::cerr tracing: Socket::Send()
          // on the master reported success every time, yet
          // HandleAccept/handle_normal never fired on the outstation
          // and no perf.txt was ever created -- this conflicting
          // Connect()+Listen() usage on the same socket is the reason.
          //
          // mim_socket is now only constructed for Inside/MIM-role
          // instances (see isInsiderOrMim note above), so guard the
          // Connect() call with a null check rather than assuming it
          // exists.
          if (mim_socket)
            {
              mim_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_remoteAddress2), m_localPort));
            }
          startOutstation (m_socket);
          if (isInsiderOrMim)
            {
              startOutstation (mim_socket);
            }
        }
    }

  m_socket->SetAcceptCallback (
    MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
    MakeCallback (&ModbusApplicationNew::HandleAccept, this));
  m_socket->SetCloseCallbacks (
    MakeCallback (&ModbusApplicationNew::HandlePeerClose, this),
    MakeCallback (&ModbusApplicationNew::HandlePeerError, this));

  if (m_isMaster)
    {
      // NOTE: this was previously Seconds(10) -- an arbitrary value set
      // without cross-checking it against the scenario's own poll-start
      // timing. Since periodic_poll's first call is typically scheduled
      // around t=1.005s (see ns3-modbus-minimal-test.cc), a 10-second
      // connection delay meant every poll attempt before t=10s tried to
      // Send() on a socket that had never even attempted to connect.
      // Reduced to well before any reasonable poll-start time; TCP
      // handshake over a simple point-to-point link with a few ms of
      // delay should complete essentially instantly, so this margin is
      // generous, not tight. Caught via std::cerr tracing: periodic_poll
      // fired correctly with a nonzero point count, but handle_normal on
      // the outstation never fired and no perf.txt was ever created.
      Simulator::Schedule (MilliSeconds (100), &ModbusApplicationNew::ConnectToPeer, this, m_socket, m_remotePort);
    }
  else
    {
      int listenResult = m_socket->Listen ();
      if (listenResult != 0)
        {
          NS_LOG_WARN ("ModbusApplication: '" << m_name << "' Listen() failed");
        }
    }
}

void
ModbusApplicationNew::ConnectToPeer (Ptr<Socket> localSocket, uint16_t servPort)
{
  NS_LOG_INFO ("ModbusApplication: connecting to remote " << m_remoteAddress);

  m_socket->SetConnectCallback (
    MakeCallback (&ModbusApplicationNew::HandleConnectionSucceeded, this),
    MakeCallback (&ModbusApplicationNew::HandleConnectionFailed, this));

  m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_remoteAddress), m_remotePort));
  // mim_socket is only constructed for Inside/MIM-role instances (see
  // makeTcpConnection's isInsiderOrMim note); guard rather than assume.
  if (mim_socket)
    {
      mim_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (m_remoteAddress2), m_localPort));
    }
}

void
ModbusApplicationNew::HandleConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_INFO ("ModbusApplication: '" << m_name << "' TCP connection succeeded at t="
               << Simulator::Now ().GetSeconds () << "s");
}

void
ModbusApplicationNew::HandleConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_WARN ("ModbusApplication: '" << m_name << "' TCP connection failed at t="
               << Simulator::Now ().GetSeconds () << "s");
}

void
ModbusApplicationNew::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  running = false;
  NS_LOG_INFO ("ModbusApplication: closing application");

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
ModbusApplicationNew::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  s->SetRecvCallback (MakeCallback (&ModbusApplicationNew::HandleRead, this));
  m_socketList.push_back (s);
  startOutstation (s);
  NS_LOG_INFO ("ModbusApplication: in HandleAccept");

  // std::cout, not NS_LOG_*: compiled out in this build's optimized
  // profile (see feedback_ns3_build_environment_gotchas.md). Tracks
  // slow-DDoS connection-exhaustion evidence: this list has no cap and no
  // idle timeout, so its size should grow and hold for the attack window.
  std::cout << "ModbusApplication: '" << m_name << "' accepted connection at t="
            << Simulator::Now ().GetSeconds () << "s, m_socketList.size()="
            << m_socketList.size () << std::endl;
}

void
ModbusApplicationNew::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << m_name << socket);
}

void
ModbusApplicationNew::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

// -------------------------------------------------------------------
// startMaster / startOutstation
//
// DNP3's equivalents construct objects against the vendored Master/
// Outstation/Endpoint library (MasterConfig, StationConfig,
// DatalinkConfig, RemoteDevice, deviceMap -- all library types with no
// Modbus equivalent). Since our handle_normal/handle_MIM already
// perform the actual request/response logic directly against
// m_deviceConfig, these become minimal role-marking hooks rather than
// object-construction sites. If per-role setup needs grow (e.g.
// tracking multiple simultaneous outstations), this is the place to
// expand -- currently intentionally thin, not an oversight.
// -------------------------------------------------------------------
void
ModbusApplicationNew::startMaster ()
{
  NS_LOG_FUNCTION (this);
  debugLevel = 0;
  // No library object construction needed -- master-side request
  // issuance happens via ReadHoldingRegisters/ReadCoils/
  // WriteSingleRegister/WriteSingleCoil, called from periodic_poll.
  //
  // CRITICAL FIX: initConfig() must be called here too, not just in
  // startOutstation(). Without it, the master's m_deviceConfig
  // (holdingRegisters/coils) stays completely empty forever, since
  // nothing else populates it -- meaning periodic_poll's own
  // analogCount/coilCount checks always see zero points to poll, so
  // the master silently never sends any request at all. Caught via
  // std::cerr tracing during the first successful (non-crashing) run:
  // periodic_poll fired correctly on schedule, but with
  // analogCount=0/coilCount=0 every time, so handle_normal on the
  // outstation side never received anything and perf.txt was never
  // created. The master needs its own copy of the points file (same
  // PointsFilename the scenario sets on both helpers) so it knows
  // what address range to poll.
  initConfig ();
}

void
ModbusApplicationNew::startOutstation (Ptr<Socket> sock)
{
  NS_LOG_FUNCTION (this << sock);
  // No library object construction needed -- outstation-side request
  // handling happens via handle_normal/handle_MIM, triggered by
  // HandleRead on this socket.
  //
  // CRITICAL: initConfig() and the m_respond/m_offline defaults below
  // are NOT optional bookkeeping -- DNP3's full startOutstation calls
  // initConfig() and explicitly sets m_respond=true/m_offline=false.
  // Without this, handle_normal's "if (!m_respond) ignore request"
  // check means the outstation silently ignores every request forever.
  // (Caught during review -- an earlier draft of this function omitted
  // these three lines entirely.)
  //
  // NOTE: this is called once per accepted connection (see
  // HandleAccept), same as DNP3's original -- meaning initConfig()
  // would re-read the points file and reset m_deviceConfig to initial
  // values on every new connection, discarding any live updates from
  // prior Modbus writes or HELICS-driven changes. This is inherited
  // DNP3 behavior, not something introduced here, but is only safe if
  // the topology genuinely has one connection per outstation (as
  // NATIG's existing topology appears to assume) -- worth revisiting
  // if that assumption changes.
  initConfig ();
  m_respond = true;
  m_offline = false;
}

// ==================== from modbus-handleread-send.cc ====================
void
ModbusApplicationNew::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("ModbusApplication::HandleRead for " << m_name);

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
ModbusApplicationNew::send_directly (Ptr<Packet> p)
{
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

  // Modbus TCP only -- no SendTo()/UDP branch (unlike DNP3's version,
  // which supports both transports).
  int (Socket::*fp)(Ptr<Packet>, uint32_t) = &Socket::Send;
  Simulator::Schedule (NanoSeconds (delay_ns), fp, m_socket, p, 0);
}

void
ModbusApplicationNew::send_directly_server (Ptr<Socket> sock, Ptr<Packet> p)
{
  // BUG FIX: this previously hardcoded sending via mim_socket
  // regardless of caller -- but handle_normal's outstation response
  // path needs to reply on the SAME accepted-connection socket the
  // request arrived on (the `socket` parameter HandleRead/handle_normal
  // actually received from HandleAccept), not the insider/MIM socket,
  // which is a separate connection to an unrelated address. Caught
  // after confirming requests were being delivered successfully (via
  // perf.txt and handle_normal tracing) but responses never reached
  // the master -- the response was being sent out the wrong socket
  // entirely.
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

  int (Socket::*fp)(Ptr<Packet>, uint32_t) = &Socket::Send;
  Simulator::Schedule (NanoSeconds (delay_ns), fp, sock, p, 0);
}

void
ModbusApplicationNew::Record (Ptr<Packet> packet, Address from)
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

// ==================== from modbus-handle-normal.cc ====================
void
ModbusApplicationNew::handle_normal (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;

  NS_LOG_LOGIC ("ModbusApplication:: handle_normal");

  while ((packet = socket->RecvFrom (from)))
    {
      m_txTrace (packet);
      NS_LOG_INFO ("ModbusApplication:: >>> address: "
                   << InetSocketAddress::ConvertFrom (from).GetIpv4 ());

      if (m_isMaster)
        {
          // Master side: this packet is a response to a prior request.
          // Decode it and log/store the returned values. (Full
          // master-side response correlation -- e.g. matching against
          // an outstanding request queue -- is not yet implemented;
          // this handles the single-outstanding-request case, which
          // matches how periodic_poll currently issues one request at
          // a time.)
          ModbusPDU responsePdu = DecodePDU (packet, /* isResponse */ true);

          if (responsePdu.quantity == 0 && responsePdu.data.empty ())
            {
              NS_LOG_WARN ("ModbusApplication (master): malformed or unsupported response, discarding");
              Record (packet, from);
              continue;
            }

          switch (responsePdu.functionCode)
            {
              case ModbusFunctionCode::READ_HOLDING_REGISTERS:
                {
                  // data is packed as 2 bytes per register, big-endian
                  for (size_t i = 0; i + 1 < responsePdu.data.size (); i += 2)
                    {
                      uint16_t val = (static_cast<uint16_t>(responsePdu.data[i]) << 8)
                                     | static_cast<uint16_t>(responsePdu.data[i + 1]);
                      NS_LOG_INFO ("ModbusApplication (master): received register value " << val);
                    }
                  break;
                }
              case ModbusFunctionCode::READ_COILS:
                {
                  // data is packed as bits, LSB-first within each byte
                  for (size_t i = 0; i < responsePdu.data.size (); i++)
                    {
                      NS_LOG_INFO ("ModbusApplication (master): received coil byte 0x"
                                   << std::hex << static_cast<int>(responsePdu.data[i]) << std::dec);
                    }
                  break;
                }
              case ModbusFunctionCode::WRITE_SINGLE_COIL:
              case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
                {
                  NS_LOG_INFO ("ModbusApplication (master): write confirmed at address "
                               << responsePdu.address << ", value " << responsePdu.value);
                  break;
                }
              default:
                break;
            }
        }
      else
        {
          // Outstation side: this packet is a request. Decode, look up
          // against our point maps, and send a response.
          if (!m_respond)
            {
              NS_LOG_LOGIC ("ModbusApplication (outstation): m_respond is false, ignoring request");
              Record (packet, from);
              continue;
            }

          ModbusPDU requestPdu = DecodePDU (packet);

          if (requestPdu.quantity == 0 && requestPdu.data.empty ()
              && requestPdu.functionCode != ModbusFunctionCode::WRITE_SINGLE_COIL
              && requestPdu.functionCode != ModbusFunctionCode::WRITE_SINGLE_REGISTER)
            {
              NS_LOG_WARN ("ModbusApplication (outstation): malformed or unsupported request, no response sent");
              Record (packet, from);
              continue;
            }

          ModbusPDU responsePdu;
          responsePdu.functionCode = requestPdu.functionCode;
          responsePdu.address = requestPdu.address;

          switch (requestPdu.functionCode)
            {
              case ModbusFunctionCode::READ_HOLDING_REGISTERS:
                {
                  for (uint16_t i = 0; i < requestPdu.quantity; i++)
                    {
                      uint16_t addr = requestPdu.address + i;
                      uint16_t val = (m_offline)
                        ? GetFrozenHoldingRegister (addr)
                        : GetHoldingRegister (addr);
                      responsePdu.data.push_back (static_cast<uint8_t>((val >> 8) & 0xFF));
                      responsePdu.data.push_back (static_cast<uint8_t>(val & 0xFF));
                    }
                  break;
                }

              case ModbusFunctionCode::READ_COILS:
                {
                  // NOTE: previously had no offline handling at all
                  // (always returned live values) -- inconsistent with
                  // READ_HOLDING_REGISTERS's intent. Fixed alongside
                  // the frozen-snapshot mechanism for consistency.
                  uint8_t currentByte = 0;
                  int bitPos = 0;
                  for (uint16_t i = 0; i < requestPdu.quantity; i++)
                    {
                      uint16_t addr = requestPdu.address + i;
                      bool val = (m_offline) ? GetFrozenCoil (addr) : GetCoil (addr);
                      if (val)
                        {
                          currentByte |= (1 << bitPos);
                        }
                      bitPos++;
                      if (bitPos == 8)
                        {
                          responsePdu.data.push_back (currentByte);
                          currentByte = 0;
                          bitPos = 0;
                        }
                    }
                  if (bitPos > 0)
                    {
                      responsePdu.data.push_back (currentByte);
                    }
                  break;
                }

              case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
                {
                  SetHoldingRegister (requestPdu.address, requestPdu.value);
                  responsePdu.value = requestPdu.value; // echo, per spec
                  break;
                }

              case ModbusFunctionCode::WRITE_SINGLE_COIL:
                {
                  // Per spec section 6.5: only 0xFF00 (ON) and 0x0000
                  // (OFF) are valid; all other values are illegal.
                  // Previously any non-0xFF00 value was silently
                  // treated as OFF -- not incorrect for well-formed
                  // traffic, but not spec-compliant for malformed/
                  // fuzzed input, which matters for attack-surface
                  // testing.
                  if (requestPdu.value == 0xFF00 || requestPdu.value == 0x0000)
                    {
                      bool coilVal = (requestPdu.value == 0xFF00);
                      SetCoil (requestPdu.address, coilVal);
                      responsePdu.value = requestPdu.value; // echo, per spec
                    }
                  else
                    {
                      NS_LOG_WARN ("ModbusApplication (outstation): illegal coil value 0x"
                                   << std::hex << requestPdu.value << std::dec
                                   << " for WRITE_SINGLE_COIL, sending exception response");
                      responsePdu.isException = true;
                      responsePdu.exceptionCode = static_cast<uint8_t>(ModbusException::ILLEGAL_DATA_VALUE);
                    }
                  break;
                }

              default:
                NS_LOG_WARN ("ModbusApplication (outstation): reached default case unexpectedly");
                break;
            }

          Ptr<Packet> responsePacket = EncodePDU (responsePdu, m_deviceConfig.unitId);
          send_directly_server (socket, responsePacket);
        }

      Record (packet, from);
    }
}

// ==================== from modbus-handle-mim.cc ====================
void
ModbusApplicationNew::handle_MIM (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  Address sourceAddr;
  socket->GetSockName (sourceAddr);

  while ((packet = socket->RecvFrom (from)))
    {
      m_txTrace (packet);
      NS_LOG_INFO ("ModbusApplication::handle_MIM >>> processing packet at time "
                   << Simulator::Now ().GetSeconds () << "s");

      // -- Decode the underlying Modbus request first; we need its --
      // -- address/function code to know which point is being touched --
      ModbusPDU requestPdu = DecodePDU (packet);

      // -- Parse attack config and node/point mappings --
      // (ported near-verbatim from DNP3's handle_MIM -- this is generic
      // string/JSON parsing with no protocol-specific dependency)
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

      // NOTE: analog_point_names / binary_point_names are populated from
      // points_filename in initConfig() (mirroring DNP3's equivalent
      // load), which we have not yet written for Modbus. The mapping
      // logic below assumes the same points-file format DNP3 uses;
      // this needs to be verified once we look at that file, since
      // Modbus's analog/binary split maps to holding-registers/coils,
      // not DNP3's original point-type scheme.
      std::vector<int> ID_point;
      std::vector<std::string> pointID;
      // BUG FIX: holding-register and coil addresses are independent,
      // zero-based spaces (see analog_name_to_address/
      // binary_name_to_address in the header) -- an analog point and a
      // binary point can legitimately land on the same numeric address.
      // ID_point alone can't distinguish "holding register 3" from
      // "coil 3", so track which space each matched entry actually
      // came from and use it below to avoid matching a request against
      // the wrong point.
      std::vector<bool> isAnalogPoint;
      for (const auto& nodePoint : nodesPoints)
        {
          bool found = false;
          for (size_t i = 0; i < analog_point_names.size (); i++)
            {
              if (analog_point_names[i].find (nodePoint) != std::string::npos)
                {
                  ID_point.push_back (i);
                  pointID.push_back (nodePoint);
                  isAnalogPoint.push_back (true);
                  found = true;
                  break;
                }
            }
          if (!found)
            {
              for (size_t i = 0; i < binary_point_names.size (); i++)
                {
                  if (binary_point_names[i].find (nodePoint) != std::string::npos)
                    {
                      ID_point.push_back (i);
                      pointID.push_back (nodePoint);
                      isAnalogPoint.push_back (false);
                      found = true;
                      break;
                    }
                }
            }
          if (!found)
            {
              NS_LOG_WARN ("ModbusApplication::handle_MIM: could not map point "
                           << nodePoint << " to any known analog/binary point");
            }
        }

      if (mitm_flag)
        {
          Json::Value configObject;
          std::map<std::string, std::string> attack;
          // BUG FIX: this was a *substring* check (configFile.find("NA")
          // == npos) meant to detect the "AttackConf" attribute's unset
          // sentinel default ("NA"). A substring search is wrong here --
          // any real path containing "NA" anywhere (e.g. this very
          // repo's own directory name, NATIG) makes the check think the
          // attribute looks like the sentinel and silently skips reading
          // the real config, leaving `attack` permanently empty so the
          // MIM attack can never fire. Found while validating the MMS
          // port of this exact pattern against a real Docker path under
          // /rd2c/PUSH/NATIG/... -- fixed here with exact equality.
          if (configFile != "NA" && !ID_point.empty ())
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

          bool attackApplied = false;
          ModbusPDU responsePdu;
          responsePdu.functionCode = requestPdu.functionCode;
          responsePdu.address = requestPdu.address;

          // Which address space this request actually targets -- needed
          // below to avoid matching a holding-register request against a
          // coil entry (or vice versa) that happens to share the same
          // numeric address.
          bool requestIsAnalog = (requestPdu.functionCode == ModbusFunctionCode::READ_HOLDING_REGISTERS
                                   || requestPdu.functionCode == ModbusFunctionCode::WRITE_SINGLE_REGISTER);
          bool requestIsBinary = (requestPdu.functionCode == ModbusFunctionCode::READ_COILS
                                   || requestPdu.functionCode == ModbusFunctionCode::WRITE_SINGLE_COIL);

          // -- Find which mapped point (if any) matches this request's --
          // -- address, then decide whether to attack it --
          for (size_t qq = 0; qq < ID_point.size (); qq++)
            {
              if (static_cast<uint16_t>(ID_point[qq]) != requestPdu.address)
                {
                  continue; // this MIM entry doesn't apply to the point being requested
                }

              // BUG FIX: address alone isn't enough -- confirm the
              // matched entry's address space (analog/holding-register
              // vs binary/coil) actually matches what this request is
              // touching. Without this, an entry mapped to e.g. coil 3
              // could match an unrelated READ_HOLDING_REGISTERS request
              // at holding-register address 3, silently applying the
              // attack to (or basing the attack decision on) the wrong
              // point. Caught during code review; keep scanning rather
              // than stop, since a later entry may still be the real
              // match for this address.
              bool pointMatchesRequestSpace = (isAnalogPoint[qq] && requestIsAnalog)
                                               || (!isAnalogPoint[qq] && requestIsBinary);
              if (!pointMatchesRequestSpace)
                {
                  continue;
                }

              float chance = (qq < attackChance.size ()) ? attackChance[qq] : 0.0f;
              float startTime = (qq < start.size ()) ? start[qq] : 0.0f;
              float stopTime = (qq < stop.size ()) ? stop[qq] : 0.0f;
              int attackTypeInt = (qq < attackType.size ()) ? static_cast<int>(attackType[qq]) : 0;

              bool inWindow = (currentTime > startTime && currentTime < stopTime);

              // attack_type 5 (replay): outside the window this is real traffic, so keep
              // refreshing the captured value -- a later window then replays a recent real
              // observation (frozen once the window opens), not whatever was first ever seen.
              if (attackTypeInt == 5 && !inWindow)
                {
                  if (isAnalogPoint[qq])
                    {
                      m_replayCaptureRegisters[requestPdu.address] = GetHoldingRegister (requestPdu.address);
                    }
                  else
                    {
                      m_replayCaptureCoils[requestPdu.address] = GetCoil (requestPdu.address);
                    }
                }

              if (inWindow && chance > r)
                {
                  NS_LOG_INFO ("ModbusApplication::handle_MIM: applying attack type "
                               << attackTypeInt << " on point " << pointID[qq]
                               << " (address " << requestPdu.address << ") at time " << currentTime << "s");

                  // -- Attack application: direct point-map mutation --
                  // (replaces DNP3's raw-byte scan + CRC recompute --
                  // see file header comment for why this differs)
                  if (attackTypeInt == 2 || attackTypeInt == 4)
                    {
                      // False data injection on a register (analog-equivalent)
                      float f = get_val (val, val_min, val_max, qq);
                      uint16_t scale = GetRegisterScale (requestPdu.address);
                      SetHoldingRegister (requestPdu.address, static_cast<uint16_t>(std::round (f / scale)));
                      float reconstructed = static_cast<float>(GetHoldingRegister (requestPdu.address)) * scale;
                      // std::cout, not NS_LOG_*: compiled out in this build's optimized
                      // profile (see feedback_ns3_build_environment_gotchas.md) -- this
                      // line is the scale-fix validation evidence, so it must actually
                      // print, not silently no-op.
                      std::cout << "ModbusApplication::handle_MIM: injected false register value "
                                << f << " at address " << requestPdu.address
                                << " (scale " << scale << ", register readback reconstructs to "
                                << reconstructed << ")" << std::endl;
                    }
                  else if (attackTypeInt == 3)
                    {
                      // Forced control command on a coil (binary-equivalent)
                      bool forcedState = (val[qq].find ("TRIP") != std::string::npos
                                          || val[qq].find ("LATCH_OFF") != std::string::npos)
                                         ? false : true;
                      SetCoil (requestPdu.address, forcedState);
                      NS_LOG_INFO ("ModbusApplication::handle_MIM: forced coil at address "
                                   << requestPdu.address << " to " << (forcedState ? "ON" : "OFF"));
                    }
                  else if (attackTypeInt == 5)
                    {
                      // Replay: reinject the frozen pre-window capture instead of the live
                      // value (or a fabricated one, like type 2/4 would).
                      // std::cout, not NS_LOG_*: compiled out in this build's optimized
                      // profile (see feedback_ns3_build_environment_gotchas.md).
                      if (isAnalogPoint[qq])
                        {
                          auto it = m_replayCaptureRegisters.find (requestPdu.address);
                          if (it != m_replayCaptureRegisters.end ())
                            {
                              SetHoldingRegister (requestPdu.address, it->second);
                              std::cout << "ModbusApplication::handle_MIM: replayed captured register value "
                                        << it->second << " at address " << requestPdu.address << std::endl;
                            }
                          else
                            {
                              std::cout << "ModbusApplication::handle_MIM: attack_type 5 fired for address "
                                        << requestPdu.address << " but no real value was captured yet" << std::endl;
                            }
                        }
                      else
                        {
                          auto it = m_replayCaptureCoils.find (requestPdu.address);
                          if (it != m_replayCaptureCoils.end ())
                            {
                              SetCoil (requestPdu.address, it->second);
                              std::cout << "ModbusApplication::handle_MIM: replayed captured coil value "
                                        << it->second << " at address " << requestPdu.address << std::endl;
                            }
                          else
                            {
                              std::cout << "ModbusApplication::handle_MIM: attack_type 5 fired for address "
                                        << requestPdu.address << " but no real value was captured yet" << std::endl;
                            }
                        }
                    }

                  attackApplied = true;

                  // Schedule reset back to the real value, mirroring
                  // DNP3's resetToRealValue delay pattern
                  if (currentTime <= stopTime && currentTime >= startTime)
                    {
                      Simulator::Schedule (Seconds (0.1), &ModbusApplicationNew::resetToRealValue,
                                            this, static_cast<int>(requestPdu.address),
                                            (qq < real_val.size ()) ? real_val[qq] : "");
                    }
                }
              break; // found the matching point; no need to keep scanning
            }

          // -- Build and send the response, whether or not an attack --
          // -- was applied (an unattacked point still needs a normal --
          // -- response, same as DNP3's handle_normal path) --
          switch (requestPdu.functionCode)
            {
              case ModbusFunctionCode::READ_HOLDING_REGISTERS:
                {
                  for (uint16_t i = 0; i < requestPdu.quantity; i++)
                    {
                      uint16_t val16 = GetHoldingRegister (requestPdu.address + i);
                      responsePdu.data.push_back (static_cast<uint8_t>((val16 >> 8) & 0xFF));
                      responsePdu.data.push_back (static_cast<uint8_t>(val16 & 0xFF));
                    }
                  break;
                }
              case ModbusFunctionCode::READ_COILS:
                {
                  uint8_t currentByte = 0;
                  int bitPos = 0;
                  for (uint16_t i = 0; i < requestPdu.quantity; i++)
                    {
                      if (GetCoil (requestPdu.address + i))
                        {
                          currentByte |= (1 << bitPos);
                        }
                      if (++bitPos == 8)
                        {
                          responsePdu.data.push_back (currentByte);
                          currentByte = 0;
                          bitPos = 0;
                        }
                    }
                  if (bitPos > 0)
                    {
                      responsePdu.data.push_back (currentByte);
                    }
                  break;
                }
              case ModbusFunctionCode::WRITE_SINGLE_REGISTER:
                responsePdu.value = attackApplied ? GetHoldingRegister (requestPdu.address) : requestPdu.value;
                break;
              case ModbusFunctionCode::WRITE_SINGLE_COIL:
                responsePdu.value = attackApplied
                  ? (GetCoil (requestPdu.address) ? 0xFF00 : 0x0000)
                  : requestPdu.value;
                break;
              default:
                break;
            }

          Ptr<Packet> responsePacket = EncodePDU (responsePdu, m_deviceConfig.unitId);
          // BUG FIX: was send_directly(responsePacket), which hardcodes
          // m_socket -- same class of bug as handle_normal's response
          // path (see send_directly_server's updated comment). Replies
          // must go out on the actual accepted-connection socket the
          // request arrived on.
          send_directly_server (socket, responsePacket);
        }

      Record (packet, from);
    }
}

// ==================== from modbus-master-poll.cc ====================
void
ModbusApplicationNew::ReadHoldingRegisters (uint16_t startAddress, uint16_t quantity)
{
  NS_LOG_FUNCTION (this << startAddress << quantity);
  ModbusPDU pdu;
  pdu.functionCode = ModbusFunctionCode::READ_HOLDING_REGISTERS;
  pdu.address = startAddress;
  pdu.quantity = quantity;

  Ptr<Packet> packet = EncodePDU (pdu, m_deviceConfig.unitId);
  send_directly (packet);
}

void
ModbusApplicationNew::ReadCoils (uint16_t startAddress, uint16_t quantity)
{
  NS_LOG_FUNCTION (this << startAddress << quantity);
  ModbusPDU pdu;
  pdu.functionCode = ModbusFunctionCode::READ_COILS;
  pdu.address = startAddress;
  pdu.quantity = quantity;

  Ptr<Packet> packet = EncodePDU (pdu, m_deviceConfig.unitId);
  send_directly (packet);
}

void
ModbusApplicationNew::WriteSingleRegister (uint16_t address, uint16_t value)
{
  NS_LOG_FUNCTION (this << address << value);
  ModbusPDU pdu;
  pdu.functionCode = ModbusFunctionCode::WRITE_SINGLE_REGISTER;
  pdu.address = address;
  pdu.value = value;

  Ptr<Packet> packet = EncodePDU (pdu, m_deviceConfig.unitId);
  send_directly (packet);
}

void
ModbusApplicationNew::WriteSingleCoil (uint16_t address, bool value)
{
  NS_LOG_FUNCTION (this << address << value);
  ModbusPDU pdu;
  pdu.functionCode = ModbusFunctionCode::WRITE_SINGLE_COIL;
  pdu.address = address;
  pdu.value = value ? 0xFF00 : 0x0000;

  Ptr<Packet> packet = EncodePDU (pdu, m_deviceConfig.unitId);
  send_directly (packet);
}

// -------------------------------------------------------------------
// periodic_poll -- the master-side scheduling loop. `count` is reused
// as the poll interval in milliseconds, matching DNP3's parameter
// naming (Simulator::Schedule(MilliSeconds(count), ...)) even though
// "count" is a slightly misleading name for an interval -- kept for
// consistency with the existing call site convention rather than
// renamed, since periodic_poll's signature is already declared in the
// header and used elsewhere.
//
// NOTE: quantity is drawn from m_deviceConfig's current point counts
// at each call, so if points are added/removed at runtime (not
// expected in this topology, but worth noting) the poll range adjusts
// automatically.
// -------------------------------------------------------------------
void
ModbusApplicationNew::periodic_poll (int count)
{
  if (running)
    {
      uint16_t analogCount = static_cast<uint16_t>(m_deviceConfig.holdingRegisters.size ());
      uint16_t coilCount = static_cast<uint16_t>(m_deviceConfig.coils.size ());

      if (analogCount > 0)
        {
          ReadHoldingRegisters (0, analogCount);
        }
      if (coilCount > 0)
        {
          ReadCoils (0, coilCount);
        }

      Simulator::Schedule (MilliSeconds (count), &ModbusApplicationNew::periodic_poll, this, count);
    }
}

// ==================== from modbus-helics.cc ====================
namespace {

// -- Small string helpers, ported verbatim from DNP3's file-local --
// -- free functions (fully generic, no protocol dependency) --

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
ModbusApplicationNew::SetEndpointName (const std::string &name, bool is_global)
{
  NS_LOG_FUNCTION (this << name << is_global);
  SetName (name);

  // Guard against running without a HELICS federate set up (e.g. a
  // socket-level-only test scenario that doesn't stand up HELICS at
  // all). Without this check, dereferencing a null/default-constructed
  // helics_federate below segfaults -- this was the actual root cause
  // of a lengthy crash investigation during the first Modbus runtime
  // test, which deliberately ran without HELICS. DNP3's original never
  // hits this because its production topology always sets up a real
  // federate before installing any application; this defensive check
  // makes that same assumption safe to violate instead of fatal.
  if (!helics_federate)
    {
      NS_LOG_WARN ("ModbusApplicationNew::SetEndpointName: helics_federate is not set; "
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
  func = std::bind (&ModbusApplicationNew::EndpointCallback, this, _1, _2);
  helics_federate->setMessageNotificationCallback (m_endpoint_id, func);
}

void
ModbusApplicationNew::EndpointCallback (helics::Endpoint id, helics::Time time)
{
  NS_LOG_FUNCTION (this << m_name << id.getName () << time);
  DoEndpoint (id, time);
}

void
ModbusApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time)
{
  NS_LOG_FUNCTION (this << id.getName () << time);
  auto message = helics_federate->getMessage (id);
  DoEndpoint (id, time, std::move (message));
}

void
ModbusApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time,
                                   std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << id.getName () << time);
  NS_LOG_INFO ("ModbusApplication::DoEndpoint");
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
                      NS_LOG_WARN ("ModbusApplication::DoEndpoint: unknown variable name " << variable);
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
                  NS_LOG_WARN ("ModbusApplication::DoEndpoint: unknown variable name " << variable);
                  Store (objId + delim + variable, value);
                }
            }
        }
    }
}

void
ModbusApplicationNew::DoMessage (std::string target_endpoint, const std::string content,
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
ModbusApplicationNew::DoRead (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string ());
  NS_LOG_INFO ("ModbusApplication::DoRead: sending message " << message->to_string ()
               << " to " << message->dest);
  helics_federate->sendMessage (m_endpoint_id, message->dest, message->data.data (), message->data.size ());
}

void
ModbusApplicationNew::attack_data (int freq)
{
  // N/A for Modbus's current scope: DNP3's version calls
  // o_p->transmitScadaData(...) -- unsolicited/spontaneous reporting,
  // where the outstation proactively pushes data without being
  // polled. None of Modbus's four scoped function codes (Read/Write
  // Holding Registers, Read/Write Coils) support unsolicited
  // responses -- Modbus is strictly request/response, master-
  // initiated. Stub kept only to satisfy the header declaration;
  // revisit if unsolicited reporting becomes relevant to the research
  // (would require a non-standard Modbus extension).
  NS_LOG_INFO ("ModbusApplication::attack_data: no-op (no unsolicited-reporting "
               "equivalent in Modbus's scoped function codes)");
}

void
ModbusApplicationNew::save_data (Ptr<Socket> socket, Ptr<Packet> packet, Address from)
{
  // Dead code in DNP3 too (defined, never called -- same situation as
  // GetStartStopArray). Stub kept only to satisfy the header
  // declaration; not a faithful port since nothing exercises it.
  NS_LOG_INFO ("ModbusApplication::save_data: no-op (unused in DNP3 source; not ported)");
}

} // namespace ns3
