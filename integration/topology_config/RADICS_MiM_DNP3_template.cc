// ****************************************************************************
// Model parameter and instrumentation set-up
// Model contains 11 nodes - CC, Insider, Router, MIM1, MIM2, MIM3, MIM4, SS1, SS2, SS3, SS4
// The CC, Insider, and Router are connected via CSMA while the router is connected via P2P to
// all of the MIM nodes. Each MIM node is connected via P2P to the same numbered SS node.
// DNP3 applications are installed in SS (DNP3 slave) and CC (DNP3 master)nodes
// CC sends 4 seocon poll requests to each SS
// The insider node is for sending directed attacks from within the control center
// SS reponds with later Power readings and Breaker settings received from
// the FNCS player
//*****************************************************************************
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <any>
#include <map>
#include <string>
#include <cstdlib>
#include <cassert>
#include <math.h>
#include <stdio.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/writer.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/error-model.h"
#include "ns3/names.h"
#include "ns3/node-list.h"
#include "ns3/dnp3-application-helper.h"
#include "ns3/dnp3-simulator-impl.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/nstime.h"

#include <filesystem>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Constant 
#define CSMA_HELPER "CsmaHelper"
#define P2P_HELPER "PointToPointHelper"
#define INTERNET_STACK_HELPER "InternetStackHelper"
#define NET_DEVICE "NetDevice"
#define DEVICE_ATTRIBUTES "DeviceAttributes"
#define CHANNEL_ATTRIBUTES "ChannelAttributes"


using namespace std;
using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("RADICS_SCADA");

Time baseDate ("1509418800s"); //Oct 30 11pm East coast time
//Time baseDate("1506830400s");  //Oct 1 12am East coast time
//Time baseDate("1510747200s"); //Nov 15 7am East coast time

static void
PacketCapture (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  file->Write (baseDate + Simulator::Now (), p);
}

void
enablePcapAllBaseTime (std::string dir, std::string prefix, NodeContainer csmas, NodeContainer p2ps)
{
  NS_LOG_INFO ("enablePcapAllBaseTime.");
  std::string filename;
  PcapHelper pcapHelper;
  std::string filepath = prefix.substr(0,prefix.find_last_of("/\\"));
  std::string processedfilename = prefix.substr(prefix.find_last_of("/\\")+1);
  
  mkdir(dir.c_str(), 0777);

  //   NodeContainer n = NodeContainer::GetGlobal ();
  for (NodeContainer::Iterator i = csmas.Begin (); i != csmas.End (); ++i)
    {
      Ptr<Node> node = *i;
      for (uint32_t j = 0; j < node->GetNDevices (); ++j)
        {
          Ptr<NetDevice> dev = node->GetDevice (j);
          Ptr<CsmaNetDevice> device = dev->GetObject<CsmaNetDevice> ();
          filename = pcapHelper.GetFilenameFromDevice (prefix, dev);

          Ptr<PcapFileWrapper> file =
              pcapHelper.CreateFile (dir+"/"+filename, std::ios::out, PcapHelper::DLT_EN10MB);
          dev->TraceConnectWithoutContext ("PromiscSniffer",
                                           MakeBoundCallback (&PacketCapture, file));
          ;
        }
    }

  for (NodeContainer::Iterator i = p2ps.Begin (); i != p2ps.End (); ++i)
    {
      Ptr<Node> node = *i;
      for (uint32_t j = 0; j < node->GetNDevices (); ++j)
        {
          Ptr<NetDevice> dev = node->GetDevice (j);
          Ptr<PointToPointNetDevice> device = dev->GetObject<PointToPointNetDevice> ();
          filename = pcapHelper.GetFilenameFromDevice (prefix, dev);

          Ptr<PcapFileWrapper> file =
              pcapHelper.CreateFile (dir+"/"+filename, std::ios::out, PcapHelper::DLT_PPP);
          dev->TraceConnectWithoutContext ("PromiscSniffer",
                                           MakeBoundCallback (&PacketCapture, file));
          ;
        }
    }
}


void readJsonConfig(std::string fpath, Json::Value& configobj)
{
    std::ifstream tifs(fpath);
    Json::Reader configreader;
    configreader.parse(tifs, configobj);
}

NetDeviceContainer
installHelper(NodeContainer nc, std::any any_helper)
{
    NetDeviceContainer dc;
    if (auto helper = any_cast<CsmaHelper>(&any_helper)) {
        NS_LOG_LOGIC("Installing CsmaHelper");
        dc = helper->Install (nc);
      }
      else if (auto helper = any_cast<PointToPointHelper>(&any_helper)) {
        NS_LOG_LOGIC("Installing PointToPointHelper");
        dc = helper->Install (nc);
      }
      else if (auto helper = any_cast<InternetStackHelper>(&any_helper)) {
        NS_LOG_LOGIC("Installing InternetStackHelper");
        helper->Install (nc);
      }
      else if (auto helper = any_cast<InternetStackHelperMIM>(&any_helper)) {
        NS_LOG_LOGIC("Installing InternetStackHelperMIM");
        helper->Install (nc);
      }
    return dc;
}

int
main (int argc, char *argv[])
{
  cout << "In main" << endl;

  std::string configFileName = "/rd2c/ns-3-dev/scratch/template.json";
  Json::Value root;

//   int includeMIM = 1; //Default is to use MIM
//   int attackSelection = 2; //Default is to have no attack selection
//   int attackStartTime = 10;
//   int attackEndTime = 1000;

  // Allow the user to override any of the defaults and the above
  // DefaultValue::Bind ()s at run-time, via command-line arguments
  CommandLine cmd;

  // To override any of these defaults, add the parameter in the first set of quotes
  //  as a calling argument preceeded by "--" and folowed by "=<value>". For example:
  //      RADICS_withMiMDNP3 --includeMIM=0
  // cmd.AddValue ("includeMIM", "1=Include MIM functionality, 0=don't include MIM functionality.", includeMIM);
  // cmd.AddValue ("attackSelection", "1=Disconnect the routing process, 2=Send packet with payload '0' to the destination", attackSelection);
  // cmd.AddValue ("attackStartTime", "Set start time of the attack in seconds", attackStartTime);
  // cmd.AddValue ("attackEndTime", "Set end time of the attack in seconds", attackEndTime);
  cmd.AddValue("topologyConfig", "NS3 topology configration file path", configFileName);
  cmd.Parse (argc, argv);

  readJsonConfig(configFileName, root);
  // cout << "includeMIM" << includeMIM << endl;
  // cout << "attackSelection" << attackSelection << endl;
  // cout << "attackStartTime" << attackStartTime << endl;
  // cout << "attackEndTime" << attackEndTime << endl;

  LogComponentEnable ("RADICS_SCADA", LOG_LEVEL_ALL);
  LogComponentEnable ("Dnp3Application", LOG_LEVEL_INFO);
  LogComponentEnable ("Dnp3SimulatorImpl", LOG_LEVEL_INFO);
  //    LogComponentEnable("Ipv4L3ProtocolMIM", LOG_LEVEL_INFO);
  //    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
  //    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // *************************************************************************
  // Initialize DNP3 simulator
  //**************************************************************************

  // Dnp3 Fncs component initialization
  Dnp3SimulatorImpl *hb = new Dnp3SimulatorImpl ();
  Ptr<Dnp3SimulatorImpl> hb2 (hb);
  hb->Unref ();
  Simulator::SetImplementation (hb2);
  
  // To maintain access to variables
  std::vector<NodeContainer> nodeContainerList;
  std::map<std::string, Ptr<Node>> nodeMap;
  std::map<std::string, std::any> helperMap;
  std::map<std::string, NetDeviceContainer> netdeviceMap;
  std::map<std::string, Ipv4InterfaceContainer> ipv4Map;
  std::map<std::string, std::any> applicationMap;

  /*
  The order of processing:
  Helpers -> NetDevice -> Node -> IP assign -> Application
                                            -> Routing
  */

  // Helpers
  NS_LOG_INFO("Creating helpers...");
  // CSMA
  if (root.isMember(CSMA_HELPER)) {
    CsmaHelper csmaHelper;
    for (auto &attribute : root[CSMA_HELPER][DEVICE_ATTRIBUTES].getMemberNames()) {
      if (attribute == "Mtu") {
        csmaHelper.SetDeviceAttribute(attribute, UintegerValue(root[CSMA_HELPER][DEVICE_ATTRIBUTES][attribute].asInt()));
      }
    }
    helperMap[root[CSMA_HELPER]["id"].asString()] = csmaHelper;
  }
  // PointToPoint
  if (root.isMember(P2P_HELPER)) {
    PointToPointHelper p2pHelper;
    for (auto &attribute : root[P2P_HELPER][DEVICE_ATTRIBUTES].getMemberNames()) {
      auto value = root[P2P_HELPER][DEVICE_ATTRIBUTES][attribute];
      if (attribute == "Mtu") {
        p2pHelper.SetDeviceAttribute(attribute, UintegerValue(value.asInt()));
      }
      else {
        p2pHelper.SetDeviceAttribute(attribute, StringValue(value.asString()));
      }
    }
    for (auto &attribute : root[P2P_HELPER][CHANNEL_ATTRIBUTES].getMemberNames()) {
      auto value = root[P2P_HELPER][CHANNEL_ATTRIBUTES][attribute];
      if (attribute == "Delay") {
        p2pHelper.SetChannelAttribute(attribute, StringValue(value.asString()));
      }
    }
    helperMap[root[P2P_HELPER]["id"].asString()] = p2pHelper;
  }
  // Internet
  if (root.isMember(INTERNET_STACK_HELPER)) {
    InternetStackHelper internet;
    helperMap[root[INTERNET_STACK_HELPER]["id"].asString()] = internet;
  }
  if (root.isMember("InternetStackHelperMIM")) {
    InternetStackHelperMIM internet;
    helperMap[root["InternetStackHelperMIM"]["id"].asString()] = internet;
  }
  
  

  // Nodes
  NS_LOG_INFO("Creating nodes...");
  for (auto &node : root["Nodes"]) {
    std::string node_id = node["id"].asString();
    NS_LOG_INFO("Node id:" << node_id);
    nodeMap[node_id] = CreateObject<Node> ();

    // register name
    Names::Add (node_id, nodeMap[node_id]);

    // NetDevice & Internet
    if (node.isMember("NetDevices")) {
        NS_LOG_INFO("Installing NetDevices..");
        for (auto &nd : node["NetDevices"]) {
            std::any helper = helperMap[nd["install"].asString()];
            NetDeviceContainer dc = installHelper(nodeMap[node_id], helper);
            netdeviceMap[node_id + "/" +nd["id"].asString()] = dc;
        }
    }
  }

  // Node containers
  NS_LOG_INFO("Creating Node Containers...");
  for (auto &nodeContainer : root["NodeContainers"]) {
    NodeContainer nc = NodeContainer ();
    
    for (auto &node : nodeContainer["Nodes"]) {
      nc.Add(nodeMap[node["id"].asString()]);
    }
    for (auto &install : nodeContainer["install"]) {
      NS_LOG_INFO("Installing NetDevices...");
      std::any helper = helperMap[install.asString()];
      NetDeviceContainer dc = installHelper(nc, helper);
      // split NetDeviceContainer
      assert(dc.GetN()==nodeContainer["Nodes"].size());
      for (uint i=0; i<dc.GetN(); i++){
        NS_LOG_INFO("Installing  NetDevice:" << i);
        Ptr<NetDevice> netdevice_ptr = dc.Get(i);
        NS_LOG_INFO("Installing  NetDevice:" << netdevice_ptr);
        std::string node_id = nodeContainer["Nodes"][i]["id"].asString();
        std::string netdevice_id = nodeContainer["Nodes"][i]["NetDevice"].asString();
        netdeviceMap[node_id + "/" + netdevice_id] = NetDeviceContainer(netdevice_ptr);
        NS_LOG_INFO("Installing  NetDevice:" << node_id + "/" + netdevice_id);
      }
    }
    nodeContainerList.push_back(nc);
  }


  NS_LOG_INFO("Assigning IP...");
  for (auto &node : root["Nodes"]) {
    std::string node_id = node["id"].asString();
    Ptr<Node> node_ptr = nodeMap[node_id];
    NS_LOG_INFO("Node id:" + node_id);
    // IP
    if (node.isMember("Ipv4")) {
        NS_LOG_INFO("Installing Ipv4");
        for (auto &ipv4 : node["Ipv4"]) {
            Ipv4AddressHelper ipv4Helper;
            ipv4Helper.SetBase(ipv4["network"].asCString(), ipv4["mask"].asCString(), ipv4["address"].asCString());
            NetDeviceContainer dc = netdeviceMap[node_id + "/" + ipv4["NetDevice"].asString()];
            Ipv4InterfaceContainer ifc = ipv4Helper.Assign(dc);
            ipv4Map[ipv4["id"].asString()] = ifc;
        }
    }
    /// TODO: Ipv6
  }
  

  // application
  NS_LOG_INFO("Creating applications...");
  for (auto &node : root["Nodes"]) {
    std::string node_id = node["id"].asString();
    Ptr<Node> node_ptr = nodeMap[node_id];
    ostringstream oss;
    oss << node_ptr;
    NS_LOG_INFO("Node id:" + node_id + " " + oss.str());
    // Application
    if (node.isMember("Applications")) {
      NS_LOG_INFO("Installing Applications");
      for (auto &app : node["Applications"]) {
        std::string app_id = app["id"].asString();
        NS_LOG_INFO("App id:" + app_id);
        if (app["app"]=="Dnp3Application") {
          std::string protocol = app["protocol"].asString();
          if (protocol == "udp") {
            protocol="ns3::UdpSocketFactory";
          }
          else if (protocol == "tcp") {
            protocol="ns3::TcpSocketFactory";
          }
          else {
              throw(__LINE__);
          }
          // std::string protocol, Address address
          
          Address address;
          Dnp3ApplicationHelper dnp3App (protocol, address);
          for (auto &attribute : app["Attributes"].getMemberNames()) {
            auto value = app["Attributes"][attribute];

            cout << attribute << ":";

            if (attribute == "LocalPort" || attribute == "RemotePort" || attribute == "IntegrityPollInterval" ||
                attribute == "MasterDeviceAddress" || attribute == "StationDeviceAddress") {
              dnp3App.SetAttribute(attribute, UintegerValue(value.asUInt()));
              cout << value.asUInt() << endl;
            }
            else if (attribute == "isMaster" || attribute == "EnableTCP" || attribute == "mitmFlag") {
              dnp3App.SetAttribute(attribute, BooleanValue(value.asBool()));
              cout << value.asBool() << endl;
            }
            else if (attribute == "JitterMinNs" || attribute == "JitterMaxNs") {
              dnp3App.SetAttribute(attribute, DoubleValue(value.asDouble()));
              cout << value.asDouble() << endl;
            }
            else if (attribute == "RemoteAddress" || attribute == "LocalAddress") {
              Ipv4InterfaceContainer ifc = ipv4Map[value.asString()];
              dnp3App.SetAttribute(attribute, AddressValue(ifc.GetAddress(0)));
              cout << ifc.GetAddress(0) << endl;
            }
            else {
              dnp3App.SetAttribute(attribute, StringValue(value.asString()));
              cout << value.asString() << endl;
            }
          }
          Ptr<Dnp3Application> app_ptr = dnp3App.Install (node_ptr, app_id);
          ApplicationContainer app_container;
          app_container.Add(app_ptr);
          if (app.isMember("Start")) {
              app_container.Start(Seconds(uint(app["Start"].asUInt())));
            // Time start = Seconds(uint(app["Start"].asUInt()));
            // app_ptr->SetStartTime (start);
          }
          if (app.isMember("Stop")) {
              app_container.Stop(Seconds(uint(app["Stop"].asUInt())));
            // Time end = Seconds(uint(app["Stop"].asUInt()));
            // app_ptr->SetStopTime (end);
          }
          applicationMap[app_id] = app_ptr;
        }
      }
    }
  }

  NS_LOG_INFO("Setting up Routing...");
  // Routing
  for (auto &routing : root["Routings"]) {
    std::string node_id = routing["node"].asString();
    NS_LOG_INFO("Node:" + node_id + " " + routing["type"].asString());
    Ptr<Node> node_ptr = nodeMap[node_id];
    if (routing["type"].asString()=="Ipv4StaticRouting"){
      Ptr<Ipv4> ipv4 = node_ptr->GetObject<Ipv4> ();


      for (uint32_t i=0; i<ipv4->GetNInterfaces (); i++) {
        Ipv4InterfaceAddress address = ipv4->GetAddress(i, 0);
        Ptr<NetDevice> nd_ptr = ipv4->GetNetDevice(i);
        cout << "Ipv4, Interface:" << i << ", address:" << address << ", device:" << nd_ptr << endl;
      }
 
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> routing_ptr = ipv4RoutingHelper.GetStaticRouting(ipv4);
      for (auto &hostRoute : routing["HostRouteTo"]) {
        
        Ipv4Address dest = hostRoute["dest"].asCString();
        uint32_t interface = hostRoute["interface"].asUInt();
        uint32_t metric = 0;
        if (hostRoute.isMember("metric")) {
          metric = hostRoute["metric"].asUInt();
        }
        routing_ptr->AddHostRouteTo(dest,interface, metric);
        ostringstream oss;
        oss << dest << "," << interface << "," << metric;

        NS_LOG_INFO("AddHostRouteTo(" + oss.str() + ")");
      }
      for (auto &networkRoute : routing["NetworkTo"]) {
        Ipv4Address network = networkRoute["network"].asCString();
        Ipv4Mask mask = networkRoute["mask"].asCString();
        uint32_t interface = networkRoute["interface"].asUInt();
        uint32_t metric = 0;
        if (networkRoute.isMember("metric")) {
          metric = networkRoute["metric"].asUInt();
        }
        if (networkRoute.isMember("nextHop")) {
          Ipv4Address nextHop = networkRoute["nextHop"].asCString();
          routing_ptr->AddNetworkRouteTo(network,mask,nextHop,interface, metric);
          ostringstream oss;
          oss << network << "," << mask << "," << nextHop << "," << interface << "," << metric;
          NS_LOG_INFO("AddNetworkRouteTo(" + oss.str() + ")");
        }
        else {
          routing_ptr->AddNetworkRouteTo(network,mask,interface, metric);
          ostringstream oss;
          oss << network << "," << mask << "," << interface << "," << metric;
          NS_LOG_INFO("AddNetworkRouteTo(" + oss.str() + ")");
        }
        
      }
    }
  }

  NS_LOG_INFO("Setting up Schedules...");
  // Scheduling events
  for (auto &schedule : root["Schedules"]) {
    int delay = schedule["delay_ms"].asInt();
    if (schedule["function"]=="periodic_poll") {
      Ptr<Dnp3Application> app = any_cast<Ptr<Dnp3Application>>(applicationMap[schedule["app_id"].asString()]);
      Simulator::Schedule(MilliSeconds(delay), &Dnp3Application::periodic_poll, app, 0);
      NS_LOG_INFO("periodic_poll of " + schedule["app_id"].asString());
    }
    /// TODO: Add other operations
    
  }

  for (auto ele : ipv4Map) {
      cout << ele.first << " -> ";
      cout << ele.second.GetAddress(0) << endl;
  }

  for (auto ele : applicationMap) {
      cout << ele.first << " -> ";
      auto app = any_cast<Ptr<Dnp3Application>>(ele.second);
    //   app->GetTypeId();
    AddressValue ptr;
    app->GetAttribute ("LocalAddress", ptr);
    cout << " LocalAddress:" << ptr.Get();
    app->GetAttribute ("RemoteAddress", ptr);
    // ptr
      cout << ", RemoteAddress:" << ptr.Get() << endl;
  }
  
  NodeContainer csmaNodeContainer, p2pNodeContainer;

  for  (auto c : nodeContainerList) {
      for (uint32_t k=0; k<c.GetN(); k++) {
          auto n = c.Get(k);
          for (uint32_t i=0; i<n->GetNDevices(); i++) {
              if (n->GetDevice(i)->IsPointToPoint()) {
                  p2pNodeContainer.Add(n);
              }
              else {
                  csmaNodeContainer.Add(n);
              }
          }
      }
  }
  
  enablePcapAllBaseTime(root["output_dir"].asString(), root["pcap_prefix"].asString(), csmaNodeContainer, p2pNodeContainer);

  if (!root.isMember("Routings")){
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  }

  NS_LOG_INFO("Running simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;

}
