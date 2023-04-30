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
 *
 * Author: Joon-Seok Kim <joonseok.kim@pnnl.gov>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dnp3-helics-helper.h"
#include "ns3/dnp3-simulator-impl.h"
#include "ns3/ipv4-static-routing-helper.h"

#include "ns3/helics-helper.h"

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/writer.h>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string> 
#include <chrono>
#include <thread>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Network Topology
//
//        n2 n3 n4              .
//         \ | /                .
//          \|/                 .
//     n1--- n0---n5            .
//          /|\                 .
//         / | \                .
//        n8 n7 n6              .


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("IntagrationExample");

void readMicroGridConfig(std::string fpath, Json::Value& configobj)
{
    std::ifstream tifs(fpath);
    Json::Reader configreader;
    configreader.parse(tifs, configobj);
}

int dirExists(const char *path)
{
    struct stat info;

    if(stat( path, &info ) != 0)
        return 0;
    else if(info.st_mode & S_IFDIR)
        return 1;
    else
        return 0;
}

void EnablePcapAll(std::string dir, std::string prefix, NodeContainer nodecontainer, bool promiscuous=true)
{
  NetDeviceContainer devs;
  for (NodeContainer::Iterator i = nodecontainer.Begin (); i != nodecontainer.End (); ++i)
    {
      Ptr<Node> node = *i;
      for (uint32_t j = 0; j < node->GetNDevices (); ++j)
        {
          devs.Add (node->GetDevice (j));
        }
    }
  for (NetDeviceContainer::Iterator i = devs.Begin (); i != devs.End (); ++i)
    {
      Ptr<NetDevice> nd = *i;
      
      if (Ptr<PointToPointNetDevice> device = nd->GetObject<PointToPointNetDevice> ())
        {
          PcapHelper pcapHelper;

          std::string filename = pcapHelper.GetFilenameFromDevice (prefix, device);
          Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (dir+filename, std::ios::out, 
                                                            PcapHelper::DLT_PPP);
          pcapHelper.HookDefaultSink<PointToPointNetDevice> (device, "PromiscSniffer", file);
        }
    
    if (Ptr<CsmaNetDevice> device = nd->GetObject<CsmaNetDevice> ())
      {
        PcapHelper pcapHelper;

        std::string filename = pcapHelper.GetFilenameFromDevice (prefix, device);
        Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (dir+filename, std::ios::out, 
                                                          PcapHelper::DLT_EN10MB);
        if (promiscuous)
          {
            pcapHelper.HookDefaultSink<CsmaNetDevice> (device, "PromiscSniffer", file);
          }
        else
          {
            pcapHelper.HookDefaultSink<CsmaNetDevice> (device, "Sniffer", file);
          }
      
      }
    }

}


/*
 * The main() loop below represents the ns-3 model. The helics ns-3
 * integration will filter messages sent by MessageFederate instances by
 * creating HelicsApplication instances at Nodes. The name given to the
 * HelicsApplication should match a registered endpoint.
 */

int
main (int argc, char *argv[])
{
  bool verbose = true;
  std::string configFileName;
  std::string helicsConfigFileName;
  std::string pointFileDir = "./";
  std::string pcapFileDir = "./";
  uint32_t i;
  Time simTime = Seconds(10);
  CommandLine cmd;
  Json::Value configObject;
  Json::Value helicsConfigObject;

  // -------------------------------------------------------------------------------
  // HELICS FILTER config file calling
  // -------------------------------------------------------------------------------

  std::cout << "number of args: " << argc << std::endl;
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("helicsConfig", "Helics configuration file path", helicsConfigFileName);
  cmd.AddValue("microGridConfig", "NS3 MicroGrid configration file path", configFileName);
  cmd.AddValue("pointFileDir", "Points file path", pointFileDir);
  cmd.AddValue("pcapFileDir", "PCAP output file path", pcapFileDir);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable ("IntagrationExample", LOG_LEVEL_INFO);
    // LogComponentEnable ("HelicsSimulatorImpl", LOG_LEVEL_LOGIC);
    LogComponentEnable ("Dnp3HelicsApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("HelicsApplication", LOG_LEVEL_LOGIC);
    LogComponentEnable ("Names", LOG_LEVEL_LOGIC);
  }

  std::cout << "Helics configuration file: " << helicsConfigFileName.c_str() << std::endl;
  std::cout << "MicroGrid configuration file: " << configFileName.c_str() << std::endl;

  readMicroGridConfig(configFileName, configObject);
  readMicroGridConfig(helicsConfigFileName, helicsConfigObject);

  HelicsHelper helicsHelper;
  std::cout << "Calling Calling Message Federate Constructor" << std::endl;
  helicsHelper.SetupApplicationFederate();

  std::string fedName = helics_federate->getName();
  
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  PointToPointStarHelper star (configObject["microgrid"].size(), p2p);
  
  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  star.InstallStack (internet);
 
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0", "0.0.0.1");
  star.AssignIpv4Addresses (ipv4);

  uint16_t port = 20000;
  uint16_t master_port = 40000;
  ApplicationContainer dnpOutstationApp, dnpMasterApp;
  Ptr<Node> hubNode = star.GetHub ();
  for (i = 0;i < configObject["microgrid"].size();i++)
  {
    auto ep_name = configObject["microgrid"][i]["name"].asString();
    std::cout << "Microgrid network node: " << ep_name << std::endl;
    Ptr<Node> tempnode1 = star.GetSpokeNode (i);
    std::cout << "hub:" << star.GetHubIpv4Address(i) << ", spike:" << star.GetSpokeIpv4Address(i) << std::endl;

    auto cc_name = configObject["controlCenter"]["name"].asString();
    std::cout << "Control Center network node: " << cc_name << std::endl;
    
    Dnp3HelicsApplicationHelper dnp3Master ("ns3::UdpSocketFactory", InetSocketAddress (star.GetHubIpv4Address(i), master_port));

    dnp3Master.SetAttribute("LocalPort", UintegerValue(master_port));
    dnp3Master.SetAttribute("RemoteAddress", AddressValue(star.GetSpokeIpv4Address (i)));
    dnp3Master.SetAttribute("RemotePort", UintegerValue(port));
    dnp3Master.SetAttribute("JitterMinNs", DoubleValue (10));
    dnp3Master.SetAttribute("JitterMaxNs", DoubleValue (100));
    dnp3Master.SetAttribute("isMaster", BooleanValue (true));
    dnp3Master.SetAttribute("Name", StringValue (cc_name+ep_name));
    dnp3Master.SetAttribute("PointsFilename", StringValue (pointFileDir+"points_"+ep_name+".csv"));
    dnp3Master.SetAttribute("MasterDeviceAddress", UintegerValue(1));
    dnp3Master.SetAttribute("StationDeviceAddress", UintegerValue(2));
    dnp3Master.SetAttribute("IntegrityPollInterval", UintegerValue(10));
    dnp3Master.SetAttribute("EnableTCP", BooleanValue (false));
  
    Ptr<Dnp3HelicsApplication> master = dnp3Master.Install (hubNode, std::string(cc_name+ep_name));
    dnpMasterApp.Add(master);

    Dnp3HelicsApplicationHelper dnp3Outstation ("ns3::UdpSocketFactory", InetSocketAddress (star.GetSpokeIpv4Address (i), port));
    dnp3Outstation.SetAttribute("LocalPort", UintegerValue(port));
    dnp3Outstation.SetAttribute("RemoteAddress", AddressValue(star.GetHubIpv4Address(i)));
    dnp3Outstation.SetAttribute("RemotePort", UintegerValue(master_port));
    dnp3Outstation.SetAttribute("isMaster", BooleanValue (false));
    dnp3Outstation.SetAttribute("Name", StringValue (ep_name));
    dnp3Outstation.SetAttribute("PointsFilename", StringValue (pointFileDir+"/points_"+ep_name+".csv"));
    dnp3Outstation.SetAttribute("MasterDeviceAddress", UintegerValue(1));
    dnp3Outstation.SetAttribute("StationDeviceAddress", UintegerValue(2));
    dnp3Outstation.SetAttribute("EnableTCP", BooleanValue (false));

    Ptr<Dnp3HelicsApplication> slave = dnp3Outstation.Install (tempnode1, std::string(ep_name));
    dnpOutstationApp.Add(slave);
    Simulator::Schedule(MilliSeconds(1005), &Dnp3HelicsApplication::periodic_poll, master, 0);
    // Simulator::Schedule(MilliSeconds(2005), &Dnp3HelicsApplication::send_control_binary, master,
    //   Dnp3HelicsApplication::DIRECT, 0, ControlOutputRelayBlock::CLOSE);
      // Dnp3HelicsApplication::DIRECT, 0, ControlOutputRelayBlock::TRIP);
    Simulator::Schedule(MilliSeconds(3005), &Dnp3HelicsApplication::send_control_analog, master, 
      Dnp3HelicsApplication::DIRECT, 0, -16);
  }

  fedName = helics_federate->getName();
  std::cout << "Federate name: " << helics_federate->getName().c_str() << std::endl;
  int ep_count = helics_federate->getEndpointCount();
  for(int i=0; i < ep_count; i++){
    helics::Endpoint ep = helics_federate->getEndpoint(i);
    std::string epName = ep.getName();
    std::string ep_info = ep.getInfo();
    size_t pos = epName.find(fedName);
    if(pos != std::string::npos) {
        epName.erase(pos, fedName.length()+1);
      }
    std::cout << "Endpoint name: " << epName << std::endl;
  }
  dnpMasterApp.Start (Seconds (0.0));
  dnpMasterApp.Stop (simTime);
  dnpOutstationApp.Start (Seconds (0.0));
  dnpOutstationApp.Stop (simTime);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  if(!dirExists(pcapFileDir.c_str())) {
    mkdir(pcapFileDir.c_str(), 0777);
  }
  EnablePcapAll(pcapFileDir, "star", NodeContainer::GetGlobal());

  Simulator::Stop (simTime);
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
