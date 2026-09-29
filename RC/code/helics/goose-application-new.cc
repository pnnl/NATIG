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
 * GOOSE (IEC 61850-8-1) application for NATIG co-simulation. See
 * goose-application-new.h for the design rationale (UDP multicast
 * instead of TCP, burst-then-heartbeat retransmission instead of fixed
 * polling, rogue-publisher attack model instead of intercept-and-modify).
 *
 * KNOWN OPEN ITEMS AT TIME OF ASSEMBLY:
 *   - Not yet compiled or tested (written without local ns-3/HELICS
 *     toolchain access -- Docker/Unity validation still needed, same
 *     as every other protocol in this codebase).
 *   - No wscript entry or build script copy lines yet.
 *   - No scenario file yet exists to instantiate a GOOSE topology.
 *   - No GOOSE points file yet exists for any test topology.
 *   - Change detection is snapshot-comparison based, re-checked either
 *     on the current burst/heartbeat schedule or immediately after a
 *     HELICS-driven Store() batch (see DoEndpoint) -- not truly
 *     per-point-change event-driven the way real GOOSE publishers are,
 *     but bounded to near-real-time reaction via the DoEndpoint hook
 *     rather than waiting out a full heartbeat interval.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#include "goose-application-new.h"
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
#include "ns3/simulator.h"
#include <unistd.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("GooseApplicationNew");

NS_OBJECT_ENSURE_REGISTERED (GooseApplicationNew);

TypeId
GooseApplicationNew::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::GooseApplicationNew")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<GooseApplicationNew> ()
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket. "
                   "Kept registered but unused, mirroring Modbus/MMS's "
                   "identical attribute -- GOOSE always uses UDP explicitly "
                   "via TypeId::LookupByName in makeMulticastConnection.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&GooseApplicationNew::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&GooseApplicationNew::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddAttribute ("LocalAddress",
                   "The source Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&GooseApplicationNew::m_localAddress),
                   MakeAddressChecker ())
    .AddAttribute ("LocalPort",
                   "The source port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&GooseApplicationNew::m_localPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RemoteAddress",
                   "The GOOSE multicast group this publisher sends to / "
                   "this subscriber joins (kept the DNP3/Modbus/MMS "
                   "attribute name for shared-topology-code compatibility, "
                   "reinterpreted as the multicast group here).",
                   AddressValue (),
                   MakeAddressAccessor (&GooseApplicationNew::m_multicastGroup),
                   MakeAddressChecker ())
    .AddAttribute ("RemoteAddress2",
                   "The rogue publisher's own multicast group address, "
                   "mirrors RemoteAddress2's insider/MIM role in the other protocols.",
                   AddressValue (Ipv4Address ("10.0.0.0")),
                   MakeAddressAccessor (&GooseApplicationNew::m_multicastGroup2),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort",
                   "The destination port of the outbound multicast packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&GooseApplicationNew::m_multicastPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("isMaster",
                   "publisher or subscriber (attribute name kept identical "
                   "to DNP3/Modbus/MMS so the shared, protocol-agnostic "
                   "topology code that sets it can be reused unchanged)",
                   BooleanValue (false),
                   MakeBooleanAccessor (&GooseApplicationNew::m_isMaster),
                   MakeBooleanChecker ())
    .AddAttribute ("PointsFilename",
                   "Input Points Definitions",
                   StringValue (),
                   MakeStringAccessor (&GooseApplicationNew::points_filename),
                   MakeStringChecker ())
    .AddAttribute ("JitterMinNs",
                   "Minimum jitter delay (ns) for packet transmission",
                   DoubleValue (1000),
                   MakeDoubleAccessor (&GooseApplicationNew::m_jitterMinNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("JitterMaxNs",
                   "Maximum jitter delay (ns) for packet transmission",
                   DoubleValue (100000),
                   MakeDoubleAccessor (&GooseApplicationNew::m_jitterMaxNs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("GooseID",
                   "GOOSE control block reference identifying this "
                   "publisher's dataset",
                   StringValue (),
                   MakeStringAccessor (&GooseApplicationNew::m_gooseId),
                   MakeStringChecker ())
    .AddAttribute ("BurstCount",
                   "Number of fast retransmissions sent immediately after "
                   "a dataset change, before decaying to the heartbeat "
                   "interval (real GOOSE's T0/T1/T2 burst stages, "
                   "simplified to a single fast interval repeated N times)",
                   UintegerValue (3),
                   MakeUintegerAccessor (&GooseApplicationNew::m_burstCount),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("BurstIntervalMs",
                   "Interval, in milliseconds, between fast retransmissions "
                   "during a burst",
                   DoubleValue (4.0),
                   MakeDoubleAccessor (&GooseApplicationNew::m_burstIntervalMs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("HeartbeatIntervalMs",
                   "Steady-state retransmission interval, in milliseconds, "
                   "once a burst has finished and the dataset is unchanged",
                   DoubleValue (2000.0),
                   MakeDoubleAccessor (&GooseApplicationNew::m_heartbeatIntervalMs),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("DeadbandPct",
                   "Relative deadband (fraction of magnitude) an analog "
                   "point must move by, since the last publish, to count "
                   "as a real change -- without this, continuously-varying "
                   "real telemetry never compares exactly equal between "
                   "checks and every publisher stays in perpetual burst "
                   "mode. Binary points always use exact equality.",
                   DoubleValue (0.005),
                   MakeDoubleAccessor (&GooseApplicationNew::m_deadbandPct),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("DeadbandAbs",
                   "Absolute deadband floor for analog change detection, "
                   "used alongside DeadbandPct so near-zero values (where "
                   "a percentage alone is meaningless) still get a sane "
                   "threshold.",
                   DoubleValue (0.01),
                   MakeDoubleAccessor (&GooseApplicationNew::m_deadbandAbs),
                   MakeDoubleChecker<double> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&GooseApplicationNew::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("Rx2", "A packet has been received",
                     MakeTraceSourceAccessor (&GooseApplicationNew::m_rxTraces),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&GooseApplicationNew::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
    .AddAttribute ("AttackSelection", "Select the type of attack. Disconnect or send 0 payload",
                   UintegerValue (0),
                   MakeUintegerAccessor (&GooseApplicationNew::m_attackType),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Value_attck", "Select a value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&GooseApplicationNew::m_attack_point_val),
                   MakeStringChecker ())
    .AddAttribute ("Value_attck_max", "Select the max value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&GooseApplicationNew::m_attack_max),
                   MakeStringChecker ())
    .AddAttribute ("Value_attck_min", "Select the min value to set the point that is being manipulated",
                   StringValue ("NA"),
                   MakeStringAccessor (&GooseApplicationNew::m_attack_min),
                   MakeStringChecker ())
    .AddAttribute ("PointID", "The ID of the point that is being modified for nodeX ex:Pref, Qref",
                   StringValue (),
                   MakeStringAccessor (&GooseApplicationNew::point_id),
                   MakeStringChecker ())
    .AddAttribute ("NodeID", "The ID of the node that has a point being modified, note before the $",
                   StringValue (),
                   MakeStringAccessor (&GooseApplicationNew::node_id),
                   MakeStringChecker ())
    .AddAttribute ("RealVal", "The value that the victim should be set back to after the attack ends",
                   StringValue ("NA"),
                   MakeStringAccessor (&GooseApplicationNew::RealVal),
                   MakeStringChecker ())
    .AddAttribute ("AttackConf", "The config file that contains the attack parameters",
                   StringValue ("NA"),
                   MakeStringAccessor (&GooseApplicationNew::configFile),
                   MakeStringChecker ())
    .AddAttribute ("AttackStartTime", "Attack start time in seconds",
                   StringValue ("0"),
                   MakeStringAccessor (&GooseApplicationNew::m_attackStartTime),
                   MakeStringChecker ())
    .AddAttribute ("AttackEndTime", "Attack end time in seconds",
                   StringValue ("0"),
                   MakeStringAccessor (&GooseApplicationNew::m_attackEndTime),
                   MakeStringChecker ())
    .AddAttribute ("AttackChance", "Attack chance in percentage (0 to 1)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&GooseApplicationNew::m_attackChance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Name",
                   "The name of the application",
                   StringValue (),
                   MakeStringAccessor (&GooseApplicationNew::m_name),
                   MakeStringChecker ())
    .AddAttribute ("ID", "Int representing the ID of the rogue-publisher attacker",
                   UintegerValue (0),
                   MakeUintegerAccessor (&GooseApplicationNew::MIM_ID),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("OutFileName",
                   "The name of the output file",
                   StringValue (),
                   MakeStringAccessor (&GooseApplicationNew::f_name),
                   MakeStringChecker ())
    .AddAttribute ("mitmFlag", "Man in the middle / rogue-publisher flag",
                   BooleanValue (false),
                   MakeBooleanAccessor (&GooseApplicationNew::mitm_flag),
                   MakeBooleanChecker ())
    .AddAttribute ("FdiFlag", "Compromised-endpoint false-data-injection flag: the real publisher fabricates its own readings, no rogue instance involved",
                   BooleanValue (false),
                   MakeBooleanAccessor (&GooseApplicationNew::fdi_flag),
                   MakeBooleanChecker ())
    .AddAttribute ("FdiID", "Int representing the ID of the FDI attacker, indexes into the config's FDI array",
                   UintegerValue (0),
                   MakeUintegerAccessor (&GooseApplicationNew::FDI_ID),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

GooseApplicationNew::GooseApplicationNew ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_rand_delay_ns = CreateObject<UniformRandomVariable> ();
  m_rand_delay_ns->SetAttribute ("Min", DoubleValue (m_jitterMinNs));
  m_rand_delay_ns->SetAttribute ("Max", DoubleValue (m_jitterMaxNs));
}

GooseApplicationNew::~GooseApplicationNew ()
{
  NS_LOG_FUNCTION (this);
}

uint32_t
GooseApplicationNew::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
GooseApplicationNew::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
GooseApplicationNew::SetName (const std::string &name)
{
  m_name = name;
}

std::string
GooseApplicationNew::GetName (void) const
{
  return m_name;
}

// -------------------------------------------------------------------
// CSVRow -- ported verbatim from mms-application-new.cc.
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
GooseApplicationNew::readMicroGridConfig (std::string fpath, Json::Value& configobj)
{
  std::ifstream tifs (fpath);
  Json::Reader configreader;
  configreader.parse (tifs, configobj);
}

void
GooseApplicationNew::GetStartStopArray ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("GooseApplication::GetStartStopArray: no-op (unused in DNP3/Modbus/MMS source; not ported)");
}

std::vector<std::string>
GooseApplicationNew::get_val_vector (std::string delimiter, std::string m_attack_val)
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
GooseApplicationNew::get_val (std::vector<std::string> val, std::vector<std::string> val_min,
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
          NS_LOG_INFO ("GooseApplication::get_val: random selector " << r);
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

std::vector<float>
GooseApplicationNew::GetVal (std::map<std::string, std::string> attack, std::string tag)
{
  std::vector<float> timer;
  std::string delimiter = ",";
  size_t pos = 0;
  std::string token;
  std::string key = "MIM-" + std::to_string (MIM_ID) + "-" + tag;

  auto it = attack.find (key);
  if (it == attack.end () || it->second.empty ())
    {
      NS_LOG_WARN ("GooseApplication::GetVal: no value for key '" << key
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
// Point-map accessors -- m_deviceConfig is name-keyed directly, same
// design as MMS (see mms-application-new.cc's identical accessors and
// the file header note on why no address-translation layer is needed).
// -------------------------------------------------------------------
void
GooseApplicationNew::SetAnalogPoint (const std::string &pointName, float value)
{
  m_deviceConfig.analogValues[pointName] = value;
}

float
GooseApplicationNew::GetAnalogPoint (const std::string &pointName) const
{
  auto it = m_deviceConfig.analogValues.find (pointName);
  return (it != m_deviceConfig.analogValues.end ()) ? it->second : 0.0f;
}

void
GooseApplicationNew::SetBinaryPoint (const std::string &pointName, bool value)
{
  m_deviceConfig.binaryValues[pointName] = value;
}

bool
GooseApplicationNew::GetBinaryPoint (const std::string &pointName) const
{
  auto it = m_deviceConfig.binaryValues.find (pointName);
  return (it != m_deviceConfig.binaryValues.end ()) ? it->second : false;
}

// Compromised-endpoint FDI: same shape as DNP3/Modbus/MMS's apply_fdi --
// resolves node_id/point_id/Value_attck/AttackChance attributes (set once at
// topology build time, no runtime JSON re-read) and returns a fabricated
// value when they match and the chance roll fires. m_attack_on (window) is
// checked by the caller. Runs on the real publisher (fdi_flag), not the
// rogue-publisher role (mitm_flag) -- a fabricated value here flows through
// the normal publish path, so it naturally trips
// datasetChangedSinceLastPublish()'s deadband/burst logic exactly like a
// real physical change would, rather than needing its own stNum-forging
// logic the way handle_rogue_publish does.

// safeStof/safeStod -- FDI config values (Value_attck, AttackStartTime,
// AttackEndTime) come from grid.json/AttackConf and can be malformed or
// empty; std::stof/std::stod throw std::invalid_argument/std::out_of_range
// uncaught on that, crashing the whole simulation. This is the same crash
// class the MIM GetVal guard closes -- see PR review from Oceane Bel (PNNL)
// on the FDI PR. Duplicated per protocol file, matching this file's existing
// convention (e.g. get_val_vector) of no shared helics helper header.
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

float
GooseApplicationNew::apply_fdi (const std::string& name, float realValue)
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

      float r = static_cast<float>(rand ()) / static_cast<float>(RAND_MAX);
      if (m_attackChance <= r)
        {
          return realValue;
        }

      float fabricated = safeStof (vals[i], realValue, "apply_fdi Value_attck");
      std::cout << "FDI: publisher " << m_name << " fabricating point " << name << " -> " << fabricated
                 << " (real value " << realValue << ") at time " << Simulator::Now ().GetSeconds () << "s" << std::endl;
      return fabricated;
    }

  return realValue;
}

// -------------------------------------------------------------------
// store_points -- called via HELICS's Store() (see DoEndpoint below).
// Unlike periodic-poll protocols, a real value change here should
// provoke a near-immediate publish rather than wait for the next
// scheduled tick -- DoEndpoint (the caller's caller) handles that by
// cancelling and re-triggering the publish schedule once per HELICS
// message batch, after all of a message's Store() calls complete.
// -------------------------------------------------------------------
void
GooseApplicationNew::store_points (std::string name, std::string value)
{
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

  NS_LOG_INFO ("GooseApplication::store_points: point not found: " << name);
}

void
GooseApplicationNew::Store (std::string point, std::string value)
{
  if (m_isMaster) // publisher role -- see file header note on isMaster's meaning here
    {
      store_points (point, value);
    }
}

void
GooseApplicationNew::set_attack (bool state)
{
  NS_LOG_INFO ("GooseApplication::set_attack >>> Start Attack Mode: " << m_attackType);
  m_attack_on = state;

  // Option B fix (Sep 2026): apply_fdi was only ever wired into
  // store_points(), reached exclusively via the HELICS DoEndpoint path --
  // which never fires in this codebase (see natig-v2 research notes).
  // Apply FDI directly to the compromised publisher's own dataset at the
  // moment the attack window opens, so a fabricated value actually
  // reaches a transmitted PDU. Guarded on fdi_flag specifically (not
  // just any set_attack toggle) since this function is shared with the
  // separate rogue/MIM attack mechanism (see StartApplication's isRogue
  // scheduling above), which must not have its analogValues touched.
  // Restoring on attack-end matters because nothing else ever refreshes
  // these values absent a real Store() -- without it the fabricated
  // value would persist past the window.
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
          m_deviceConfig.analogValues = m_preAttackAnalogValues.analogValues;
        }
    }
}

void
GooseApplicationNew::set_respond (bool respond)
{
  NS_LOG_INFO ("GooseApplication::set_respond");
  m_respond = respond;
}

void
GooseApplicationNew::set_offline (bool offline)
{
  NS_LOG_INFO ("GooseApplication::set_offline");
  m_offline = offline;
}

void
GooseApplicationNew::SetLocal (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void
GooseApplicationNew::SetLocal (Ipv4Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = ip;
  m_localPort = port;
}

void
GooseApplicationNew::SetLocal (Ipv6Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_localAddress = Address (ip);
  m_localPort = port;
}

void
GooseApplicationNew::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  Simulator::Cancel (m_publishEvent);
  Application::DoDispose ();
}

void
GooseApplicationNew::initConfig (void)
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

  m_preAttackAnalogValues = m_deviceConfig;

  NS_LOG_INFO ("GooseApplication::initConfig: loaded " << analog_point_names.size ()
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

void WriteRef (std::vector<uint8_t>& buf, const std::string& ref)
{
  WriteU16BE (buf, static_cast<uint16_t>(ref.size ()));
  buf.insert (buf.end (), ref.begin (), ref.end ());
}

} // anonymous namespace

// -------------------------------------------------------------------
// DecodePDU / EncodePDU
//
// Single message shape, no service code, no request/response --
// [length(4)] [goIDLen(2)+goID] [stNum(4)] [sqNum(4)]
// [numAnalog(2)][refLen(2)+ref+value(4)]*
// [numBinary(2)][refLen(2)+ref+value(1)]*
// -------------------------------------------------------------------
GoosePDU
GooseApplicationNew::DecodePDU (Ptr<Packet> packet)
{
  GoosePDU pdu;
  uint32_t size = packet->GetSize ();

  if (size < 4)
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

  const uint8_t* data = &buf[4];
  uint32_t dataLen = size - 4;
  uint32_t offset = 0;

  if (dataLen < 2)
    {
      pdu.isMalformed = true;
      return pdu;
    }
  uint16_t goIdLen = ReadU16BE (&data[offset]);
  offset += 2;
  if (dataLen < offset + goIdLen + 8)
    {
      NS_LOG_WARN ("DecodePDU: truncated before stNum/sqNum, discarding");
      pdu.isMalformed = true;
      return pdu;
    }
  pdu.goID.assign (reinterpret_cast<const char*>(&data[offset]), goIdLen);
  offset += goIdLen;

  pdu.stNum = ReadU32BE (&data[offset]);
  offset += 4;
  pdu.sqNum = ReadU32BE (&data[offset]);
  offset += 4;

  if (dataLen < offset + 2)
    {
      pdu.isMalformed = true;
      return pdu;
    }
  uint16_t numAnalog = ReadU16BE (&data[offset]);
  offset += 2;
  for (uint16_t i = 0; i < numAnalog; i++)
    {
      if (dataLen < offset + 2)
        {
          pdu.isMalformed = true;
          return pdu;
        }
      uint16_t refLen = ReadU16BE (&data[offset]);
      offset += 2;
      if (dataLen < offset + refLen + 4)
        {
          pdu.isMalformed = true;
          return pdu;
        }
      std::string ref (reinterpret_cast<const char*>(&data[offset]), refLen);
      offset += refLen;
      float val = ReadFloatBE (&data[offset]);
      offset += 4;
      pdu.analogValues.emplace_back (ref, val);
    }

  if (dataLen < offset + 2)
    {
      pdu.isMalformed = true;
      return pdu;
    }
  uint16_t numBinary = ReadU16BE (&data[offset]);
  offset += 2;
  for (uint16_t i = 0; i < numBinary; i++)
    {
      if (dataLen < offset + 2)
        {
          pdu.isMalformed = true;
          return pdu;
        }
      uint16_t refLen = ReadU16BE (&data[offset]);
      offset += 2;
      if (dataLen < offset + refLen + 1)
        {
          pdu.isMalformed = true;
          return pdu;
        }
      std::string ref (reinterpret_cast<const char*>(&data[offset]), refLen);
      offset += refLen;
      bool val = (data[offset] != 0);
      offset += 1;
      pdu.binaryValues.emplace_back (ref, val);
    }

  return pdu;
}

Ptr<Packet>
GooseApplicationNew::EncodePDU (const GoosePDU& pdu)
{
  std::vector<uint8_t> body;

  WriteRef (body, pdu.goID);
  WriteU32BE (body, pdu.stNum);
  WriteU32BE (body, pdu.sqNum);

  WriteU16BE (body, static_cast<uint16_t>(pdu.analogValues.size ()));
  for (const auto& entry : pdu.analogValues)
    {
      WriteRef (body, entry.first);
      WriteFloatBE (body, entry.second);
    }
  WriteU16BE (body, static_cast<uint16_t>(pdu.binaryValues.size ()));
  for (const auto& entry : pdu.binaryValues)
    {
      WriteRef (body, entry.first);
      body.push_back (entry.second ? 1 : 0);
    }

  std::vector<uint8_t> frame;
  WriteU32BE (frame, static_cast<uint32_t>(body.size ()));
  frame.insert (frame.end (), body.begin (), body.end ());
  return Create<Packet> (frame.data (), frame.size ());
}

// ==================== lifecycle ====================
void
GooseApplicationNew::StartApplication ()
{
  NS_LOG_FUNCTION (this);
  running = true;
  m_attack_on = false;
  if (fdi_flag) {
    // Unlike handle_rogue_publish (reactive to attack_data's schedule), FDI has
    // a fixed window on the real publisher itself, so it can be scheduled once
    // here instead. See DNP3/Modbus/MMS's identical addition.
    double attackStart = safeStod (m_attackStartTime, -1.0, "FDI AttackStartTime");
    double attackEnd = safeStod (m_attackEndTime, -1.0, "FDI AttackEndTime");
    if (attackStart < 0.0 || attackEnd < 0.0)
      {
        std::cerr << "[WARN] " << "GooseApplicationNew::StartApplication: bad FDI AttackStartTime/"
                     "AttackEndTime config value for node " << m_name
                     << " -- FDI disabled for this instance." << std::endl;
      }
    else
      {
        Simulator::Schedule(Seconds(attackStart), &GooseApplicationNew::set_attack, this, true);
        Simulator::Schedule(Seconds(attackEnd), &GooseApplicationNew::set_attack, this, false);
      }
  }
  makeMulticastConnection ();
}

void
GooseApplicationNew::makeMulticastConnection (void)
{
  NS_LOG_FUNCTION (this);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tid);

  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_multicastPort);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&GooseApplicationNew::HandleRead, this));

  // -- Attack start/end time scheduling, generic string parsing, no --
  // -- protocol-specific dependency (ported near-verbatim from MMS/Modbus) --
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

  bool isRogue = (m_name.find ("Inside") != std::string::npos
                   || m_name.find ("MIM") != std::string::npos);

  for (size_t index = 0; index < timer.size (); index++)
    {
      if (isRogue)
        {
          NS_LOG_UNCOND ("GooseApplication: I'm the rogue/insider publisher");
          if (timer[index])
            {
              Simulator::Schedule (Seconds (timer[index]), &GooseApplicationNew::set_attack, this, true);
              StartVect.push_back (std::to_string (timer[index]));
            }
          if (timer_end[index])
            {
              Simulator::Schedule (Seconds (timer_end[index]), &GooseApplicationNew::set_attack, this, false);
              StopVect.push_back (std::to_string (timer_end[index]));
            }
        }
    }

  if (m_isMaster || isRogue)
    {
      // Publisher (or rogue publisher): joins/sends to the multicast
      // group by connecting the UDP socket to it -- Connect() on a UDP
      // socket just sets a default destination, a real send target for
      // Send() without needing SendTo() each time.
      Address group = isRogue ? m_multicastGroup2 : m_multicastGroup;
      m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom (group), m_multicastPort));
      startPublisher ();
    }
  else
    {
      // Subscriber: no explicit "join" call needed -- ns-3's Socket
      // API in this version has no generic IPv4 multicast join method
      // (only Ipv6JoinGroup exists; see src/csma/examples/csma-multicast.cc,
      // where the receiving PacketSinkHelper just Binds to
      // (Ipv4Address::GetAny(), port), same as `local` above, and
      // receives multicast traffic delivered on a shared segment
      // without any per-socket join). The multicast route this
      // actually depends on (so the *sender's* outbound multicast
      // packets get routed onto the shared segment at all) is topology
      // -level setup via Ipv4StaticRoutingHelper::SetDefaultMulticastRoute
      // on the publisher's interface, not something this application
      // class configures itself -- see the topology/scenario file.
      startSubscriber ();
    }
}

void
GooseApplicationNew::StopApplication ()
{
  NS_LOG_FUNCTION (this);
  running = false;
  Simulator::Cancel (m_publishEvent);
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
GooseApplicationNew::startPublisher ()
{
  NS_LOG_FUNCTION (this);
  debugLevel = 0;
  initConfig ();
}

void
GooseApplicationNew::startSubscriber ()
{
  NS_LOG_FUNCTION (this);
  initConfig ();
  m_respond = true;
  m_offline = false;
}

// ==================== read/send plumbing ====================
void
GooseApplicationNew::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address from;
  Ptr<Packet> packet;

  while ((packet = socket->RecvFrom (from)))
    {
      m_txTrace (packet);
      if (!m_respond)
        {
          Record (packet, from);
          continue;
        }

      GoosePDU pdu = DecodePDU (packet);
      if (pdu.isMalformed)
        {
          Record (packet, from);
          continue;
        }

      // A subscriber accepts a frame as the latest state if its
      // stNum is newer, or the same stNum with a newer sqNum (a
      // heartbeat repeat) -- this is also exactly the acceptance
      // logic a rogue publisher's forged frame exploits (see
      // handle_rogue_publish): nothing here distinguishes the
      // legitimate publisher's multicast group membership from an
      // unauthorized one sending to the same group, since GOOSE has
      // no authentication at the wire level in this implementation
      // (matching real, unsecured GOOSE deployments -- the actual
      // point of this attack surface for the detectability comparison).
      //
      // CONFIRMED BEHAVIOR (deliberate, not a bug): once a rogue
      // publisher forges a higher stNum than the legitimate publisher
      // will ever reach again, subscribers stay poisoned indefinitely
      // after the attack window closes -- the legitimate publisher's
      // own heartbeats (still at its real, lower stNum) can never
      // outrank the forged one. Verified via live tracing during
      // development: a subscriber stayed on an attacker's injected
      // value for the rest of a run after a 10s attack window ended.
      // This matches a real, documented IEC 61850-8-1 GOOSE
      // vulnerability (stNum-based poisoning persistence), not
      // something to silently patch around. Real GOOSE gives
      // publishers a narrow recovery path via stNum==1 being reserved
      // as an explicit "publisher reset" signal subscribers must
      // accept regardless of the previous highest stNum seen -- not
      // implemented here; deliberately deferred so the attack's full,
      // realistic persistence is what the detectability comparison
      // actually measures.
      if (pdu.stNum > m_stNum || (pdu.stNum == m_stNum && pdu.sqNum > m_sqNum))
        {
          m_stNum = pdu.stNum;
          m_sqNum = pdu.sqNum;
          for (const auto& entry : pdu.analogValues)
            {
              SetAnalogPoint (entry.first, entry.second);
            }
          for (const auto& entry : pdu.binaryValues)
            {
              SetBinaryPoint (entry.first, entry.second);
            }
          NS_LOG_INFO ("GooseApplication (subscriber): accepted goID=" << pdu.goID
                       << " stNum=" << pdu.stNum << " sqNum=" << pdu.sqNum);
        }
      else
        {
          NS_LOG_LOGIC ("GooseApplication (subscriber): stale/duplicate frame discarded, "
                        "stNum=" << pdu.stNum << " sqNum=" << pdu.sqNum);
        }

      Record (packet, from);
    }
}

void
GooseApplicationNew::send_directly (Ptr<Packet> p)
{
  m_txTrace (p);
  int delay_ns = (int) (m_rand_delay_ns->GetValue (m_jitterMinNs, m_jitterMaxNs) + 0.5);

  int (Socket::*fp)(Ptr<Packet>, uint32_t) = &Socket::Send;
  Simulator::Schedule (NanoSeconds (delay_ns), fp, m_socket, p, 0);
}

void
GooseApplicationNew::Record (Ptr<Packet> packet, Address from)
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
                  << " : " << m_multicastGroup
                  << " : " << packet->GetUid () << "\n";
          outfile.close ();
        }
    }
}

// -------------------------------------------------------------------
// PublishNow / schedulePublish / publishAndReschedule
//
// The burst-then-heartbeat retransmission core. publishAndReschedule()
// is the single recurring tick: it publishes the current dataset,
// decides whether that was a real change (bumping stNum, resetting
// sqNum, and arming a fresh burst) or a repeat (just bumping sqNum),
// then reschedules itself at the burst or heartbeat interval as
// appropriate.
// -------------------------------------------------------------------
void
GooseApplicationNew::PublishNow (void)
{
  Simulator::Cancel (m_publishEvent);
  publishAndReschedule ();
}

bool
GooseApplicationNew::datasetChangedSinceLastPublish (void)
{
  // Binary points: exact equality -- a breaker position change is a
  // real discrete event, no deadband needed, and the point sets
  // differing in membership at all (added/removed points) is a real
  // structural change on its own.
  if (m_deviceConfig.binaryValues.size () != m_lastPublishedConfig.binaryValues.size ())
    {
      return true;
    }
  for (const auto& entry : m_deviceConfig.binaryValues)
    {
      auto it = m_lastPublishedConfig.binaryValues.find (entry.first);
      if (it == m_lastPublishedConfig.binaryValues.end () || it->second != entry.second)
        {
          return true;
        }
    }

  // Analog points: deadband comparison -- see the DeadbandPct/DeadbandAbs
  // attribute docs and the class header note on why exact equality
  // against continuously-varying real telemetry causes perpetual burst
  // mode.
  if (m_deviceConfig.analogValues.size () != m_lastPublishedConfig.analogValues.size ())
    {
      return true;
    }
  for (const auto& entry : m_deviceConfig.analogValues)
    {
      auto it = m_lastPublishedConfig.analogValues.find (entry.first);
      if (it == m_lastPublishedConfig.analogValues.end ())
        {
          return true;
        }
      float delta = std::fabs (entry.second - it->second);
      float threshold = std::max (static_cast<float>(m_deadbandAbs),
                                    static_cast<float>(m_deadbandPct) * std::fabs (it->second));
      if (delta > threshold)
        {
          return true;
        }
    }

  return false;
}

void
GooseApplicationNew::publishAndReschedule (void)
{
  if (!running)
    {
      return;
    }

  bool changed = datasetChangedSinceLastPublish ();
  if (changed)
    {
      m_stNum++;
      m_sqNum = 0;
      m_burstRemaining = m_burstCount;
      m_lastPublishedConfig = m_deviceConfig;
    }
  else
    {
      m_sqNum++;
    }

  GoosePDU pdu;
  pdu.goID = m_gooseId;
  pdu.stNum = m_stNum;
  pdu.sqNum = m_sqNum;
  for (const auto& entry : m_deviceConfig.analogValues)
    {
      pdu.analogValues.emplace_back (entry.first, entry.second);
    }
  for (const auto& entry : m_deviceConfig.binaryValues)
    {
      pdu.binaryValues.emplace_back (entry.first, entry.second);
    }

  Ptr<Packet> packet = EncodePDU (pdu);
  send_directly (packet);

  double nextIntervalMs = m_heartbeatIntervalMs;
  if (m_burstRemaining > 0)
    {
      nextIntervalMs = m_burstIntervalMs;
      m_burstRemaining--;
    }

  m_publishEvent = Simulator::Schedule (MilliSeconds (static_cast<uint64_t>(nextIntervalMs)),
                                          &GooseApplicationNew::publishAndReschedule, this);
}

void
GooseApplicationNew::schedulePublish (void)
{
  // Kicks off the very first publish immediately (a real GOOSE
  // publisher always sends its current state as soon as it starts,
  // rather than waiting for the first heartbeat interval to elapse).
  m_publishEvent = Simulator::Schedule (MilliSeconds (0), &GooseApplicationNew::publishAndReschedule, this);
}

// ==================== rogue-publisher attack ====================
// -------------------------------------------------------------------
// handle_rogue_publish -- the GOOSE-appropriate replacement for
// handle_MIM. There is no in-path position to intercept in a
// multicast topology (unlike Modbus/MMS's TCP client-server
// connections, which a MIM socket can sit between); instead, an
// unauthorized "Insider" node publishes its own competing GOOSE frame
// to the same multicast group with a forged/advanced stNum, exploiting
// the acceptance rule in HandleRead (newer stNum always wins) to have
// subscribers treat the forged state as authoritative -- this is
// exactly how real, unsecured GOOSE deployments get spoofed.
// -------------------------------------------------------------------
void
GooseApplicationNew::handle_rogue_publish (void)
{
  if (!mitm_flag || !m_attack_on)
    {
      return;
    }

  Json::Value configObject;
  std::map<std::string, std::string> attack;
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

  std::string delimiter = ",";
  std::vector<std::string> val = get_val_vector (delimiter, m_attack_point_val);
  std::vector<std::string> val_min = get_val_vector (delimiter, m_attack_min);
  std::vector<std::string> val_max = get_val_vector (delimiter, m_attack_max);
  std::vector<std::string> nodes = get_val_vector (delimiter, node_id);
  std::vector<std::string> points = get_val_vector (delimiter, point_id);

  std::vector<float> attackType = GetVal (attack, "attack_type");

  GoosePDU pdu;
  pdu.goID = m_gooseId;
  // Forge a state strictly newer than anything a real subscriber has
  // seen so far -- advancing our own stNum/sqNum tracking too, so
  // repeated attack ticks keep out-forging the real publisher's
  // heartbeats rather than being immediately superseded by them.
  m_stNum++;
  m_sqNum = 0;
  pdu.stNum = m_stNum;
  pdu.sqNum = m_sqNum;

  for (size_t xx = 0; xx < nodes.size () && xx < points.size (); xx++)
    {
      std::string nodePoint = nodes[xx] + "$" + points[xx];
      int attackTypeInt = (xx < attackType.size ()) ? static_cast<int>(attackType[xx]) : 0;

      bool isAnalog = false;
      for (const auto& n : analog_point_names)
        {
          if (n.find (nodePoint) != std::string::npos) { isAnalog = true; break; }
        }
      bool isBinary = false;
      if (!isAnalog)
        {
          for (const auto& n : binary_point_names)
            {
              if (n.find (nodePoint) != std::string::npos) { isBinary = true; break; }
            }
        }

      // Same attack-type numbering as DNP3/Modbus/MMS (2/4 = false
      // data injection on an analog point, 3 = forced control command
      // on a binary point), so cross-protocol attack comparisons stay
      // valid -- applied here by forging the published dataset value
      // rather than mutating an intercepted response.
      if ((attackTypeInt == 2 || attackTypeInt == 4) && isAnalog)
        {
          float f = get_val (val, val_min, val_max, xx);
          pdu.analogValues.emplace_back (nodePoint, f);
        }
      else if (attackTypeInt == 3 && isBinary)
        {
          bool forcedState = !(val[xx].find ("TRIP") != std::string::npos
                                || val[xx].find ("LATCH_OFF") != std::string::npos);
          pdu.binaryValues.emplace_back (nodePoint, forcedState);
        }
    }

  if (pdu.analogValues.empty () && pdu.binaryValues.empty ())
    {
      return; // nothing matched a configured attack target this tick
    }

  Ptr<Packet> packet = EncodePDU (pdu);
  send_directly (packet);
  NS_LOG_INFO ("GooseApplication::handle_rogue_publish: forged stNum=" << pdu.stNum
               << " for goID=" << pdu.goID << " at time " << Simulator::Now ().GetSeconds () << "s");
}

// ==================== HELICS integration ====================
namespace {

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
GooseApplicationNew::SetEndpointName (const std::string &name, bool is_global)
{
  NS_LOG_FUNCTION (this << name << is_global);
  SetName (name);

  if (!helics_federate)
    {
      NS_LOG_WARN ("GooseApplicationNew::SetEndpointName: helics_federate is not set; "
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
  func = std::bind (&GooseApplicationNew::EndpointCallback, this, _1, _2);
  helics_federate->setMessageNotificationCallback (m_endpoint_id, func);
}

void
GooseApplicationNew::EndpointCallback (helics::Endpoint id, helics::Time time)
{
  NS_LOG_FUNCTION (this << m_name << id.getName () << time);
  DoEndpoint (id, time);
}

void
GooseApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time)
{
  NS_LOG_FUNCTION (this << id.getName () << time);
  auto message = helics_federate->getMessage (id);
  DoEndpoint (id, time, std::move (message));
}

void
GooseApplicationNew::DoEndpoint (helics::Endpoint id, helics::Time time,
                                   std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << id.getName () << time);
  NS_LOG_INFO ("GooseApplication::DoEndpoint");
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
                      NS_LOG_WARN ("GooseApplication::DoEndpoint: unknown variable name " << variable);
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
                  NS_LOG_WARN ("GooseApplication::DoEndpoint: unknown variable name " << variable);
                  Store (objId + delim + variable, value);
                }
            }
        }
    }

  // A batch of HELICS-driven Store() calls just completed -- if we're
  // the publisher and anything actually changed, react immediately
  // instead of waiting out the current heartbeat/burst interval (see
  // file header note on this bounded-latency change-detection design).
  if (m_isMaster && running)
    {
      PublishNow ();
    }

  // Rogue-publisher attack tick: forge a competing frame targeting
  // whatever this instance's attack config points at, if active.
  if (mitm_flag && m_attack_on)
    {
      handle_rogue_publish ();
    }
}

void
GooseApplicationNew::DoMessage (std::string target_endpoint, const std::string content,
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
GooseApplicationNew::DoRead (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string ());
  NS_LOG_INFO ("GooseApplication::DoRead: sending message " << message->to_string ()
               << " to " << message->dest);
  helics_federate->sendMessage (m_endpoint_id, message->dest, message->data.data (), message->data.size ());
}

void
GooseApplicationNew::scheduleRoguePublish (int freq)
{
  if (!running)
    {
      return;
    }
  handle_rogue_publish ();
  Simulator::Schedule (MilliSeconds (freq), &GooseApplicationNew::scheduleRoguePublish, this, freq);
}

void
GooseApplicationNew::attack_data (int freq)
{
  // Kicks off periodic scheduling, named attack_data only for
  // structural parity with DNP3/Modbus/MMS's scheduling entry points
  // (their production topology files call this externally to start
  // periodic_poll/attack_data).
  //
  // For the legitimate publisher role, this starts the real
  // burst/heartbeat schedule (see schedulePublish) -- `freq` is
  // unused there, since that cadence comes from the BurstCount/
  // BurstIntervalMs/HeartbeatIntervalMs attributes, not a single fixed
  // interval.
  //
  // For a rogue/insider instance (mitm_flag set), handle_rogue_publish
  // is only ever invoked from a live HELICS message batch (see
  // DoEndpoint) in the production topology -- a standalone test with
  // no HELICS federate at all would otherwise never trigger the
  // attack, so this gives the rogue role its own periodic schedule
  // instead, reusing `freq` (ms) as that interval directly.
  if (mitm_flag)
    {
      scheduleRoguePublish (freq);
    }
  else
    {
      (void) freq;
      schedulePublish ();
    }
}

} // namespace ns3
