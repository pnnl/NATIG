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
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/olsr-helper.h"
//#include "ns3/gtk-config-store.h"
#include "ns3/csma-module.h"

#include "ns3/helics-helper.h"
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

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeBs,
 * attaches one UE per eNodeB starts a flow for each UE to and from a remote host.
 * It also starts another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("RoutingDev");
Time baseDate("1509418800s");

Ptr<FlowMonitor> flowMonitor;
FlowMonitorHelper flowHelper;
std::map<FlowId, double> tp_transmitted;
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


void Throughput (){
        Ptr<Ipv4FlowClassifier> classifier=DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

        std::stringstream netStatsOut;
        std::stringstream netStatsOut2;
        std::string loc = std::getenv("RD2C");
        std::string loc1 = loc + "/integration/control/TP-Prob.txt";
        std::string loc2 = loc + "/integration/control/TP.txt";
        string proto;
        map< FlowId, FlowMonitor::FlowStats > stats = flowMonitor->GetFlowStats();
        std::vector <Ptr<FlowProbe>> xx = flowMonitor->GetAllProbes();
        for (int i = 0; i < xx.size(); i ++){
            map< FlowId, FlowProbe::FlowStats > probstats = xx[i]->GetStats();
            for (map< FlowId, FlowProbe::FlowStats>::iterator
                        flow=probstats.begin(); flow!=probstats.end(); flow++)
            {
                Ipv4FlowClassifier::FiveTuple  t = classifier->FindFlow(flow->first);
                switch(t.protocol)
                {
                        case(6):
                                proto = "TCP";
                                break;
                        case(17):
                                proto = "UDP";
                                break;
                        default:
                                exit(1);
                }

                netStatsOut2 << Simulator::Now ().GetSeconds () << " " <<  flow->first << " (" << proto << " " << t.sourceAddress << " / " << t.sourcePort << " --> " << t.destinationAddress << " / " << t.destinationPort << ") " << flow->second.bytes << " " << flow->second.packets << " " << flow->second.delayFromFirstProbeSum  << endl;


                FILE * pFile;
                pFile = fopen (loc1.c_str(),"a");
                if (pFile!=NULL)
                {
                        fprintf(pFile, netStatsOut2.str().c_str());
                        fclose (pFile);
                }

            }
        }
        for (map< FlowId, FlowMonitor::FlowStats>::iterator
                        flow=stats.begin(); flow!=stats.end(); flow++)
        {
                Ipv4FlowClassifier::FiveTuple  t = classifier->FindFlow(flow->first);
                switch(t.protocol)
                {
                        case(6):
                                proto = "TCP";
                                break;
                        case(17):
                                proto = "UDP";
                                break;
                        default:
                                exit(1);
                }
                if (tp_transmitted.find(flow->first) == tp_transmitted.end()) {
                    tp_transmitted[flow->first] = 0;
                }
		if (int(flow->first) < 21){
                double rx = (double)flow->second.rxBytes;
                netStatsOut << Simulator::Now ().GetSeconds () << " " <<  flow->first << " (" << proto << " " << t.sourceAddress << " / " << t.sourcePort << " --> " << t.destinationAddress << " / " << t.destinationPort << ") " << ((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024 << " " << flow->second.lostPackets << " " << rx - tp_transmitted[flow->first] << " " << flow->second.txBytes << " " << ((double)flow->second.txPackets-(double)flow->second.rxPackets)/(double)flow->second.txPackets << " " << (flow->second.delaySum.GetSeconds()/flow->second.rxPackets) << " " << flow->second.txPackets << " " << flow->second.rxPackets << " " << (flow->second.jitterSum.GetSeconds()/(flow->second.rxPackets))  << endl;
                tp_transmitted[flow->first] = (double)flow->second.rxBytes;                
                FILE * pFile;
                pFile = fopen (loc2.c_str(),"a");
                if (pFile!=NULL)
                {
                        fprintf(pFile, netStatsOut.str().c_str());
                        fclose (pFile);
                }
		}

        }
        Simulator::Schedule (Seconds (.5), &Throughput); // Callback every 0.5s

}

void updateUETable(NodeContainer subNodes, NodeContainer ueNodes){
    std::stringstream addrTrans;
    for (int i = 0; i < subNodes.GetN(); i++){
      int cc = 0;
      for (int j = 0; j < ueNodes.GetN(); j++){
        cc += 1;
        addrTrans << subNodes.Get(i)->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal() << ": " << ueNodes.Get(j)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << endl;
    }
  }

  std::cout << "The translation table" << std::endl;
  std::cout << addrTrans.str().c_str() << std::endl;

  FILE * addFile;
  std::string loc = std::getenv("RD2C");
  std::string loc1 = loc + "/integration/control/add.txt";
  addFile = fopen (loc1.c_str(),"w");

  if (addFile!=NULL)
  {
          fprintf(addFile, addrTrans.str().c_str());
          fclose (addFile);
  }
}

void setRoutingTable(NodeContainer remoteHostContainer, NodeContainer subNodes, NodeContainer MIM, NodeContainer ueNodes, Ipv4Address gateway){
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHostContainer.Get(0)->GetObject<Ipv4> ());
  for (int i = 0; i < ueNodes.GetN(); i++){
      remoteHostStaticRouting->AddNetworkRouteTo (ueNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), gateway, 1);
      int cc = 0;
      for (int j = 0; j < MIM.GetN(); j++){
        cc += 1;
        remoteHostStaticRouting->AddNetworkRouteTo (subNodes.Get(i)->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), Ipv4Mask ("255.255.0.0"), gateway, 1, 0);
      }
  }

  for (int i = 0; i < ueNodes.GetN(); i++){
    Ptr<Ipv4StaticRouting> ueNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get(i)->GetObject<Ipv4>());
    int cc = 0;
    for (int j = 0; j < subNodes.GetN(); j++){
      cc += 1;
      ueNodeStaticRouting->AddNetworkRouteTo(subNodes.Get(j)->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), Ipv4Mask("255.255.0.0"), MIM.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 2, 0);
    }
  }

  for (int i = 0; i < MIM.GetN(); i++){
    Ipv4Address addr2_ = ueNodes.Get(i)->GetObject<Ipv4>()->GetAddress (2, 0).GetLocal ();
    Ptr<Ipv4StaticRouting> subNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (MIM.Get(i)->GetObject<Ipv4>());
    subNodeStaticRouting->AddNetworkRouteTo (remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), addr2_, 1);
    int cc = 0;
    for (int j = 0; j < subNodes.GetN(); j++){
        cc += 1;
	int ind = i;
	subNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (MIM.Get(ind)->GetObject<Ipv4>());
        subNodeStaticRouting->AddNetworkRouteTo (subNodes.Get(j)->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), Ipv4Mask ("255.255.0.0"), cc, 0);
    }
  }

  for (int i = 0; i < subNodes.GetN(); i++){
    int cc = 0;
    Ptr<Ipv4StaticRouting> subNodeStaticRouting3 = ipv4RoutingHelper.GetStaticRouting (subNodes.Get(i)->GetObject<Ipv4>());
    for (int j = 0; j < MIM.GetN(); j++){
        cc += 1;
        Ptr<Ipv4> ipv4_2 = MIM.Get(j)->GetObject<Ipv4>();
	int ind = i+1;
	if (not ipv4_2->IsUp (ind)){
            ind += 1;
	}
        Ipv4Address addr5_ = ipv4_2->GetAddress(ind,0).GetLocal();
        subNodeStaticRouting3->AddNetworkRouteTo (remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.0.0.0"), addr5_, cc, 0);
    }
  }
}

void changeRoute (NodeContainer remoteHostContainer, NodeContainer subNodes, NodeContainer MIM, NodeContainer ueNodes, Ipv4Address gateway, int index) {
      std::cout << "Getting the available interfaces" << std::endl;
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      //for (int i = 0; i < MIM.GetN(); i++){
      Ptr<Ipv4> ipv4_2 = subNodes.Get(index-1)->GetObject<Ipv4>();
      int NewIndex = index + 1;
      if (NewIndex == subNodes.GetN()){
          NewIndex = index - 1;
      }
      remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetObject<Ipv4L3Protocol> ()->destAddr = subNodes.Get(index-1)->GetObject<Ipv4>()->GetAddress(index, 0).GetLocal();
      remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetObject<Ipv4L3Protocol> ()->NewdestAddr = subNodes.Get(index-1)->GetObject<Ipv4>()->GetAddress(NewIndex, 0).GetLocal();
      //MIM.Get(index-1)->GetObject<Ipv4>()->SetDown(index); 

      std::cout << "Setting the addresses" << std::endl;
      std::cout <<  remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetObject<Ipv4L3Protocol> ()->destAddr << std::endl;
      std::cout <<  remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetObject<Ipv4L3Protocol> ()->NewdestAddr << std::endl;

      Ipv4InterfaceAddress add12 = ipv4_2->GetAddress(NewIndex,0);
      Ipv4InterfaceAddress add1 = ipv4_2->GetAddress(index,0);
      ipv4_2->AddAddress (NewIndex, add1);
      ipv4_2->AddAddress (index, add12);
      ipv4_2->RemoveAddress (index, add1.GetLocal());
      ipv4_2->RemoveAddress (NewIndex, add12.GetLocal());
      setRoutingTable(remoteHostContainer, subNodes, MIM, ueNodes, gateway);
      updateUETable(subNodes, ueNodes);
      
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

  bool verbose = true;
  std::string configFileName;
  std::string helicsConfigFileName;
  std::string topologyConfigFileName;
  std::string pointFileDir = "./";
  std::string pcapFileDir = "./";
  uint32_t i;
  int attackSelection = 2; //std::stoi(parameters["attackSel"]); //Default is to have no attack selection
  int attackStartTime = 120; //std::stoi(parameters["start"]);
  int attackEndTime   = 180; //std::stoi(parameters["end"]);
  Json::Value configObject;
  Json::Value helicsConfigObject;
  Json::Value topologyConfigObject;
  int numBots = 0;

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
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("helicsConfig", "Helics configuration file path", helicsConfigFileName);
  cmd.AddValue("microGridConfig", "NS3 MicroGrid configration file path", configFileName);
  cmd.AddValue("topologyConfig", "NS3 topology configuration file path", topologyConfigFileName);
  cmd.AddValue("pointFileDir", "Points file path", pointFileDir);
  cmd.AddValue("pcapFileDir", "PCAP output file path", pcapFileDir);
  cmd.Parse (argc, argv);

  //ConfigStore inputConfig;
  //inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (1024 * 1024 * 1024)); //999999999));
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));


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


  std::cout << "Helics configuration file: " << helicsConfigFileName.c_str() << std::endl;
  std::cout << "MicroGrid configuration file: " << configFileName.c_str() << std::endl;

  readMicroGridConfig(configFileName, configObject);
  readMicroGridConfig(helicsConfigFileName, helicsConfigObject);
  readMicroGridConfig(topologyConfigFileName, topologyConfigObject);

  HelicsHelper helicsHelper(6000);
  std::cout << "Calling Calling Message Federate Constructor" << std::endl;
  helicsHelper.SetupApplicationFederate();

  std::string fedName = helics_federate->getName();

   simTime = Seconds(std::stof(configObject["Simulation"][0]["SimTime"].asString()));
   float start = std::stof(configObject["Simulation"][0]["StartTime"].asString());
   includeMIM = std::stoi(configObject["Simulation"][0]["includeMIM"].asString());
   bool DDoS = std::stoi(configObject["DDoS"][0]["Active"].asString());
   numBots = std::stoi(configObject["DDoS"][0]["NumberOfBots"].asString());
   distance = std::stod(topologyConfigObject["Gridlayout"][0]["distance"].asString());
   Config::SetDefault("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue(true));
   Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(std::stoi(topologyConfigObject["5GSetup"][0]["txPower"].asString())));
   Config::SetDefault("ns3::LteEnbRrc::DefaultTransmissionMode", UintegerValue(2));
   Config::SetDefault("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue(false));
   //Config::SetDefault("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue(false));
   // ALL SECTORS AND BANDS configuration
  Config::SetDefault ("ns3::FfMacScheduler::UlCqiFilter", EnumValue (FfMacScheduler::PUSCH_UL_CQI));
  Config::SetDefault ("ns3::LteUePhy::TxPower", DoubleValue (60));
  Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue (5.0));
  Config::SetDefault ("ns3::LteUePhy::EnableRlfDetection", BooleanValue (false));
  Config::SetDefault ("ns3::LteAmc::AmcModel", EnumValue (LteAmc::PiroEW2010));


  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  // use always LOS model
  lteHelper->SetAttribute ("PathlossModel",StringValue ("ns3::ThreeGppUmiStreetCanyonPropagationLossModel")); //"ns3::FriisPropagationLossModel"));
  //lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
  //lteHelper->SetFfrAlgorithmType("ns3::LteFrNoOpAlgorithm");
  uint8_t bandwidth = 100;
  lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(bandwidth));
  lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(bandwidth));
  //lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");
  //lteHelper->SetHandoverAlgorithmAttribute("Hysteresis",
  //                                       DoubleValue(6.0));
  //lteHelper->SetHandoverAlgorithmAttribute("TimeToTrigger",
  //                                       TimeValue(MilliSeconds(4.0)));
  //lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  InternetStackHelperMIM internetMIM;
  NS_LOG_INFO ("Enabling OLSR Routing.");
  OlsrHelper olsr;

  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 10);
  /*if (DDoS){
    internetMIM.SetRoutingHelper (list);
    internetMIM.Install (remoteHostContainer);
  }else{*/
    internet.SetRoutingHelper (list);
    internet.Install (remoteHostContainer);
  //}

  PointToPointHelper p2ph2;
  p2ph2.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (topologyConfigObject["Channel"][0]["P2PRate"].asString())));
  //p2ph2.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph2.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (std::stoi(topologyConfigObject["Channel"][0]["delay"].asString()))));

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (topologyConfigObject["Channel"][0]["P2PRate"].asString())));
  //p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (std::stoi(topologyConfigObject["Channel"][0]["delay"].asString()))));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  numNodePairs = std::stoi(topologyConfigObject["5GSetup"][0]["numUE"].asString());
  enbNodes.Create (numNodePairs);
  ueNodes.Create (numNodePairs);


  // Install Mobility Model
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator> ();
  Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator> ();
  int32_t yValue = std::stod(topologyConfigObject["Gridlayout"][0]["MinY"].asString());
  double gNbHeight = std::stod(topologyConfigObject["Gridlayout"][0]["GnBH"].asString());
  double ueHeight = std::stod(topologyConfigObject["Gridlayout"][0]["UEH"].asString());
  double xValue = std::stod(topologyConfigObject["Gridlayout"][0]["MinX"].asString());
  double minBigBoxX = 0.0; //-10.0;
  double minBigBoxY = 0.0; //-15.0;
  double maxBigBoxX = 20.0; //20;
  double maxBigBoxY =  20.0; //10;


  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  //mobility.SetPositionAllocator (apPositionAlloc);
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (std::stod(topologyConfigObject["Gridlayout"][0]["MinX"].asString())),
                                    "MinY", DoubleValue (std::stod(topologyConfigObject["Gridlayout"][0]["MinY"].asString())),
                                    "DeltaX", DoubleValue (std::stod(topologyConfigObject["Gridlayout"][0]["DeltaX"].asString())),
                                    "DeltaY", DoubleValue (std::stod(topologyConfigObject["Gridlayout"][0]["DeltaY"].asString())),
                                    "GridWidth", UintegerValue (std::stod(topologyConfigObject["Gridlayout"][0]["GridWidth"].asString())),
                                    "LayoutType", StringValue ("RowFirst"));
  mobility.Install (enbNodes);

  //mobility.SetPositionAllocator (staPositionAlloc);
  mobility.Install (ueNodes);
  //lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");
  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);
 

  // Install the IP stack on the UEs
  if (DDoS){
    internetMIM.SetRoutingHelper (list);
    internetMIM.Install (ueNodes); 
  }else{
    internet.SetRoutingHelper (list);
    internet.Install (ueNodes);
  }
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
  int numSSPUE = configObject["microgrid"].size(); //10;
  NodeContainer subNodes;
  subNodes.Create (numSSPUE);
  /*if (DDoS){
    internetMIM.SetRoutingHelper (list);
    internetMIM.Install (subNodes);
  }else{*/
    internet.SetRoutingHelper (list);
    internet.Install (subNodes);
  //}

  NodeContainer botNodes;
  botNodes.Create(numBots);
  mobility.Install (botNodes);
  //Creating the man in the middle attacker
  NodeContainer MIM;
  MIM.Create(numSSPUE);
  if (includeMIM){
    internetMIM.Install (MIM);
  }else{
    internet.Install (MIM);
  }

  std::vector<NodeContainer> csmaSubNodes;
  for (int i = 0; i < subNodes.GetN(); i++){
    std::cout << "Creating the csma nodes" << std::endl;
    for (int j = 0; j < ueNodes.GetN(); j++){
        NodeContainer csmaSubNodes_temp (ueNodes.Get(i), MIM.Get(i), subNodes.Get(j));
        csmaSubNodes.push_back(csmaSubNodes_temp);
    }
  }

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (topologyConfigObject["Channel"][0]["P2PRate"].asString()))); //"100Mb/s")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (0)));

  Ipv4InterfaceContainer inter;
  Ipv4InterfaceContainer inter_MIM;
  for (int i = 0; i < csmaSubNodes.size(); i++){
    NetDeviceContainer internetDevicesSub = csma.Install (csmaSubNodes[i]);

    Ipv4AddressHelper ipv4Sub;
    std::string address = "172."+std::to_string(17+i)+".0.0";
    ipv4Sub.SetBase (address.c_str(), "255.255.0.0", "0.0.0.1");
    Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign (internetDevicesSub);

    inter.Add(interfacesSub.Get(2));
    inter_MIM.Add(interfacesSub.Get(1));
  }

  updateUETable(subNodes, ueNodes);

  Ipv4Address gateway = epcHelper->GetUeDefaultGatewayAddress ();
  NodeContainer ncP2P_nodes;
  for (int i = 0; i < ueNodes.GetN(); i++){
    ncP2P_nodes.Add(ueNodes.Get(i));
  }
  for (int i = 0; i < MIM.GetN(); i++){
    ncP2P_nodes.Add(MIM.Get(i));
  }
  for (int i = 0; i < subNodes.GetN(); i++){
    ncP2P_nodes.Add(subNodes.Get(i));
  }

  NetDeviceContainer botDeviceContainer[numBots];
  for (int i = 0; i < numBots; ++i)
  {
          if (configObject["DDoS"][0]["NodeType"][0].asString().find("CC") != std::string::npos){
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), remoteHostContainer.Get(0));
          }else if (configObject["DDoS"][0]["NodeType"][0].asString().find("UE") != std::string::npos){
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), ueNodes.Get(int(i%MIM.GetN())));
          }else{
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), MIM.Get(int(i%MIM.GetN()))); //remoteHostContainer.Get(0));//We are currently attacking the remoteHost but I will need to change that in the future to be dynamic
          }
  }

  internet.Install(botNodes);
  Ipv4AddressHelper ipv4_n;
  ipv4_n.SetBase("30.0.0.0", "255.255.255.252");

  for (int j = 0; j < numBots; ++j)
  {
          ipv4_n.Assign(botDeviceContainer[j]);
          ipv4_n.NewNetwork();
  }


  setRoutingTable(remoteHostContainer, subNodes, MIM, ueNodes, gateway);
  changeRoute (remoteHostContainer, subNodes, MIM, ueNodes, gateway, 2);
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables();


  //std::cout << "UE Node routing table" << "\n";
  //PrintRoutingTable (ueNodeZero);

  // Install and start applications on UEs and remote host
  std::vector<std::string> val;
  if (includeMIM){
    /*for (int i = 0; i < MIM.GetN(); i++){
      Ptr<Ipv4> ip = MIM.Get(i)->GetObject<Ipv4>();
      ip->GetObject<Ipv4L3ProtocolMIM> ()->victimAddr = remoteHostAddr;//remoteHostsInterfaces.GetAddress (0);
    }*/
    std::string IDsMIM = configObject["MIM"][0]["listMIM"].asString();
    size_t pos = 0;
    std::string delimiter = ",";
    std::string token;
    std::string VI = IDsMIM;
    while ((pos = VI.find(delimiter)) != std::string::npos) {
	    token = VI.substr(0, pos);
	    val.push_back(token);
	    VI.erase(0, pos + delimiter.length());
    }
    val.push_back(VI);
  }
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


  uint16_t port = 20000;
  uint16_t master_port = 40000;
  ApplicationContainer dnpOutstationApp, dnpMasterApp;
  std::vector<uint16_t> mimPort;


  Ptr<EpcTft> tft = Create<EpcTft>();
  EpcTft::PacketFilter pf;
  pf.localPortStart = 1234;
  pf.localPortEnd = 1234;
  tft->Add(pf);
  lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, EpsBearer(EpsBearer::NGBR_VIDEO_TCP_DEFAULT),tft);

  for (i = 0;i < configObject["microgrid"].size();i++)
      {
            mimPort.push_back(master_port);
            auto ep_name = configObject["microgrid"][i]["name"].asString();
            std::cout << "Microgrid network node: " << ep_name << " " << subNodes.GetN() << " " << configObject["microgrid"][i].size() << " " << i << std::endl;
             Ptr<Node> tempnode1 = subNodes.Get(i);
	     auto cc_name = configObject["controlCenter"]["name"].asString();
	     std::cout << "Control Center network node: " << cc_name << std::endl;
	     
	     int ID = i+1;
	     Dnp3ApplicationHelperNew dnp3Master ("ns3::UdpSocketFactory", InetSocketAddress (remoteHostAddr, master_port));  //star.GetHubIpv4Address(i), master_port));
	     
	     dnp3Master.SetAttribute("LocalPort", UintegerValue(master_port));
	     dnp3Master.SetAttribute("RemoteAddress", AddressValue(tempnode1->GetObject<Ipv4>()->GetAddress(i+1,0).GetLocal()));//star.GetSpokeIpv4Address (i)));
	     dnp3Master.SetAttribute("RemotePort", UintegerValue(port));
	     dnp3Master.SetAttribute("JitterMinNs", DoubleValue (std::stoi(topologyConfigObject["Channel"][0]["jitterMin"].asString())));
	     dnp3Master.SetAttribute("JitterMaxNs", DoubleValue (std::stoi(topologyConfigObject["Channel"][0]["jitterMax"].asString())));
	     dnp3Master.SetAttribute("isMaster", BooleanValue (true));
	     dnp3Master.SetAttribute("Name", StringValue (cc_name+ep_name));
	     dnp3Master.SetAttribute("PointsFilename", StringValue (pointFileDir+"/points_"+ep_name+".csv"));
	     dnp3Master.SetAttribute("MasterDeviceAddress", UintegerValue(1));
	     dnp3Master.SetAttribute("StationDeviceAddress", UintegerValue(i+2));
	     dnp3Master.SetAttribute("IntegrityPollInterval", UintegerValue(10));
	     dnp3Master.SetAttribute("EnableTCP", BooleanValue (false));
	     
	     Ptr<Dnp3ApplicationNew> master = dnp3Master.Install (remoteHost, std::string(cc_name+ep_name));
	     dnpMasterApp.Add(master);
            
             	     
	     Dnp3ApplicationHelperNew dnp3Outstation ("ns3::UdpSocketFactory", InetSocketAddress (tempnode1->GetObject<Ipv4>()->GetAddress(i+1,0).GetLocal(), port)); //star.GetSpokeIpv4Address (i), port));
	     dnp3Outstation.SetAttribute("LocalPort", UintegerValue(port));
	     dnp3Outstation.SetAttribute("RemoteAddress", AddressValue(remoteHostAddr)); //star.GetHubIpv4Address(i)));
	     dnp3Outstation.SetAttribute("RemotePort", UintegerValue(master_port));
	     dnp3Outstation.SetAttribute("isMaster", BooleanValue (false));
	     dnp3Outstation.SetAttribute("Name", StringValue (ep_name));
	     dnp3Outstation.SetAttribute("PointsFilename", StringValue (pointFileDir+"/points_"+ep_name+".csv"));
	     dnp3Outstation.SetAttribute("MasterDeviceAddress", UintegerValue(1));
	     dnp3Outstation.SetAttribute("StationDeviceAddress", UintegerValue(i+2));
	     dnp3Outstation.SetAttribute("EnableTCP", BooleanValue (false));
	     
	     Ptr<Dnp3ApplicationNew> slave = dnp3Outstation.Install (tempnode1, std::string(ep_name));
	     dnpOutstationApp.Add(slave);
	     
	     Simulator::Schedule(MilliSeconds(1005), &Dnp3ApplicationNew::periodic_poll, master, std::stoi(configObject["Simulation"][0]["PollReqFreq"].asString()));
	     //Simulator::Schedule(MilliSeconds(3005), &Dnp3ApplicationNew::send_control_analog, master, 
	     //		           Dnp3ApplicationNew::DIRECT, 0, -16);
	     master_port += 1;
	     
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

  if (includeMIM == 1){
	  for (int x = 0; x < val.size(); x++){ //std::stoi(configObject["MIM"][0]["NumberAttackers"].asString()); x++){
		  int MIM_ID = std::stoi(val[x]) + 1; //x+1;
		  auto ep_name = configObject["MIM"][MIM_ID]["name"].asString();
		  Ptr<Node> tempnode = MIM.Get(MIM_ID-1); //star.GetSpokeNode (MIM_ID-1);
		  Names::Add(ep_name, tempnode);
		  std::string enamestring = ep_name;
		  Ptr<Ipv4> ip = Names::Find<Node>(enamestring)->GetObject<Ipv4>();
		  int ID = MIM_ID;
		  
		  
		  ip->GetObject<Ipv4L3ProtocolMIM> ()->victimAddr = remoteHostAddr; //subNodes.Get(MIM_ID-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(); //remoteHostAddr; //hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(ID,0).GetLocal(); //star.GetHubIpv4Address(MIM_ID-1);
                   Dnp3ApplicationHelperNew dnp3MIM1 ("ns3::UdpSocketFactory", InetSocketAddress (tempnode->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port)); //star.GetSpokeIpv4Address(MIM_ID-1),port)); 
		   dnp3MIM1.SetAttribute("LocalPort", UintegerValue(port));
		   dnp3MIM1.SetAttribute("RemoteAddress", AddressValue(remoteHostAddr)); //star.GetHubIpv4Address(MIM_ID-1)));
		   if(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 3 || std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 4){
			   dnp3MIM1.SetAttribute("RemoteAddress2", AddressValue(subNodes.Get(MIM_ID-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal())); //remoteHostAddr)); //inter.GetAddress(x)));
		   }
		   dnp3MIM1.SetAttribute("RemotePort", UintegerValue(mimPort[MIM_ID-1]));
		   
		   dnp3MIM1.SetAttribute ("PointsFilename", StringValue (pointFileDir+"/points_mg"+std::to_string(MIM_ID)+".csv"));
		   dnp3MIM1.SetAttribute("JitterMinNs", DoubleValue (500));
		   dnp3MIM1.SetAttribute("JitterMaxNs", DoubleValue (1000));
		   dnp3MIM1.SetAttribute("isMaster", BooleanValue (false));
		   dnp3MIM1.SetAttribute ("Name", StringValue (enamestring));
		   dnp3MIM1.SetAttribute("MasterDeviceAddress", UintegerValue(1));
		   dnp3MIM1.SetAttribute("StationDeviceAddress", UintegerValue(2+x));
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
		   dnpMIMApp.Start (Seconds (start));
		   dnpMIMApp.Stop (simTime);
		   
	  }
    }
    dnpMasterApp.Start (Seconds (start));
    dnpMasterApp.Stop (simTime);
    dnpOutstationApp.Start (Seconds (start));
    dnpOutstationApp.Stop (simTime);

    std::cout << "Setting up Bots" << std::endl;
    int BOT_START = std::stof(configObject["DDoS"][0]["Start"].asString());;
    int BOT_STOP = std::stof(configObject["DDoS"][0]["End"].asString());;
    std::string str_on_time = configObject["DDoS"][0]["TimeOn"].asString();
    std::string str_off_time = configObject["DDoS"][0]["TimeOff"].asString();
    int TCP_SINK_PORT = 9000;
    int UDP_SINK_PORT = master_port; //mimPort[2]-10;
    int MAX_BULK_BYTES = std::stof(configObject["DDoS"][0]["PacketSize"].asString()); //20971520000;
    std::string DDOS_RATE = configObject["DDoS"][0]["Rate"].asString(); //"2000kb/s";


    if (DDoS){
	    ApplicationContainer onOffApp[botNodes.GetN()];
	    for (int k = 0; k < botNodes.GetN(); ++k)
            {
		Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (botNodes.Get(k)->GetObject<Ipv4> ());
		if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
                remoteHostStaticRouting->AddNetworkRouteTo (subNodes.Get(k)->GetObject<Ipv4>()->GetAddress(k+1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), ueNodes.Get(k)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal(),1); //gateway, 1);
		}
		else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
                remoteHostStaticRouting->AddNetworkRouteTo (MIM.Get(k)->GetObject<Ipv4>()->GetAddress(k+1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), ueNodes.Get(k)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal(),1); //gateway, 1);
		}
		else if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                remoteHostStaticRouting->AddNetworkRouteTo (remoteHostAddr, Ipv4Mask ("255.255.0.0"), ueNodes.Get(k)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(),1); //gateway, 1);
                }


		if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
		OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(subNodes.Get(k)->GetObject<Ipv4>()->GetAddress(k+1,0).GetLocal(), UDP_SINK_PORT))); //remoteHostAddr, UDP_SINK_PORT)));
                onoff.SetConstantRate(DataRate(DDOS_RATE));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                onOffApp[k] = onoff.Install(botNodes.Get(k));
		}
		else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
                OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(MIM.Get(k)->GetObject<Ipv4>()->GetAddress(k+1,0).GetLocal(), UDP_SINK_PORT))); //remoteHostAddr, UDP_SINK_PORT)));
                onoff.SetConstantRate(DataRate(DDOS_RATE));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                onOffApp[k] = onoff.Install(botNodes.Get(k));
		}
		else if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(remoteHostAddr, UDP_SINK_PORT))); //remoteHostAddr, UDP_SINK_PORT)));
                onoff.SetConstantRate(DataRate(DDOS_RATE));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                onOffApp[k] = onoff.Install(botNodes.Get(k));
		}
                onOffApp[k].Start(Seconds(BOT_START));
                onOffApp[k].Stop(Seconds(BOT_STOP));

                if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
                        PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                             Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
                        ApplicationContainer UDPSinkApp = UDPsink.Install(subNodes.Get(k)); //remoteHost); 
                        UDPSinkApp.Start(Seconds(0.0));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));
		}
		else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
			PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                             Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
                        ApplicationContainer UDPSinkApp = UDPsink.Install(MIM.Get(k)); //remoteHost); 
                        UDPSinkApp.Start(Seconds(0.0));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));
                
		}
            }
            if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                        PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                             Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
                        ApplicationContainer UDPSinkApp = UDPsink.Install(remoteHost); 
                        UDPSinkApp.Start(Seconds(0.0));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));

            }
    }

    std::cout << "Done Setting up the bots " << std::endl;

    NodeContainer endpointNodes;
    endpointNodes.Add (remoteHostContainer.Get(0));
    for (int i = 0; i < ueNodes.GetN(); i++){
        endpointNodes.Add (ueNodes.Get (i));
    }
    for (int i = 0; i < MIM.GetN(); i++){
        endpointNodes.Add (MIM.Get (i));
    }
    for (int i = 0; i < subNodes.GetN(); i++){
        endpointNodes.Add (subNodes.Get (i));
    }

    flowMonitor = flowHelper.Install(endpointNodes);
    flowMonitor->SetAttribute("DelayBinWidth", DoubleValue(0.5));
    flowMonitor->SetAttribute("JitterBinWidth", DoubleValue(0.5));
    flowMonitor->SetAttribute("PacketSizeBinWidth", DoubleValue(50));
    int mon = std::stoi(configObject["Simulation"][0]["MonitorPerf"].asString());
    Simulator::Schedule (Seconds (0.2), &Throughput);

    if (mon) {
        if (DDoS){
            p2ph.EnablePcapAll (pcapFileDir+"p2p-DDoS", false);
        }else{
            p2ph.EnablePcapAll (pcapFileDir+"p2p", false);
        }
    }

  lteHelper->EnableTraces ();
  //enablePcapAllBaseTime("radics-exercise2-utility1-1day", remoteHostContainer, ncP2P_nodes);

  Simulator::Stop (simTime);
  Simulator::Run ();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy ();
  return 0;
}
