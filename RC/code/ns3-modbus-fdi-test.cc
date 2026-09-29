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
 * Minimal Modbus compromised-endpoint FDI (false data injection) test.
 *
 * Based on ns3-modbus-mim-test.cc, but exercises set_attack()/apply_fdi
 * (commit 4856b53) instead of handle_MIM: the outstation's name does NOT
 * contain "MIM", so HandleRead dispatches to the normal handle_normal path
 * -- this attack fabricates the outstation's own readings directly (no
 * MITM position needed), gated by FdiFlag rather than mitmFlag/AttackConf.
 *
 * Expected result: TestRegister1's real value is 100. Between
 * AttackStartTime=10s and AttackEndTime=20s, set_attack(true)/apply_fdi
 * fabricates it to 132000 (matching a real configured FDI attack magnitude,
 * and exceeding a raw 16-bit register's 65535 range -- exercising the
 * analog register scale fix, m_registerScale/GetRegisterScale in
 * modbus-application-new.cc). Every register-1 poll in that window should
 * return 132000; before/after, it should return 100.
 *
 * Validates two things directly from console output:
 *   1. The "FDI: outstation ... fabricating point ..." line from apply_fdi
 *      itself fires (confirms 4856b53's fix -- FDI actually reaches a real
 *      register write, not just store_points()'s dead HELICS path).
 *   2. The "FDI register readback ... reconstructs to 132000" line from
 *      set_attack (added alongside the register-scale fix) confirms
 *      GetHoldingRegister() x scale reproduces the fabricated value exactly
 *      -- directly answering the review request to re-validate Modbus FDI
 *      by reading back the register.
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/modbus-application-helper-new.h"
#include "ns3/modbus-application-new.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ModbusFdiTest");

int
main (int argc, char *argv[])
{
  double simTimeSeconds = 30.0;
  int pollIntervalMs = 2000;
  std::string pointsFile = "points_modbus_mim_test.csv";

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation duration in seconds", simTimeSeconds);
  cmd.AddValue ("pollInterval", "Master poll interval in milliseconds", pollIntervalMs);
  cmd.AddValue ("pointsFile", "Path to the Modbus points CSV file", pointsFile);
  cmd.Parse (argc, argv);

  LogComponentEnable ("ModbusFdiTest", LOG_LEVEL_INFO);
  LogComponentEnable ("ModbusApplicationNew", LOG_LEVEL_ALL);

  NS_LOG_INFO ("Modbus FDI test: simTime=" << simTimeSeconds
               << "s, pollInterval=" << pollIntervalMs << "ms, pointsFile=" << pointsFile);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  Ipv4Address masterAddress = interfaces.GetAddress (0);
  Ipv4Address outstationAddress = interfaces.GetAddress (1);

  uint16_t masterPort = 5020;
  uint16_t outstationPort = 5020;

  // -- Outstation (server): no "MIM" in the name, so handle_normal --
  // -- handles requests -- FDI fabricates its own registers directly. --
  ModbusApplicationHelperNew modbusOutstation ("ns3::TcpSocketFactory",
                                                InetSocketAddress (outstationAddress, outstationPort));
  modbusOutstation.SetAttribute ("LocalPort", UintegerValue (outstationPort));
  modbusOutstation.SetAttribute ("RemoteAddress", AddressValue (masterAddress));
  modbusOutstation.SetAttribute ("RemotePort", UintegerValue (masterPort));
  modbusOutstation.SetAttribute ("isMaster", BooleanValue (false));
  modbusOutstation.SetAttribute ("Name", StringValue ("ModbusOutstationFDI1"));
  modbusOutstation.SetAttribute ("PointsFilename", StringValue (pointsFile));
  modbusOutstation.SetAttribute ("UnitId", UintegerValue (1));
  modbusOutstation.SetAttribute ("EnableTCP", BooleanValue (true));
  modbusOutstation.SetAttribute ("JitterMinNs", DoubleValue (1000));
  modbusOutstation.SetAttribute ("JitterMaxNs", DoubleValue (10000));

  // -- Compromised-endpoint FDI configuration --
  modbusOutstation.SetAttribute ("FdiFlag", BooleanValue (true));
  modbusOutstation.SetAttribute ("FdiID", UintegerValue (1));
  modbusOutstation.SetAttribute ("NodeID", StringValue ("Node1"));
  modbusOutstation.SetAttribute ("PointID", StringValue ("TestRegister1"));
  modbusOutstation.SetAttribute ("Value_attck", StringValue ("132000"));
  modbusOutstation.SetAttribute ("RealVal", StringValue ("100"));
  modbusOutstation.SetAttribute ("AttackStartTime", StringValue ("10"));
  modbusOutstation.SetAttribute ("AttackEndTime", StringValue ("20"));

  Ptr<ModbusApplicationNew> outstation =
    modbusOutstation.Install (nodes.Get (1), std::string ("ModbusOutstationFDI1"));

  // -- Master (client), unchanged --
  ModbusApplicationHelperNew modbusMaster ("ns3::TcpSocketFactory",
                                            InetSocketAddress (masterAddress, masterPort));
  modbusMaster.SetAttribute ("LocalPort", UintegerValue (masterPort));
  modbusMaster.SetAttribute ("RemoteAddress", AddressValue (outstationAddress));
  modbusMaster.SetAttribute ("RemotePort", UintegerValue (outstationPort));
  modbusMaster.SetAttribute ("isMaster", BooleanValue (true));
  modbusMaster.SetAttribute ("Name", StringValue ("ModbusMaster1"));
  modbusMaster.SetAttribute ("PointsFilename", StringValue (pointsFile));
  modbusMaster.SetAttribute ("UnitId", UintegerValue (1));
  modbusMaster.SetAttribute ("EnableTCP", BooleanValue (true));
  modbusMaster.SetAttribute ("JitterMinNs", DoubleValue (1000));
  modbusMaster.SetAttribute ("JitterMaxNs", DoubleValue (10000));

  Ptr<ModbusApplicationNew> master =
    modbusMaster.Install (nodes.Get (0), std::string ("ModbusMaster1"));

  Simulator::Schedule (MilliSeconds (1005), &ModbusApplicationNew::periodic_poll,
                        master, pollIntervalMs);

  Simulator::Stop (Seconds (simTimeSeconds));
  NS_LOG_INFO ("Starting simulation...");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation complete.");

  return 0;
}
