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

#include "ns3/helics-helper.h"

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/writer.h>


#include <iostream>
#include <fstream>
#include <string> 
#include <chrono>
#include <thread>
#include <vector>

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
  uint32_t i;
  Time simTime = Seconds(8.001);
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
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable ("IntagrationExample", LOG_LEVEL_INFO);
    // LogComponentEnable ("HelicsSimulatorImpl", LOG_LEVEL_LOGIC);
    LogComponentEnable ("HelicsFilterApplication", LOG_LEVEL_LOGIC);
    LogComponentEnable ("HelicsApplication", LOG_LEVEL_LOGIC);
    LogComponentEnable ("Names", LOG_LEVEL_LOGIC);
  }

  std::cout << "Helics configuration file: " << helicsConfigFileName.c_str() << std::endl;
  std::cout << "MicroGrid configuration file: " << configFileName.c_str() << std::endl;

  readMicroGridConfig(configFileName, configObject);
  readMicroGridConfig(helicsConfigFileName, helicsConfigObject);

  HelicsHelper helicsHelper;
  std::cout << "Calling Calling Message Federate Constructor" << std::endl;
  helicsHelper.SetupApplicationFederateWithConfig(helicsConfigFileName);
  std::cout << "Getting Federate information" << std::endl;

  std::string fedName = helics_federate->getName();
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
  
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  PointToPointStarHelper star (ep_count-1, pointToPoint);
  
  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  star.InstallStack (internet);
 
  NS_LOG_INFO ("Assign IP Addresses.");
  star.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"));

  for (i = 0;i < configObject["microgrid"].size();i++)
  {
    auto ep_name = configObject["microgrid"][i]["name"].asString();
    std::cout << "Microgrid network node: " << ep_name << std::endl;
    Ptr<Node> tempnode = star.GetSpokeNode (i);
    Names::Add(ep_name, tempnode);
  }

  {
    auto ep_name = configObject["controlCenter"]["name"].asString();
    std::cout << "Control Center network node: " << ep_name << std::endl;
    Ptr<Node> tempnode = star.GetHub ();
    Names::Add(ep_name, tempnode);
  }

  // HELICS
  std::string enamestring;
  size_t tpos = 0;
  std::string token;
  NS_LOG_INFO("Number of filters " << helicsConfigObject["filters"].size());
  NS_LOG_INFO("Number of endpoints " << helicsConfigObject["endpoints"].size());
  std::vector<ApplicationContainer> helicsFilterApps;
  for (i = 0;i < helicsConfigObject["endpoints"].size();i++)
  {
    tpos = 0;
    enamestring = helicsConfigObject["endpoints"][i]["name"].asString();
    while ((tpos = enamestring.find("/")) != std::string::npos)
    {
      token = enamestring.substr(0, tpos);
      enamestring.erase(0, tpos + 1);
    }
    NS_LOG_INFO("<<< enamestring : " << enamestring << 
      ", filter " << helics_federate->getFilter(i).getName() << 
      ", endpoint " << helics_federate->getEndpoint(i).getName());
    ApplicationContainer apps = helicsHelper.InstallFilter(
      Names::Find<Node>(enamestring), helics_federate->getFilter(i), helics_federate->getEndpoint(i));
    apps.Start(Seconds(0.0));
    apps.Stop(simTime);
    helicsFilterApps.push_back(apps);
  }
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  pointToPoint.EnablePcapAll("star");

  Simulator::Stop (simTime);
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
