/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 *   Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation;
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * \file cttc-3gpp-channel-example.cc
 * \ingroup examples
 * \brief Channel Example
 *
 * This example describes how to setup a simulation using the 3GPP channel model
 * from TR 38.900. Topology consists by default of 2 UEs and 2 gNbs, and can be
 * configured to be either mobile or static scenario.
 *
 * The output of this example are default NR trace files that can be found in
 * the root ns-3 project folder.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include "ns3/nr-helper.h"
#include <ns3/buildings-helper.h>
#include "ns3/log.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/nr-mac-scheduler-tdma-rr.h"
#include "ns3/nr-module.h"
#include <ns3/antenna-module.h>

#include "ns3/csma-module.h"
#include "ns3/helics-helper.h"
#include "ns3/wifi-module.h"

#include "ns3/dnp3-application-helper-new.h"
#include "ns3/dnp3-simulator-impl.h"

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

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("IntagrationExample5G");

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

int
main (int argc, char *argv[])
{

  std::string configFileName;
  std::string helicsConfigFileName;
  std::string topologyConfigFileName;
  std::string pointFileDir = "./";
  std::string pcapFileDir = "./";
  uint32_t i;
  int attackSelection = 2; //std::stoi(parameters["attackSel"]); //Default is to have no attack selection
  int attackStartTime = 120; //std::stoi(parameters["start"]);
  int attackEndTime   = 180; //std::stoi(parameters["end"]);
  CommandLine cmd;
  Json::Value configObject;
  Json::Value helicsConfigObject;
  Json::Value topologyConfigObject;

  //5G parameter settings
  std::string scenario = "UMa"; //scenario
  double frequency = 28e9; // central frequency
  double bandwidth = 100e6; //bandwidth
  double mobility = false; //whether to enable mobility
  double simTime = 180; // in second
  double speed = 1; // in m/s for walking UT.
  bool logging = true; //whether to enable logging from the simulation, another option is by exporting the NS_LOG environment variable
  double hBS; //base station antenna height in meters
  double hUT; //user antenna height in meters
  double txPower = 40; // txPower
  enum BandwidthPartInfo::Scenario scenarioEnum = BandwidthPartInfo::UMa;


  //Getting the configuration parameters
  std::cout << "number of args: " << argc << std::endl;
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("helicsConfig", "Helics configuration file path", helicsConfigFileName);
  cmd.AddValue("microGridConfig", "NS3 MicroGrid configration file path", configFileName);
  cmd.AddValue("topologyConfig", "NS3 topology configuration file path", topologyConfigFileName);
  cmd.AddValue("pointFileDir", "Points file path", pointFileDir);
  cmd.AddValue("pcapFileDir", "PCAP output file path", pcapFileDir);
  cmd.AddValue ("scenario", "The scenario for the simulation. Choose among 'RMa', 'UMa', 'UMi-StreetCanyon', 'InH-OfficeMixed', 'InH-OfficeOpen'.", scenario);
  cmd.AddValue ("frequency", "The central carrier frequency in Hz.", frequency);
  cmd.AddValue ("mobility","If set to 1 UEs will be mobile, when set to 0 UE will be static. By default, they are mobile.", mobility);
  cmd.AddValue ("logging","If set to 0, log components will be disabled.",logging);
  cmd.Parse (argc, argv);

  // enable logging
  if (logging)
    {
      LogComponentEnable ("ThreeGppPropagationLossModel", LOG_LEVEL_ALL);
      LogComponentEnable ("IntagrationExample5G", LOG_LEVEL_INFO);
      LogComponentEnable ("Dnp3ApplicationNew", LOG_LEVEL_INFO);
      LogComponentEnable ("Names", LOG_LEVEL_LOGIC);
    }

  /*
   * Default values for the simulation. We are progressively removing all
   * the instances of SetDefault, but we need it for legacy code (LTE)
   */
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (999999999));

  std::cout << "Helics configuration file: " << helicsConfigFileName.c_str() << std::endl;
  std::cout << "MicroGrid configuration file: " << configFileName.c_str() << std::endl;

  readMicroGridConfig(configFileName, configObject);
  readMicroGridConfig(helicsConfigFileName, helicsConfigObject);
  readMicroGridConfig(topologyConfigFileName, topologyConfigObject);

  scenario = topologyConfigObject["5GSetup"][0]["scenario"].asString();

  HelicsHelper helicsHelper(6000);
  std::cout << "Calling Calling Message Federate Constructor" << std::endl;
  helicsHelper.SetupApplicationFederate();

  std::string fedName = helics_federate->getName();

  // set mobile device and base station antenna heights in meters, according to the chosen scenario
  if (scenario.compare ("RMa") == 0)
    {
      hBS = 35;
      hUT = 1.5;
      scenarioEnum = BandwidthPartInfo::RMa;
    }
  else if (scenario.compare ("UMa") == 0)
    {
      hBS = 25;
      hUT = 1.5;
      scenarioEnum = BandwidthPartInfo::UMa;
    }
  else if (scenario.compare ("UMi-StreetCanyon") == 0)
    {
      hBS = 10;
      hUT = 1.5;
      scenarioEnum = BandwidthPartInfo::UMi_StreetCanyon;
    }
  else if (scenario.compare ("InH-OfficeMixed") == 0)
    {
      hBS = 3;
      hUT = 1;
      scenarioEnum = BandwidthPartInfo::InH_OfficeMixed;
    }
  else if (scenario.compare ("InH-OfficeOpen") == 0)
    {
      hBS = 3;
      hUT = 1;
      scenarioEnum = BandwidthPartInfo::InH_OfficeOpen;
    }
  else
    {
      NS_ABORT_MSG ("Scenario not supported. Choose among 'RMa', 'UMa', 'UMi-StreetCanyon', 'InH-OfficeMixed', and 'InH-OfficeOpen'.");
    }

  // create base stations and mobile terminals
  NodeContainer enbNodes;
  NodeContainer ueNodes;
  int numUE = std::stoi(topologyConfigObject["5GSetup"][0]["numUE"].asString());
  int numEnb = std::stoi(topologyConfigObject["5GSetup"][0]["numEnb"].asString());
  enbNodes.Create (numEnb);
  ueNodes.Create (numUE);

  // position the base stations
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  for (int x = 0; x < enbNodes.GetN(); x++){
    enbPositionAlloc->Add (Vector (0.0+x*10, 0.0+x*20, hBS));
  }
  //enbPositionAlloc->Add (Vector (0.0, 80.0, hBS));
  MobilityHelper enbmobility;
  enbmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbmobility.SetPositionAllocator (enbPositionAlloc);
  enbmobility.Install (enbNodes);

  // position the mobile terminals and enable the mobility
  MobilityHelper uemobility;
  uemobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  uemobility.Install (ueNodes);

  for (int x = 0; x < ueNodes.GetN(); x++){
    ueNodes.Get (x)->GetObject<MobilityModel> ()->SetPosition (Vector (90+x, 15*x, hUT)); // (x, y, z) in m
    //speed = speed*-1;
    //ueNodes.Get (x)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (0, speed, 0)); // move UE1 along the y axis
  }


  /*
   * Create NR simulation helpers
   */
  Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper> ();
  epcHelper->SetAttribute ("S1uLinkDelay", TimeValue (MilliSeconds (std::stof(topologyConfigObject["5GSetup"][0]["S1uLinkDelay"].asString()))));
  Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject <IdealBeamformingHelper> ();
  Ptr<NrHelper> nrHelper = CreateObject<NrHelper> ();
  nrHelper->SetGnbPhyAttribute ("N2Delay", UintegerValue (std::stof(topologyConfigObject["5GSetup"][0]["N2Delay"].asString())));
  nrHelper->SetGnbPhyAttribute ("N1Delay", UintegerValue (std::stof(topologyConfigObject["5GSetup"][0]["N1Delay"].asString())));
  nrHelper->SetBeamformingHelper (idealBeamformingHelper);
  nrHelper->SetEpcHelper (epcHelper);

  /*
   * Spectrum configuration. We create a single operational band and configure the scenario.
   */
  BandwidthPartInfoPtrVector allBwps;
  CcBwpCreator ccBwpCreator;
  const uint8_t numCcPerBand = 1;  // in this example we have a single band, and that band is composed of a single component carrier

  /* Create the configuration for the CcBwpHelper. SimpleOperationBandConf creates
   * a single BWP per CC and a single BWP in CC.
   *
   * Hence, the configured spectrum is:
   *
   * |---------------Band---------------|
   * |---------------CC-----------------|
   * |---------------BWP----------------|
   */
  CcBwpCreator::SimpleOperationBandConf bandConf (frequency, bandwidth, numCcPerBand, scenarioEnum);
  OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc (bandConf);
  //Initialize channel and pathloss, plus other things inside band.
  nrHelper->InitializeOperationBand (&band);
  allBwps = CcBwpCreator::GetAllBwps ({band});

  // Configure ideal beamforming method
  idealBeamformingHelper->SetAttribute ("BeamformingMethod", TypeIdValue (DirectPathBeamforming::GetTypeId ()));

  // Configure scheduler
  nrHelper->SetSchedulerTypeId (NrMacSchedulerTdmaRR::GetTypeId ());

  // Antennas for the UEs
  nrHelper->SetUeAntennaAttribute ("NumRows", UintegerValue (2));
  nrHelper->SetUeAntennaAttribute ("NumColumns", UintegerValue (4));
  nrHelper->SetUeAntennaAttribute ("AntennaElement", PointerValue (CreateObject<IsotropicAntennaModel> ()));

  // Antennas for the gNbs
  nrHelper->SetGnbAntennaAttribute ("NumRows", UintegerValue (8));
  nrHelper->SetGnbAntennaAttribute ("NumColumns", UintegerValue (8));
  nrHelper->SetGnbAntennaAttribute ("AntennaElement", PointerValue (CreateObject<IsotropicAntennaModel> ()));

  // install nr net devices
  NetDeviceContainer enbNetDev = nrHelper->InstallGnbDevice (enbNodes, allBwps);
  NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice (ueNodes, allBwps);

  /*for (int i = 0; i < ueNetDev.GetN(); i++){
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.00001));
    ueNetDev.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  }*/

  int64_t randomStream = 1;
  randomStream += nrHelper->AssignStreams (enbNetDev, randomStream);
  randomStream += nrHelper->AssignStreams (ueNetDev, randomStream);

  for (int i = 0; i < enbNetDev.GetN(); i++){
      nrHelper->GetGnbPhy (enbNetDev.Get (0), 0)->SetTxPower (txPower);
  }
  //nrHelper->GetGnbPhy (enbNetDev.Get (1), 0)->SetTxPower (txPower);

  // When all the configuration is done, explicitly call UpdateConfig ()
  for (auto it = enbNetDev.Begin (); it != enbNetDev.End (); ++it)
    {
      DynamicCast<NrGnbNetDevice> (*it)->UpdateConfig ();
    }

  for (auto it = ueNetDev.Begin (); it != ueNetDev.End (); ++it)
    {
      DynamicCast<NrUeNetDevice> (*it)->UpdateConfig ();
    }

  // create the internet and install the IP stack on the UEs
  // get SGW/PGW and create a single RemoteHost
  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  InternetStackHelperMIM internetStackMIM;
  internet.Install (remoteHostContainer);
  internetStackMIM.Install (enbNodes);

  // connect a remoteHost to pgw. Setup routing too
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  internetStackMIM.Install (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueNetDev));

  //Creating the Microgrid Nodes
  NodeContainer Microgrid;
  Microgrid.Create(ueNodes.GetN());
  internet.Install(Microgrid);

  for (int i = 0; i < Microgrid.GetN(); i++){
	  Ptr<Node> tempNode = Microgrid.Get(i);
	  NetDeviceContainer NetDev = p2ph.Install(NodeContainer(tempNode, ueNodes.Get(i))); //star.GetSpokeNode(i)));
	  Ipv4AddressHelper ipv4Sub;
	  std::string address = std::to_string(i+2)+".0.0.0";
	  ipv4Sub.SetBase(address.c_str(), "255.0.0.0"); //, "0.0.0.1");
	  Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign(NetDev);
	  std::cout << "Second route" << std::endl;
	  Ptr<Ipv4StaticRouting> MicrogridRouting = ipv4RoutingHelper.GetStaticRouting (Microgrid.Get(i)->GetObject<Ipv4>());
	  MicrogridRouting->AddNetworkRouteTo (internetIpIfaces.GetAddress(0), Ipv4Mask("255.0.0.0"), ueIpIface.GetAddress(i), 1);
	  std::cout << "Microgrid Added" << std::endl;
	  remoteHostStaticRouting->AddNetworkRouteTo(Microgrid.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.0.0.0"), ueIpIface.GetAddress(i), 1);
  }

  // assign IP address to UEs, and install UDP downlink applications
  uint16_t dlPort = 1234;
  //ApplicationContainer clientApps;
  //ApplicationContainer serverApps;
  uint16_t port = 20000;
  uint16_t master_port = 40000;
  std::vector<uint16_t> mimPort;
  ApplicationContainer dnpOutstationApp, dnpMasterApp;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      mimPort.push_back(master_port);
      Ptr<Node> ueNode = ueNodes.Get (u);

      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }
  for (uint32_t u = 0; u < Microgrid.GetN(); u++){
      auto ep_name = configObject["microgrid"][u]["name"].asString();
      auto cc_name = configObject["controlCenter"]["name"].asString();
      std::cout << "Microgrid network node: " << ep_name << " " << configObject["microgrid"][u].size() << std::endl;
      Dnp3ApplicationHelperNew dnp3Master ("ns3::UdpSocketFactory", InetSocketAddress (internetIpIfaces.GetAddress(0), master_port)); //remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), master_port));  //star.GetHubIpv4Address(i), master_port));
      std::cout << "FFFFFFFFFFFFFFFFFFFFFF" << std::endl;
      dnp3Master.SetAttribute("LocalPort", UintegerValue(master_port));
      dnp3Master.SetAttribute("RemoteAddress", AddressValue(Microgrid.Get(u)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal()));//star.GetSpokeIpv4Address (i)));
      dnp3Master.SetAttribute("RemotePort", UintegerValue(port));
      dnp3Master.SetAttribute("JitterMinNs", DoubleValue (std::stoi(topologyConfigObject["Channel"][0]["jitterMin"].asString())));
      dnp3Master.SetAttribute("JitterMaxNs", DoubleValue (std::stoi(topologyConfigObject["Channel"][0]["jitterMax"].asString())));
      dnp3Master.SetAttribute("isMaster", BooleanValue (true));
      dnp3Master.SetAttribute("Name", StringValue (cc_name+ep_name));
      dnp3Master.SetAttribute("PointsFilename", StringValue (pointFileDir+"/points_"+ep_name+".csv"));
      dnp3Master.SetAttribute("MasterDeviceAddress", UintegerValue(1));
      dnp3Master.SetAttribute("StationDeviceAddress", UintegerValue(u+2));
      dnp3Master.SetAttribute("IntegrityPollInterval", UintegerValue(10));
      dnp3Master.SetAttribute("EnableTCP", BooleanValue (false));
      
      Ptr<Dnp3ApplicationNew> master = dnp3Master.Install (remoteHostContainer.Get(0), std::string(cc_name+ep_name));
      dnpMasterApp.Add(master);
      Dnp3ApplicationHelperNew dnp3Outstation ("ns3::UdpSocketFactory", InetSocketAddress (Microgrid.Get(u)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port)); //star.GetSpokeIpv4Address (i), port));
      dnp3Outstation.SetAttribute("LocalPort", UintegerValue(port));
      dnp3Outstation.SetAttribute("RemoteAddress", AddressValue(internetIpIfaces.GetAddress(0)));//remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal())); //star.GetHubIpv4Address(i)));
      dnp3Outstation.SetAttribute("RemotePort", UintegerValue(master_port));
      dnp3Outstation.SetAttribute("isMaster", BooleanValue (false));
      dnp3Outstation.SetAttribute("Name", StringValue (ep_name));
      dnp3Outstation.SetAttribute("PointsFilename", StringValue (pointFileDir+"/points_"+ep_name+".csv"));
      dnp3Outstation.SetAttribute("MasterDeviceAddress", UintegerValue(1));
      dnp3Outstation.SetAttribute("StationDeviceAddress", UintegerValue(u+2));
      dnp3Outstation.SetAttribute("EnableTCP", BooleanValue (false));
      
      Ptr<Dnp3ApplicationNew> slave = dnp3Outstation.Install (Microgrid.Get(u), std::string(ep_name));
      dnpOutstationApp.Add(slave);
      Simulator::Schedule(MilliSeconds(1005), &Dnp3ApplicationNew::periodic_poll, master, 0);
      Simulator::Schedule(MilliSeconds(3005), &Dnp3ApplicationNew::send_control_analog, master, Dnp3ApplicationNew::DIRECT, 0, -16);
      //master_port += 1;
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

  // attach UEs to the closest eNB
  nrHelper->AttachToClosestEnb (ueNetDev, enbNetDev);

  //attack configuration
  std::map<std::string, std::string> attack;
  
  for (uint32_t j = 1; j < configObject["MIM"].size(); j++){
	  for(const auto& item : configObject["MIM"][j].getMemberNames() ){
		  std::string ID = "MIM-"+std::to_string(j)+"-"+item;
		  std::string my_str = configObject["MIM"][j][item].asString();
		  my_str.erase(remove(my_str.begin(), my_str.end(), '"'), my_str.end());
		  std::cout << "This is the keys value: " << ID << "  and the value is " << my_str << std::endl;
		  attack.insert(pair<std::string,std::string >(ID, my_str));
	  }
	  
  }

  //Getting a list of MIM
  //NOTE: The MIM nodes cannot be the GnB nodes (ns3 limitation)
  //  //For this scenario and maybe the 4G scenario we will need to use the UEs as the MIM nodes
  int includeMIM = std::stoi(configObject["Simulation"][0]["includeMIM"].asString());
  std::string IDsMIM = configObject["MIM"][0]["listMIM"].asString();
  size_t pos = 0;
  std::string delimiter = ",";
  std::vector<std::string> val;
  std::string token;
  std::string VI = IDsMIM;
  while ((pos = VI.find(delimiter)) != std::string::npos) {
	  token = VI.substr(0, pos);
	  val.push_back(token);
	  VI.erase(0, pos + delimiter.length());
  }
  val.push_back(VI);
  if (includeMIM == 1){
	  for (int x = 0; x < val.size(); x++){ //std::stoi(configObject["MIM"][0]["NumberAttackers"].asString()); x++){
		  int MIM_ID = std::stoi(val[x]) + 1; //x+1;
		  auto ep_name = configObject["MIM"][MIM_ID]["name"].asString();
		  Ptr<Node> tempnode = ueNodes.Get(MIM_ID-1); //star.GetSpokeNode (MIM_ID-1);
		  Names::Add(ep_name, tempnode);
		  std::string enamestring = ep_name;
		  Ptr<Ipv4> ip = Names::Find<Node>(enamestring)->GetObject<Ipv4>();
		  int ID = 1;
		  ip->GetObject<Ipv4L3ProtocolMIM> ()->victimAddr = internetIpIfaces.GetAddress(0); //Microgrid.Get(MIM_ID-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(); //internetIpIfaces.GetAddress(0);
		  
		  Dnp3ApplicationHelperNew dnp3MIM1 ("ns3::UdpSocketFactory", InetSocketAddress (tempnode->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port)); 
		  dnp3MIM1.SetAttribute("LocalPort", UintegerValue(port));
		  dnp3MIM1.SetAttribute("RemoteAddress", AddressValue(internetIpIfaces.GetAddress(0))); //star.GetHubIpv4Address(MIM_ID-1)));
		  if(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 3 || std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 4){
			  dnp3MIM1.SetAttribute("RemoteAddress2", AddressValue(Microgrid.Get(MIM_ID-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal()));
		  }
		  dnp3MIM1.SetAttribute("RemotePort", UintegerValue(mimPort[MIM_ID-1]));
		  
		  dnp3MIM1.SetAttribute ("PointsFilename", StringValue (pointFileDir+"/points_mg"+std::to_string(MIM_ID)+".csv"));
		  dnp3MIM1.SetAttribute("JitterMinNs", DoubleValue (500));
		  dnp3MIM1.SetAttribute("JitterMaxNs", DoubleValue (1000));
		  dnp3MIM1.SetAttribute("isMaster", BooleanValue (false));
		  dnp3MIM1.SetAttribute ("Name", StringValue (enamestring));
		  dnp3MIM1.SetAttribute("MasterDeviceAddress", UintegerValue(1));
 		  dnp3MIM1.SetAttribute("StationDeviceAddress", UintegerValue(2));
	    	  dnp3MIM1.SetAttribute("IntegrityPollInterval", UintegerValue (10));
	    	  dnp3MIM1.SetAttribute("EnableTCP", BooleanValue (false));
	    	  dnp3MIM1.SetAttribute("AttackSelection", UintegerValue(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]))); 

		  if (std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 2 || std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 4){
			  if(attack["MIM-"+std::to_string(MIM_ID)+"-scenario_id"] == "b"){
				  dnp3MIM1.SetAttribute("Value_attck_max", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-attack_val"]));
				  dnp3MIM1.SetAttribute("Value_attck_min", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-real_val"]));
				  dnp3MIM1.SetAttribute("NodeID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-node_id"])); 
				  dnp3MIM1.SetAttribute("PointID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-point_id"])); 
			  }
			  if(attack["MIM-"+std::to_string(MIM_ID)+"-scenario_id"] == "a"){
				  dnp3MIM1.SetAttribute("Value_attck", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-attack_val"]));
				  dnp3MIM1.SetAttribute("NodeID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-node_id"])); 
				  dnp3MIM1.SetAttribute("PointID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-point_id"])); 
			  }
		  }
		  
		  if(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 3){
			  dnp3MIM1.SetAttribute("Value_attck", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-attack_val"]));
			  dnp3MIM1.SetAttribute("NodeID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-node_id"])); 
			  dnp3MIM1.SetAttribute("PointID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-point_id"])); 
		  }
		  
		  dnp3MIM1.SetAttribute("AttackStartTime", UintegerValue(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-Start"]))); 
		  dnp3MIM1.SetAttribute("AttackEndTime", UintegerValue(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-End"]))); 
		  dnp3MIM1.SetAttribute("mitmFlag", BooleanValue(true));
		  Ptr<Dnp3ApplicationNew> mim = dnp3MIM1.Install (tempnode, enamestring);
		  ApplicationContainer dnpMIMApp(mim);
		  dnpMIMApp.Start (Seconds (0.0));
		  dnpMIMApp.Stop (Seconds(simTime));

	  }
  }

  // start server and client apps
  dnpMasterApp.Start (Seconds (0.0));
  dnpOutstationApp.Start (Seconds (0.0));
  dnpMasterApp.Stop (Seconds (simTime));
  dnpOutstationApp.Stop (Seconds (simTime));

  // enable the traces provided by the nr module
  nrHelper->EnableTraces ();

  //Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


