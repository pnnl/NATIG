/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Authors: Jaume Nin <jaume.nin@cttc.cat>
 *          Manuel Requena <manuel.requena@cttc.es>
 */

#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cassert>
#include <math.h>
#include <stdio.h>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
//#include "ns3/gtk-config-store.h"
#include "ns3/csma-module.h"

#include "ns3/dnp3-application-helper.h"
#include "ns3/dnp3-simulator-impl.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It also starts another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("RoutingDev");
Time baseDate("1509418800s");
static void
PacketCapture (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  file->Write (baseDate + Simulator::Now (), p);
}

void
enablePcapAllBaseTime (std::string prefix, NodeContainer csmas, NodeContainer p2ps)
{
	NS_LOG_INFO("enablePcapAllBaseTime.");
	std::string filename;
	PcapHelper pcapHelper;
	for (NodeContainer::Iterator i = csmas.Begin (); i != csmas.End (); ++i)
            {
	        Ptr<Node> node = *i;
		for (uint32_t j = 0; j < node->GetNDevices (); ++j)
		    {
		          Ptr<NetDevice> dev = node->GetDevice(j);
		          Ptr<CsmaNetDevice> device = dev->GetObject<CsmaNetDevice> ();
		          filename = pcapHelper.GetFilenameFromDevice (prefix, dev);

			  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out, PcapHelper::DLT_EN10MB);
			  dev->TraceConnectWithoutContext ("PromiscSniffer", MakeBoundCallback (&PacketCapture, file));;

														              
		    }					   
	    }

	  for (NodeContainer::Iterator i = p2ps.Begin (); i != p2ps.End (); ++i)
	      {
	           Ptr<Node> node = *i;
	           for (uint32_t j = 0; j < node->GetNDevices (); ++j)
	               {
	                    Ptr<NetDevice> dev = node->GetDevice(j);
	                    Ptr<PointToPointNetDevice> device = dev->GetObject<PointToPointNetDevice> ();
	                    filename = pcapHelper.GetFilenameFromDevice (prefix, dev);

			    Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out,  PcapHelper::DLT_PPP);
			    dev->TraceConnectWithoutContext ("PromiscSniffer", MakeBoundCallback (&PacketCapture, file));;

		        }
	      }

}


void PrintRoutingTable (Ptr<Node>& n)
{
    Ptr<Ipv4StaticRouting> routing = 0;
    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4> ipv6 = n->GetObject<Ipv4> ();
    uint32_t nbRoutes = 0;
    Ipv4RoutingTableEntry route;

    routing = routingHelper.GetStaticRouting (ipv6);

    std::cout << "Routing table of " << n << " : " << std::endl;
    std::cout << "Destination\t" << "Gateway\t" << "Interface\t" << std::endl;

    nbRoutes = routing->GetNRoutes ();
    for (uint32_t i = 0; i < nbRoutes; i++)
    {
       route = routing->GetRoute (i);
       std::cout << route.GetDest () << "\t"
                 << route.GetGateway () << "\t"
                 << route.GetInterface () << "\t"
                 << std::endl;
    }
}

int
main (int argc, char *argv[])
{
  uint16_t numNodePairs = 1;
  Time simTime = MilliSeconds (25000);
  double distance = 60.0;
  Time interPacketInterval = MilliSeconds (100);
  bool useCa = false;
  bool disableDl = false;
  bool disableUl = false;
  bool disablePl = false;
  int includeMIM = 0; //Default is to use MIM
  int attackSelection = 2; //Default is to have no attack selection
  int attackStartTime = 1000;
  int attackEndTime = 6000;

  // Command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("numNodePairs", "Number of eNodeBs + UE pairs", numNodePairs);
  cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.AddValue ("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
  cmd.AddValue ("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.AddValue ("disableDl", "Disable downlink data flows", disableDl);
  cmd.AddValue ("disableUl", "Disable uplink data flows", disableUl);
  cmd.AddValue ("disablePl", "Disable data flows between peer UEs", disablePl);
  cmd.AddValue ("includeMIM", "1=Include MIM functionality, 0=don't include MIM functionality.", includeMIM);
  cmd.AddValue ("attackSelection", "1=Disconnect the routing process, 2=Send packet with payload '0' to the destination", attackSelection);
  cmd.AddValue ("attackStartTime", "Set start time of the attack in seconds", attackStartTime);
  cmd.AddValue ("attackEndTime", "Set end time of the attack in seconds", attackEndTime);
  cmd.Parse (argc, argv);

  //ConfigStore inputConfig;
  //inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line
  cmd.Parse(argc, argv);

  if (useCa)
      //arp->SetAttribute ("RequestJitter", StringValue ("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"));
   {
     Config::SetDefault ("ns3::LteHelper::UseCa", BooleanValue (useCa));
     Config::SetDefault ("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue (2));
     Config::SetDefault ("ns3::LteHelper::EnbComponentCarrierManager", StringValue ("ns3::RrComponentCarrierManager"));
   }

  LogComponentEnable("Dnp3Application", LOG_LEVEL_INFO);
  LogComponentEnable ("Dnp3SimulatorImpl", LOG_LEVEL_INFO); 
  LogComponentEnable ("Ipv4L3ProtocolMIM", LOG_LEVEL_INFO);

  Dnp3SimulatorImpl *hb=new Dnp3SimulatorImpl();
  Ptr<Dnp3SimulatorImpl> hb2(hb);
  hb->Unref();
  Simulator::SetImplementation(hb2);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  InternetStackHelperMIM internetMIM;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (numNodePairs);
  ueNodes.Create (numNodePairs);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numNodePairs; i++)
    {
      positionAlloc->Add (Vector (distance * i, 0, 0));
    }
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach one UE per eNodeB
  for (uint16_t i = 0; i < numNodePairs; i++)
    {
      lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
      // side effect: the default EPS bearer will be activated
    }

  //This is where the substation setup starts
  NodeContainer subNodes;
  subNodes.Create (2);
  internet.Install (subNodes);

  //Creating the man in the middle attacker
  NodeContainer MIM;
  MIM.Create(2);
  if (includeMIM){
    internetMIM.Install (MIM.Get(0)); //.Get(0));
    internet.Install (MIM.Get(1));
  }else{
    internet.Install (MIM);
  }
  //internet.Install (MIM.Get(1));

  NodeContainer csmaSubNodes (ueNodes.Get (0), MIM.Get(0), subNodes.Get(0));
  NodeContainer csmaSubNodes2 (ueNodes.Get (0), MIM.Get(1), subNodes.Get (1));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  csma.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

  NetDeviceContainer internetDevicesSub = csma.Install (csmaSubNodes);
  NetDeviceContainer internetDevicesSub2 = csma.Install (csmaSubNodes2);


  NodeContainer ncP2P_nodes (ueNodes.Get(0), MIM.Get(0), MIM.Get(1), subNodes.Get(0), subNodes.Get(1));

  //Assign IP Address
  Ipv4AddressHelper ipv4Sub;
  ipv4Sub.SetBase ("172.17.0.0", "255.255.0.0", "0.0.0.1");
  Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign (internetDevicesSub);
  Ipv4AddressHelper ipv4Sub2;
  ipv4Sub2.SetBase ("172.18.0.0", "255.255.0.0", "0.0.0.1");
  Ipv4InterfaceContainer interfacesSub2 = ipv4Sub2.Assign (internetDevicesSub2);

  Ipv4InterfaceContainer inter;
  inter.Add(interfacesSub.Get(2));
  inter.Add(interfacesSub2.Get(2));

  Ipv4Address gateway = epcHelper->GetUeDefaultGatewayAddress ();
  
  Ptr<Node> ueNodeZero = ueNodes.Get (0);
  Ptr<Ipv4> ipv4 = ueNodeZero->GetObject<Ipv4> (); 
  Ipv4Address addr2 = ipv4->GetAddress (2, 0).GetLocal ();
  Ipv4Address addr4 = ipv4->GetAddress (3, 0).GetLocal ();
  Ptr<Ipv4> ipv42 = MIM.Get(0)->GetObject<Ipv4> ();
  Ipv4Address addr5 = ipv42->GetAddress(1,0).GetLocal();
  Ptr<Ipv4> ipv43 = MIM.Get(1)->GetObject<Ipv4>();
  Ipv4Address addr6 = ipv43->GetAddress(1,0).GetLocal();

  std::cout << ipv42->GetNInterfaces() << "\n";
  std::cout << ipv42->GetNAddresses(1) << "\n";
  std::cout << gateway << "\n";
  std::cout << addr2 << "\n";
  std::cout << addr4 << "\n";
  std::cout << addr5 << "\n";



  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("172.0.0.0"), Ipv4Mask ("255.0.0.0"), gateway, 1);

  Ptr<Ipv4StaticRouting> subNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (MIM.Get(0)->GetObject<Ipv4>());
  subNodeStaticRouting->AddNetworkRouteTo (Ipv4Address ("1.0.0.0"), Ipv4Mask ("255.0.0.0"), addr2, 1);

  Ptr<Ipv4StaticRouting> subNodeStaticRouting4 = ipv4RoutingHelper.GetStaticRouting (MIM.Get(1)->GetObject<Ipv4>());
  subNodeStaticRouting4->AddNetworkRouteTo (Ipv4Address ("1.0.0.0"), Ipv4Mask ("255.0.0.0"), addr4, 1);

  Ptr<Ipv4StaticRouting> subNodeStaticRouting3 = ipv4RoutingHelper.GetStaticRouting (subNodes.Get(0)->GetObject<Ipv4>());
  subNodeStaticRouting3->AddNetworkRouteTo (Ipv4Address ("1.0.0.0"), Ipv4Mask ("255.0.0.0"), addr5, 1);


  Ptr<Ipv4StaticRouting> subNodeStaticRouting2 = ipv4RoutingHelper.GetStaticRouting (subNodes.Get(1)->GetObject<Ipv4>());
  subNodeStaticRouting2->AddNetworkRouteTo (Ipv4Address ("1.0.0.0"), Ipv4Mask ("255.0.0.0"), addr6, 1);


  //std::cout << "UE Node routing table" << "\n";
  //PrintRoutingTable (ueNodeZero);

  // Install and start applications on UEs and remote host
  if (includeMIM){
    Ptr<Ipv4> ip = MIM.Get(0)->GetObject<Ipv4>();
    ip->GetObject<Ipv4L3ProtocolMIM> ()->victimAddr = remoteHostAddr;//remoteHostsInterfaces.GetAddress (0);
  }
  //Ptr<Ipv4> ip2 = MIM.Get(1)->GetObject<Ipv4>();
  //ip2->GetObject<Ipv4L3ProtocolMIM> ()->victimAddr = remoteHostAddr;
  //ip->GetObject<Ipv4L3ProtocolMIM> ()->sourceAddr = Ipv4Address("7.0.0.2");
  uint16_t dlPort = (rand() % 16380 + 49152);
  uint16_t mimPort = dlPort;
  uint16_t ulPort = 2000;
  uint16_t otherPort = 3000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  for (uint32_t u = 0; u < subNodes.GetN (); ++u)
    {

      //++dlPort;
      //dlPort = (rand() % 16380 + 49152);
      Dnp3ApplicationHelper dnp3Master1 ("ns3::UdpSocketFactory",
		      		         InetSocketAddress (remoteHostAddr, dlPort));
      dnp3Master1.SetAttribute("LocalPort", UintegerValue(dlPort));
      dnp3Master1.SetAttribute("RemoteAddress", AddressValue(inter.GetAddress (u)));
      dnp3Master1.SetAttribute("RemotePort", UintegerValue(20000));
      dnp3Master1.SetAttribute("JitterMinNs", DoubleValue (500));
      dnp3Master1.SetAttribute("JitterMaxNs", DoubleValue (1000));
      dnp3Master1.SetAttribute("isMaster", BooleanValue (true));
      dnp3Master1.SetAttribute ("Name", StringValue (std::string("control_center")+std::to_string(1+u).c_str()));
      dnp3Master1.SetAttribute ("PointsFilename", StringValue ("points.csv"));
      dnp3Master1.SetAttribute("MasterDeviceAddress", UintegerValue(1));
      dnp3Master1.SetAttribute("StationDeviceAddress", UintegerValue(2+u));
      dnp3Master1.SetAttribute("IntegrityPollInterval", UintegerValue (10));
      dnp3Master1.SetAttribute("EnableTCP", BooleanValue (false));
      Ptr<Dnp3Application> master1 = dnp3Master1.Install (remoteHost, std::string("control_center")+std::to_string(1+u));
      serverApps.Add(master1);

      Simulator::Schedule(MilliSeconds(4000+11*u), &Dnp3Application::periodic_poll, master1, 0);

      //Add the substation code here
      Dnp3ApplicationHelper dnp3Outstation1 ("ns3::UdpSocketFactory",
		                              InetSocketAddress (inter.GetAddress (u), 20000));
      dnp3Outstation1.SetAttribute("LocalPort", UintegerValue(20000));
      dnp3Outstation1.SetAttribute("RemoteAddress", AddressValue(remoteHostAddr));
      dnp3Outstation1.SetAttribute("RemotePort", UintegerValue(dlPort));
      dnp3Outstation1.SetAttribute("isMaster", BooleanValue (false));
      dnp3Outstation1.SetAttribute ("Name", StringValue (std::string("GLD")+std::to_string(2+u).c_str()));
      dnp3Outstation1.SetAttribute ("PointsFilename", StringValue ("points.csv"));
      dnp3Outstation1.SetAttribute("MasterDeviceAddress", UintegerValue(1));
      dnp3Outstation1.SetAttribute("StationDeviceAddress", UintegerValue(2+u));
      dnp3Outstation1.SetAttribute("EnableTCP", BooleanValue (false));
      Ptr<Dnp3Application> station1 = dnp3Outstation1.Install (subNodes.Get(u), std::string("GLD")+std::to_string(2+u));		
      clientApps.Add(station1);
      dlPort = (rand() % 16380 + 49152);


   }
   NS_LOG_INFO("Installing MIM server spoofing as SS.");
   //The MIM node that will conduct the man in the middle attack
   if (includeMIM){
     Dnp3ApplicationHelper dnp3MIM1 ("ns3::UdpSocketFactory",
                                InetSocketAddress (interfacesSub.GetAddress(1), mimPort));

     dnp3MIM1.SetAttribute("LocalPort", UintegerValue(mimPort));
     dnp3MIM1.SetAttribute("RemoteAddress", AddressValue(remoteHostAddr));
     dnp3MIM1.SetAttribute("RemotePort", UintegerValue(20000));
     dnp3MIM1.SetAttribute ("PointsFilename", StringValue ("./points.csv"));
     dnp3MIM1.SetAttribute("JitterMinNs", DoubleValue (500));
     dnp3MIM1.SetAttribute("JitterMaxNs", DoubleValue (1000));
     dnp3MIM1.SetAttribute("isMaster", BooleanValue (false));
     dnp3MIM1.SetAttribute ("Name", StringValue ("MIM"));
     dnp3MIM1.SetAttribute("MasterDeviceAddress", UintegerValue(1));
     dnp3MIM1.SetAttribute("StationDeviceAddress", UintegerValue(2));
     dnp3MIM1.SetAttribute("IntegrityPollInterval", UintegerValue (10));
     dnp3MIM1.SetAttribute("EnableTCP", BooleanValue (false));
     dnp3MIM1.SetAttribute("AttackSelection", UintegerValue(attackSelection));
     dnp3MIM1.SetAttribute("AttackStartTime", UintegerValue(attackStartTime));
     dnp3MIM1.SetAttribute("AttackEndTime", UintegerValue(attackEndTime));
     dnp3MIM1.SetAttribute("mitmFlag", BooleanValue(true));
     Ptr<Dnp3Application> mim = dnp3MIM1.Install (MIM.Get(0), std::string("MIM"));
     ApplicationContainer dnpMIMApp(mim);

     dnpMIMApp.Start (Seconds (0.0));
     dnpMIMApp.Stop (Seconds (10000));
   }
   serverApps.Start(Seconds (0.0));
   serverApps.Stop (Seconds (10000));
   clientApps.Start(Seconds (0.0));
   clientApps.Stop (Seconds (10000));
  

  //Simulator::Schedule(MilliSeconds(4000), &Dnp3Application::periodic_poll, master1, 0);
  //Simulator::Schedule(MilliSeconds(4011), &Dnp3Application::periodic_poll, serverApps.Get(1), 0);

  lteHelper->EnableTraces ();
  // Uncomment to enable PCAP tracing
  //p2ph.EnablePcapAll("routing-dev");
  //csma.EnablePcapAll("routing-dev-csma", true);
  enablePcapAllBaseTime("radics-exercise2-utility1-1day", remoteHostContainer, ncP2P_nodes);

  //Simulator::Stop (simTime);
  Simulator::Run ();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy ();
  return 0;
}
