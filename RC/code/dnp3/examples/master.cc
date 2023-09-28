/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/dnp3-application-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DNP3MasterExample");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  LogComponentEnable ("DNP3MasterExample", LOG_LEVEL_ALL);
  LogComponentEnable ("Dnp3Application", LOG_LEVEL_ALL);
//  LogComponentEnable ("PointToPointChannel", LOG_LEVEL_ALL);
//  LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_ALL);

//  LogComponentEnable ("TcpSocketImpl", LOG_LEVEL_ALL);
  NodeContainer nodes;
  nodes.Create (2);

//  PointToPointHelper pointToPoint;
//  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
//  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (400000)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer devices;
  devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  uint16_t port = 4000;

  Dnp3ApplicationHelper dnp3Master ("ns3::TcpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (0), port));
  //NS_LOG_INFO ("sETTING DNP3 parameters");

  dnp3Master.SetAttribute("LocalPort", UintegerValue(4000));
  dnp3Master.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress (1)));
  dnp3Master.SetAttribute("RemotePort", UintegerValue(4000));

  dnp3Master.SetAttribute("JitterMinNs", DoubleValue (10));
  dnp3Master.SetAttribute("JitterMaxNs", DoubleValue (100));
  dnp3Master.SetAttribute("isMaster", BooleanValue (true));
  dnp3Master.SetAttribute ("Name", StringValue ("Master"));
  dnp3Master.SetAttribute("MasterDeviceAddress", UintegerValue(1));
  dnp3Master.SetAttribute("StationDeviceAddress", UintegerValue(2));
  dnp3Master.SetAttribute("IntegrityPollInterval", UintegerValue (10));
  dnp3Master.SetAttribute("EnableTCP", BooleanValue (true));

    ApplicationContainer dnpMasterApp = dnp3Master.Install (nodes.Get (0));

//  PacketSinkHelper sink ("ns3::TcpSocketFactory",
//                         InetSocketAddress (interfaces.GetAddress (0), port));
//
//  ApplicationContainer apps = sink.Install(nodes.Get (0));

//  apps.Start (Seconds (0.0));
//  apps.Stop (Seconds (2000.0));
  Dnp3ApplicationHelper dnp3Outstation ("ns3::TcpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), port));
  dnp3Outstation.SetAttribute("LocalPort", UintegerValue(4000));
  dnp3Outstation.SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress (0)));
  dnp3Outstation.SetAttribute("RemotePort", UintegerValue(4000));
  dnp3Outstation.SetAttribute("isMaster", BooleanValue (false));
  dnp3Outstation.SetAttribute ("Name", StringValue ("Outstation"));
  dnp3Outstation.SetAttribute("MasterDeviceAddress", UintegerValue(1));
  dnp3Outstation.SetAttribute("StationDeviceAddress", UintegerValue(2));
  dnp3Outstation.SetAttribute("EnableTCP", BooleanValue (true));

  ApplicationContainer dnpOutstationApp = dnp3Outstation.Install (nodes.Get (1));

  dnpMasterApp.Start (Seconds (5.0));
  dnpMasterApp.Stop (Seconds (10000.0));
  dnpOutstationApp.Start (Seconds (0.0));
  dnpOutstationApp.Stop (Seconds (10000.0));

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("dnp3-example.tr"));
  p2p.EnablePcapAll ("dnp3-example", false);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
