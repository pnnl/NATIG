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
// #include "ns3/dnp3-helics-helper.h"
#include "ns3/dnp3-simulator-impl.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/flow-monitor-module.h"

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

/*
 * The main() loop below represents the ns-3 model. The helics ns-3
 * integration will filter messages sent by MessageFederate instances by
 * creating HelicsApplication instances at Nodes. The name given to the
 * HelicsApplication should match a registered endpoint.
 */

void updatePower(){
    //Use this function to pass in the nodes/nodecontainer/any pointers to sections of the network that would allow you to update the network after your RL agent predicts the new states. 
    NS_LOG_UNCOND("Update/Monitor feature values over time");
    Simulator::Schedule (Seconds (2), &updatePower); //Call the function every 2 seconds
}

int
main (int argc, char *argv[])
{
  bool verbose = true;
  std::string configFileName;
  std::string helicsConfigFileName;
  std::string topologyConfigFileName;
  std::string pointFileDir = "./";
  std::string pcapFileDir = "./";
  uint32_t i;
  Time simTime = Seconds(180);
  int attackSelection = 2; //std::stoi(parameters["attackSel"]); //Default is to have no attack selection
  int attackStartTime = 120; //std::stoi(parameters["start"]);
  int attackEndTime   = 180; //std::stoi(parameters["end"]);
  CommandLine cmd;
  Json::Value configObject;
  Json::Value helicsConfigObject;
  Json::Value topologyConfigObject;

  // -------------------------------------------------------------------------------
  // HELICS FILTER config file calling
  // -------------------------------------------------------------------------------

  std::cout << "number of args: " << argc << std::endl;
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("helicsConfig", "Helics configuration file path", helicsConfigFileName);
  cmd.AddValue("microGridConfig", "NS3 MicroGrid configration file path", configFileName);
  cmd.AddValue("topologyConfig", "NS3 topology configuration file path", topologyConfigFileName);
  cmd.AddValue("pointFileDir", "Points file path", pointFileDir);
  cmd.AddValue("pcapFileDir", "PCAP output file path", pcapFileDir);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable ("IntagrationExample", LOG_LEVEL_INFO);
    // LogComponentEnable ("HelicsSimulatorImpl", LOG_LEVEL_LOGIC);
    LogComponentEnable ("Dnp3ApplicationNew", LOG_LEVEL_INFO);
    // LogComponentEnable ("HelicsApplication", LOG_LEVEL_LOGIC);
    LogComponentEnable ("Names", LOG_LEVEL_LOGIC);
  }

  std::cout << "Helics configuration file: " << helicsConfigFileName.c_str() << std::endl;
  std::cout << "MicroGrid configuration file: " << configFileName.c_str() << std::endl;

  readMicroGridConfig(configFileName, configObject);
  readMicroGridConfig(helicsConfigFileName, helicsConfigObject);
  readMicroGridConfig(topologyConfigFileName, topologyConfigObject);

  HelicsHelper helicsHelper;
  std::cout << "Calling Calling Message Federate Constructor" << std::endl;
  helicsHelper.SetupApplicationFederate();

  std::string fedName = helics_federate->getName();
  
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  InternetStackHelper internetStack;
  InternetStackHelperMIM internetStackMIM;
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0", "0.0.0.1");

  simTime = Seconds(std::stof(configObject["Simulation"][0]["SimTime"].asString()));
  float start = std::stof(configObject["Simulation"][0]["StartTime"].asString());
  int includeMIM = std::stoi(configObject["Simulation"][0]["includeMIM"].asString());

  NodeContainer Microgrid;
  Microgrid.Create(configObject["microgrid"].size());
  internetStack.Install(Microgrid);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  NodeContainer MIMNode;
  NodeContainer hubNode;

  bool ring = false; //true;
  bool star_bool = true; //false;

  //Ring topology start
  if (ring){
      //generating the nodes following the number of nodes that was mentioned in the configuration file
      NodeContainer nodes;
      nodes.Create(topologyConfigObject["Node"].size());
      std::cout << "Creating the nodes " << topologyConfigObject["Node"].size() << " vs " << configObject["MIM"].size() << std::endl;
      //Dividing the nodes depending on whether they will serve as control center or man in the middle
      for (int h = 0; h < configObject["MIM"].size()-1; h++){
          MIMNode.Add(nodes.Get(h));
      }
      hubNode.Add(nodes.Get(configObject["MIM"].size()-1));
      std::cout << "MIM nodes have been added" << std::endl;

      //Attaching the nodes to the InternetStacks
      internetStackMIM.Install(MIMNode);
      internetStack.Install(hubNode);
      std::cout << "Added the hub Node" << std::endl;

      //Connecting the nodes as a ring
      std::vector<NetDeviceContainer> NetRing;
      for (int node = 0; node < topologyConfigObject["Node"].size(); node++){
         std::cout << "Node " << topologyConfigObject["Node"][node]["name"] << " is connected to the following nodes:" << std::endl;
	 //int con = 0;
         for(int con = 0; con < topologyConfigObject["Node"][node]["connections"].size(); con++){
             std::cout << "Connection " << con << " " << topologyConfigObject["Node"][node]["connections"][con] << std::endl;	     
	     NetDeviceContainer NetDev = p2p.Install(NodeContainer(nodes.Get(std::stoi(topologyConfigObject["Node"][node]["name"].asString())), nodes.Get(std::stoi(topologyConfigObject["Node"][node]["connections"][con].asString()))));
             NetRing.push_back(NetDev);
	 }
      }
      //assigning the address to the nodes
      Ipv4AddressHelper ipv4Sub;
      std::string address = "10.1.1.0";
      ipv4Sub.SetBase(address.c_str(), "255.255.255.0", "0.0.0.1");
      for (int h = 0; h < NetRing.size(); h++){
	    Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign(NetRing[h].Get(0));
            Ipv4InterfaceContainer interfacesSubCC = ipv4Sub.Assign(NetRing[h].Get(1));
	    ipv4Sub.NewNetwork ();
	    std::cout << "DONE" << std::endl; 
      }

      for (int node = 0; node < topologyConfigObject["Node"].size()-1; node++){
        std::cout << "Node " << topologyConfigObject["Node"][node]["name"] << " is connected to the following nodes:" << std::endl;
	//int con = 0;
        for(int con = 0; con < topologyConfigObject["Node"][node]["connections"].size(); con++){
            std::cout << "Connection " << con << " " << topologyConfigObject["Node"][node]["connections"][con] << std::endl;
	    if (std::stoi(topologyConfigObject["Node"][node]["connections"][con].asString()) < configObject["MIM"].size()-1){
            Ptr<Ipv4StaticRouting> interRouting2 = ipv4RoutingHelper.GetStaticRouting (MIMNode.Get(std::stoi(topologyConfigObject["Node"][node]["name"].asString()))->GetObject<Ipv4>());
	    std::cout << "Adding the Network route" << std::endl;
	    std::cout << hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << std::endl; 
	    interRouting2->AddNetworkRouteTo (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.255.0"), MIMNode.Get(std::stoi(topologyConfigObject["Node"][node]["connections"][con].asString()))->GetObject<Ipv4>()->GetAddress(2,0).GetLocal(), 1);
	    }else{
		Ptr<Ipv4StaticRouting> interRouting2 = ipv4RoutingHelper.GetStaticRouting (MIMNode.Get(std::stoi(topologyConfigObject["Node"][node]["name"].asString()))->GetObject<Ipv4>());
                interRouting2->AddHostRouteTo (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal(), 1, 1);
	    }
         }
      }
  }else{
      //star topology start
      PointToPointStarHelper star (configObject["MIM"].size(), p2p);  
      NS_LOG_INFO ("Install internet stack on all nodes.");
      star.InstallStack (internetStackMIM, internetStack);
      NS_LOG_INFO ("Assign IP Addresses.");
      star.AssignIpv4Addresses (ipv4);

      uint32_t numSpoke = star.SpokeCount();
      for (int y = 0; y < numSpoke; y++){
          MIMNode.Add(star.GetSpokeNode(y));
      }
      hubNode.Add(star.GetHub());
  }
  Ptr<Ipv4StaticRouting> ControlRouting = ipv4RoutingHelper.GetStaticRouting(hubNode.Get(0)->GetObject<Ipv4>()); //star.GetHub()->GetObject<Ipv4>());
  for (i = 0; i < configObject["microgrid"].size(); i++){
	  auto ep_name = configObject["microgrid"][i]["name"].asString();
	  std::cout << "Microgrid network node: " << ep_name << " " << configObject["microgrid"].size() << std::endl;
	  Ptr<Node> tempNode = Microgrid.Get(i);
	  //Names::Add(ep_name, tempNode);
	  NetDeviceContainer NetDev = p2p.Install(NodeContainer(tempNode,MIMNode.Get(i))); //star.GetSpokeNode(i)));
	  Ipv4AddressHelper ipv4Sub;
	  std::string address = "11."+std::to_string(i+2)+".0.0";
	  ipv4Sub.SetBase(address.c_str(), "255.255.255.0", "0.0.0.1");
	  Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign(NetDev);
	  //Adding the routes
	  if (ring){
            std::cout << "Second route" << std::endl;
            Ptr<Ipv4StaticRouting> MicrogridRouting = ipv4RoutingHelper.GetStaticRouting (Microgrid.Get(i)->GetObject<Ipv4>());
	    MicrogridRouting->AddNetworkRouteTo (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.255.0"), MIMNode.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1);
	    //I think we may need to add a pointer to each microgrid from each MIM node.
	    for (int j = 0; j < configObject["MIM"].size(); j++){
                Ptr<Ipv4StaticRouting> interRouting2 = ipv4RoutingHelper.GetStaticRouting (MIMNode.Get(j)->GetObject<Ipv4>());
		if (j != i){
                    interRouting2->AddNetworkRouteTo (tempNode->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.255.0"), MIMNode.Get(i+1)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal(), 1);
		}else{
                    interRouting2->AddHostRouteTo (tempNode->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1, 1);
		}
	    }
	    std::cout << "Microgrid Added" << std::endl;
	    ControlRouting->AddNetworkRouteTo(Microgrid.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.255.0"), MIMNode.Get(0)->GetObject<Ipv4>()->GetAddress(2,0).GetLocal(), 1);

	    //Ptr<Ipv4StaticRouting> interRouting2 = ipv4RoutingHelper.GetStaticRouting (MIMNode.Get(i)->GetObject<Ipv4>());
            //interRouting2->AddHostRouteTo (tempNode->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1,2);
	    //ControlRouting->AddNetworkRouteTo(Microgrid.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.255.0"), MIMNode.Get(-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), i+1); 
	  }else{
	    std::cout << "Second route" << std::endl;
	    Ptr<Ipv4StaticRouting> MicrogridRouting = ipv4RoutingHelper.GetStaticRouting (Microgrid.Get(i)->GetObject<Ipv4>());
	    MicrogridRouting->AddNetworkRouteTo (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(i+1,0).GetLocal(), Ipv4Mask("255.255.255.0"), MIMNode.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1); //star.GetSpokeIpv4Address(i), 1); star.GetHubIpv4Address(i)
	    std::cout << "Microgrid Added" << std::endl;
            ControlRouting->AddNetworkRouteTo(Microgrid.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.255.0"), MIMNode.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), i+1);//star.GetSpokeIpv4Address(i), i+1);
          }
  }
  //Ptr<Node> hubNode = star.GetHub();
  //reading the attack configuration
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

  //Getting a list MIM nodes
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

  uint16_t port = 20000;
  uint16_t master_port = 40000;
  ApplicationContainer dnpOutstationApp, dnpMasterApp;
  //Ptr<Node> hubNode = star.GetHub ();
  std::vector<uint16_t> mimPort;
  //changing the parameters of the nodes in the network
  Simulator::Schedule (Seconds (3.2), &updatePower); 

  for (i = 0;i < configObject["microgrid"].size();i++)
  {
    mimPort.push_back(master_port);
    auto ep_name = configObject["microgrid"][i]["name"].asString();
    std::cout << "Microgrid network node: " << ep_name << std::endl;
    Ptr<Node> tempnode1 = Microgrid.Get(i); //star.GetSpokeNode (i);
    //std::cout << "hub:" << star.GetHubIpv4Address(i) << ", spike:" << star.GetSpokeIpv4Address(i) << std::endl;

    auto cc_name = configObject["controlCenter"]["name"].asString();
    std::cout << "Control Center network node: " << cc_name << std::endl;
    
    int ID = i+1;
    if (ring){
       ID = 1;
    }

    Dnp3ApplicationHelperNew dnp3Master ("ns3::UdpSocketFactory", InetSocketAddress (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(ID,0).GetLocal(), master_port));  //star.GetHubIpv4Address(i), master_port));

    dnp3Master.SetAttribute("LocalPort", UintegerValue(master_port));
    dnp3Master.SetAttribute("RemoteAddress", AddressValue(tempnode1->GetObject<Ipv4>()->GetAddress(1,0).GetLocal()));//star.GetSpokeIpv4Address (i)));
    dnp3Master.SetAttribute("RemotePort", UintegerValue(port));
    dnp3Master.SetAttribute("JitterMinNs", DoubleValue (10));
    dnp3Master.SetAttribute("JitterMaxNs", DoubleValue (100));
    dnp3Master.SetAttribute("isMaster", BooleanValue (true));
    dnp3Master.SetAttribute("Name", StringValue (cc_name+ep_name));
    dnp3Master.SetAttribute("PointsFilename", StringValue (pointFileDir+"/points_"+ep_name+".csv"));
    dnp3Master.SetAttribute("MasterDeviceAddress", UintegerValue(1));
    dnp3Master.SetAttribute("StationDeviceAddress", UintegerValue(i+2));
    dnp3Master.SetAttribute("IntegrityPollInterval", UintegerValue(10));
    dnp3Master.SetAttribute("EnableTCP", BooleanValue (false));
  
    Ptr<Dnp3ApplicationNew> master = dnp3Master.Install (hubNode.Get(0), std::string(cc_name+ep_name));
    dnpMasterApp.Add(master);

    Dnp3ApplicationHelperNew dnp3Outstation ("ns3::UdpSocketFactory", InetSocketAddress (tempnode1->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port)); //star.GetSpokeIpv4Address (i), port));
    dnp3Outstation.SetAttribute("LocalPort", UintegerValue(port));
    dnp3Outstation.SetAttribute("RemoteAddress", AddressValue(hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(ID,0).GetLocal())); //star.GetHubIpv4Address(i)));
    dnp3Outstation.SetAttribute("RemotePort", UintegerValue(master_port));
    dnp3Outstation.SetAttribute("isMaster", BooleanValue (false));
    dnp3Outstation.SetAttribute("Name", StringValue (ep_name));
    dnp3Outstation.SetAttribute("PointsFilename", StringValue (pointFileDir+"/points_"+ep_name+".csv"));
    dnp3Outstation.SetAttribute("MasterDeviceAddress", UintegerValue(1));
    dnp3Outstation.SetAttribute("StationDeviceAddress", UintegerValue(i+2));
    dnp3Outstation.SetAttribute("EnableTCP", BooleanValue (false));

    Ptr<Dnp3ApplicationNew> slave = dnp3Outstation.Install (tempnode1, std::string(ep_name));
    dnpOutstationApp.Add(slave);
    Simulator::Schedule(MilliSeconds(1005), &Dnp3ApplicationNew::periodic_poll, master, 0);
    // Simulator::Schedule(MilliSeconds(2005), &Dnp3HelicsApplication::send_control_binary, master,
    //   Dnp3HelicsApplication::DIRECT, 0, ControlOutputRelayBlock::CLOSE);
      // Dnp3HelicsApplication::DIRECT, 0, ControlOutputRelayBlock::TRIP);
    Simulator::Schedule(MilliSeconds(3005), &Dnp3ApplicationNew::send_control_analog, master, 
      Dnp3ApplicationNew::DIRECT, 0, -16);
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

  //Adding the MIM node
  //Adding the Dnp3 application to man in the middle attack
  //int MIM_ID = 0;
  if (includeMIM == 1){
    for (int x = 0; x < val.size(); x++){ //std::stoi(configObject["MIM"][0]["NumberAttackers"].asString()); x++){
      int MIM_ID = std::stoi(val[x]) + 1; //x+1;
      auto ep_name = configObject["MIM"][MIM_ID]["name"].asString();
      Ptr<Node> tempnode = MIMNode.Get(MIM_ID-1); //star.GetSpokeNode (MIM_ID-1);
      Names::Add(ep_name, tempnode);
      std::string enamestring = ep_name;
      Ptr<Ipv4> ip = Names::Find<Node>(enamestring)->GetObject<Ipv4>();
      int ID = MIM_ID;
      if (ring){
          ID = 1;
      }


      ip->GetObject<Ipv4L3ProtocolMIM> ()->victimAddr = hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(ID,0).GetLocal(); //star.GetHubIpv4Address(MIM_ID-1);


      //scenario 1, 2 and 3
      Dnp3ApplicationHelperNew dnp3MIM1 ("ns3::UdpSocketFactory", InetSocketAddress (MIMNode.Get(MIM_ID-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port)); //star.GetSpokeIpv4Address(MIM_ID-1),port)); 
      dnp3MIM1.SetAttribute("LocalPort", UintegerValue(port));
      dnp3MIM1.SetAttribute("RemoteAddress", AddressValue(hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(ID, 0).GetLocal())); //star.GetHubIpv4Address(MIM_ID-1)));
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
      dnpMIMApp.Start (Seconds (start));
      dnpMIMApp.Stop (simTime);

    }
  }



  dnpMasterApp.Start (Seconds (start));
  dnpMasterApp.Stop (simTime);
  dnpOutstationApp.Start (Seconds (start));
  dnpOutstationApp.Stop (simTime);

  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  if(!dirExists(pcapFileDir.c_str())) {
    mkdir(pcapFileDir.c_str(), 0777);
  }
  p2p.EnablePcapAll(pcapFileDir+"star", false);

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop (simTime);
  Simulator::Run ();

  flowMonitor->SerializeToXmlFile("stats.xml", true, true);

  Simulator::Destroy ();
  return 0;
}
