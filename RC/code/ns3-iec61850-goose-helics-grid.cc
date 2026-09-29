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
 * Adapted for GOOSE (IEC 61850-8-1) support, derived from
 * ns3-iec61850-helics-grid.cc (per-protocol convention: build each new
 * protocol's topology file from the most recently added protocol's,
 * not the original DNP3/UDP file, since portability/correctness fixes
 * accumulate) by:
 *   Author: Joon-Seok Kim <joonseok.kim@pnnl.gov>
 *   Author: Oceane Bel
 *
 * Unlike DNP3/Modbus/MMS's client-server install sites, GOOSE
 * fundamentally needs a shared, multicast-capable segment rather than
 * a point-to-point link -- see goose-application-new.h's design note.
 * One real topology change was necessary as a result: the per-microgrid
 * link between the substation node (Microgrid.Get(i)) and its
 * insider/MIM node (MIMNode.Get(i)) is CSMA here instead of the
 * PointToPointHelper link every other protocol's copy of this file
 * uses for that same pair -- a point-to-point link can only ever carry
 * traffic between exactly those two nodes and doesn't model a shared
 * broadcast medium, so it can't carry real multicast the way GOOSE
 * requires. This is a change local to this file only; the other
 * protocols' own copies of this ~1000-line shared structure are
 * untouched.
 *
 * The substation node (Microgrid.Get(i)) is the legitimate GOOSE
 * publisher; its insider/MIM node (MIMNode.Get(i)) is always installed
 * as the legitimate subscriber on that same segment, and -- for
 * whichever microgrids the attack config actually targets -- has its
 * mitmFlag/attack attributes set directly on the already-installed
 * application object (no second Install() call, since GOOSE has no
 * handle_MIM-style request-side dispatch to install separately; a
 * single instance can be a normal subscriber and, when configured, a
 * rogue publisher at the same time -- see goose-application-new.h).
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#include "ns3/core-module.h"
#include "ns3/config-store-module.h"
#include "ns3/network-module.h"

#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"

#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-epc-helper.h"

#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

#include "ns3/csma-module.h"
#include "ns3/helics-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/antenna-module.h"
#include "ns3/flow-monitor-module.h"

#include "ns3/eps-bearer-tag.h"

#include "ns3/goose-application-helper-new.h"
#include "ns3/goose-application-new.h"


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include <ns3/buildings-helper.h>
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include <ns3/antenna-module.h>

// jsoncpp's include path differs between build environments (see
// goose-application-new.h's identical guard, and the fatal error this
// hardcoded <json/json.h> produced when compiling in the Docker
// container, which only provides <jsoncpp/json/json.h>). Using the same
// __has_include fallback here instead of hardcoding one path variant.
#if defined(__has_include)
  #if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
    #include <jsoncpp/json/forwards.h>
    #include <jsoncpp/json/writer.h>
  #else
    #include <json/json.h>
    #include <json/forwards.h>
    #include <json/writer.h>
  #endif
#else
  #include <json/json.h>
  #include <json/forwards.h>
  #include <json/writer.h>
#endif
#include "ns3/olsr-helper.h"
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
#include <bits/stdc++.h>
#include <ctime>

// Network Topology
//
//        n2 n3 n4              .
//         \ | /                .
//          \|/                 .
//     n1--- n0---n5            .
//          /|\                 .
//         / | \                .
//        n8 n7 n6              .


// BUG FIX: this was missing from the file entirely -- the ~1000+ lines
// of reused, protocol-agnostic code below (Throughput(), updateUETable(),
// setRoutingTable(), changeRoute(), attack config parsing) uses
// unqualified std:: names (string, map, pair, endl) throughout, which
// only compiles with this in scope. Most likely dropped when this file
// was assembled from the DNP3 original, since none of that reused code
// was written with explicit std:: qualification.
using namespace std;
using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("IntagrationExample");
Time baseDate("1509418800s");

Ptr<FlowMonitor> flowMonitor;
FlowMonitorHelper flowHelper;
std::map<FlowId, double> tp_transmitted;
std::map<int, int> previous;
std::map<int, std::map<int, std::vector<int>>> route_perf;
std::map<int, std::string> monitor;
std::map<int, int> interface;
std::map<int, float> epsilon;
float epsilon_min = 0.01;
float epsilon_decay = 0.5;
float alpha = 0.3;
int period_routing = 150;
void readMicroGridConfig(std::string fpath, Json::Value& configobj)
{
    std::ifstream tifs(fpath);
    // BUG FIX: check whether the file actually opened BEFORE parsing --
    // checking tifs.good() AFTER Json::Reader::parse() is wrong, since
    // parse() reads the stream through to EOF even on full success,
    // which naturally clears goodbit regardless of outcome. That's a
    // real mistake caught immediately by the very next test run (it
    // fired a false FATAL on a file that parsed correctly). The actual
    // signal to check post-parse is parseOk alone.
    if (!tifs.is_open ())
      {
        std::cerr << "GooseApplication: FATAL: could not open config file '"
                   << fpath << "'" << std::endl;
        exit (1);
      }
    Json::Reader configreader;
    bool parseOk = configreader.parse(tifs, configobj);
    // BUG FIX: silently proceeding on a failed parse used to leave
    // configobj empty, which later surfaced as a confusing stoi("")
    // crash far downstream instead of a clear error at the actual point
    // of failure. Fail loudly here instead.
    if (!parseOk)
      {
        std::cerr << "GooseApplication: FATAL: failed to parse config file '"
                   << fpath << "': " << configreader.getFormattedErrorMessages () << std::endl;
        exit (1);
      }
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

void Throughput (){
        Ptr<Ipv4FlowClassifier> classifier=DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

        //std::stringstream netStatsOut;
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
                                // Kept from the original: an unrecognized transport
                                // protocol on a flow is unexpected enough to warrant
                                // stopping rather than silently mislabeling it. Added
                                // a message so this doesn't die silently if it ever fires.
                                std::cerr << "GooseApplication: FATAL: unexpected transport protocol "
                                           << (int)t.protocol << " on flow " << flow->first << std::endl;
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
		std::stringstream netStatsOut;
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
		std::string delimiter = ".";
                std::ostringstream ss;
                ss << t.sourceAddress;//ipHeader.GetSource();
                std::vector<std::string> ip;
                std::string ip_ = ss.str();
                size_t pos = 0;
                while ((pos = ip_.find(delimiter)) != std::string::npos) {
                   ip.push_back(ip_.substr(0, pos));
                   ip_.erase(0, pos + delimiter.length());
                }

                std::stringstream ss1;
                ss1 << ip[0];
                int ID;
                ss1 >> ID;
                string temp = ss1.str().substr(ss1.str().length() - 1, 1);
                std::stringstream ss2;
                ss2 << temp;
                int ID2;
                ss2 >> ID2;

		//getting the equation for the reward that the user wants to use
		std::vector<std::string> features;
		//timestamp
		std::stringstream timestamp;
		timestamp << Simulator::Now ().GetSeconds ();
		features.push_back(timestamp.str());
		//Port
		std::stringstream port;
                port << t.sourcePort;
                features.push_back(port.str());
                //Path ID
                std::stringstream pathID;
                pathID << " (" << proto << " " << t.sourceAddress << " / " << t.sourcePort << " --> " << t.destinationAddress << " / " << t.destinationPort << ") ";
		features.push_back(pathID.str());
		//Throughput
		std::stringstream tp;
		tp << ((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024;
		features.push_back(tp.str());
		//NS3 lost packet
		std::stringstream lost;
		lost << flow->second.lostPackets;
		features.push_back(lost.str());
		//received bytes
		std::stringstream RXbytes;
		RXbytes << (double)flow->second.rxBytes;
		features.push_back(RXbytes.str());
		//transmitted bytes
                std::stringstream TXbytes;
                TXbytes << (double)flow->second.txBytes;
                features.push_back(TXbytes.str());
		//lost packet ratio
                std::stringstream lostRatio;
		lostRatio << ((double)flow->second.txPackets-(double)flow->second.rxPackets)/(double)flow->second.txPackets;
		features.push_back(lostRatio.str());
		//delay per packet
		std::stringstream PacketDelay;
		PacketDelay << (flow->second.delaySum.GetSeconds()/flow->second.rxPackets);
		features.push_back(PacketDelay.str());
		//received packets
                std::stringstream RXpackets;
                RXpackets << (double)flow->second.rxPackets;
                features.push_back(RXpackets.str());
                //transmitted packets
                std::stringstream TXPackets;
                TXPackets << (double)flow->second.txPackets;
                features.push_back(TXPackets.str());
                //jitter per packets
		std::stringstream PacketJitter;
		PacketJitter << (flow->second.jitterSum.GetSeconds()/(flow->second.rxPackets));
		features.push_back(PacketJitter.str());



                if (ID > 10){ 
                    std::cout << "I am HERE -------------------------- " << ss1.str() << std::endl;
                    std::cout << "Choosing interface " << std::to_string(interface[ID2]) << "for" << std::to_string(ID) << std::endl;
                    if (route_perf.find(ID2) == route_perf.end()){
                          std::map<int, std::vector<int>> xxx;
                          route_perf[ID2] = xxx;
                    }
		    route_perf[ID2][interface[ID2]].push_back(std::stof(features[3]));
		}


		if (((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024 > 0 )
		{
			double rx = (double)flow->second.rxBytes;
			if (ID == 10){
	 		    netStatsOut << Simulator::Now ().GetSeconds () << " " <<  t.sourcePort << " (" << proto << " " << t.sourceAddress << " / " << t.sourcePort << " --> " << t.destinationAddress << " / " << t.destinationPort << ") " << ((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024 << " " << flow->second.lostPackets << " " <<  (double)flow->second.rxBytes << " " << flow->second.txBytes << " " << ((double)flow->second.txPackets-(double)flow->second.rxPackets)/(double)flow->second.txPackets << " " << (flow->second.delaySum.GetSeconds()/flow->second.rxPackets) << " " << flow->second.txPackets << " " << flow->second.rxPackets << " " << (flow->second.jitterSum.GetSeconds()/(flow->second.rxPackets))  << endl;
                            monitor[t.sourcePort] = netStatsOut.str();
			}else{
			    netStatsOut << Simulator::Now ().GetSeconds () << " " <<  t.destinationPort << " (" << proto << " " << t.sourceAddress << " / " << t.sourcePort << " --> " << t.destinationAddress << " / " << t.destinationPort << ") " << ((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024 << " " << flow->second.lostPackets << " " <<  (double)flow->second.rxBytes << " " << flow->second.txBytes << " " << ((double)flow->second.txPackets-(double)flow->second.rxPackets)/(double)flow->second.txPackets << " " << (flow->second.delaySum.GetSeconds()/flow->second.rxPackets) << " " << flow->second.txPackets << " " << flow->second.rxPackets << " " << (flow->second.jitterSum.GetSeconds()/(flow->second.rxPackets))  << endl;
                            monitor[t.destinationPort] = netStatsOut.str();
			}
                        tp_transmitted[flow->first] = (double)flow->second.rxBytes;
		}
		
	}
	FILE * pFile;
	pFile = fopen (loc2.c_str(),"a");
        map<int, std::string>::iterator it;
        for (it = monitor.begin(); it != monitor.end(); it++)
        {
              fprintf(pFile, it->second.c_str());
        }
	fclose (pFile);
	Simulator::Schedule (Seconds (.05), &Throughput); // Callback every 0.5s

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
        Ptr<Ipv4> ipv4 = subNodes.Get(j)->GetObject<Ipv4>();
        int ind = i+1;
        /*if (not ipv4->IsUp (ind)){
            ind += 1;
        }*/
        Ipv4Address addr5_ = ipv4_2->GetAddress(ind,0).GetLocal();
        subNodeStaticRouting3->AddNetworkRouteTo (remoteHostContainer.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.0.0.0"), addr5_, cc, 0);
    }
  }
}

void changeRoute (std::vector<NodeContainer> nodes, int index, std::string fileID) {
          std::cout << "Getting the available interfaces" << std::endl;
          Ipv4StaticRoutingHelper ipv4RoutingHelper;
          //for (int index = 1; index < ueNodes.GetN()+1; index ++){
          Ptr<Ipv4> ipv4_2 = nodes[2].Get(0)->GetObject<Ipv4>();
          std::string delimiter = ".";
          std::ostringstream ss;
          ss << ipv4_2->GetAddress(1,0);//ipHeader.GetSource();
          std::vector<std::string> ip;
          std::string ip_ = ss.str();
          size_t pos = 0;
          while ((pos = ip_.find(delimiter)) != std::string::npos) {
               ip.push_back(ip_.substr(0, pos));
               ip_.erase(0, pos + delimiter.length());
          }

          std::stringstream ss1;
          ss1 << ip[1];
          int ID2;
          ss1 >> ID2;
          std::cout << "I am " << ip[1] << std::endl;
          string temp = ss1.str(); //.substr(ss1.str().length() - 2);
          if (ss1.str().length() > 2){
                temp = ss1.str().substr(ss1.str().length() - 2);
          }
          int ID = 0;

          int NewIndex = 1 + (rand() % int(nodes[2].GetN()));//random - (rand() % int(ueNodes.GetN()/2));
          if (NewIndex > nodes[2].GetN()){
              NewIndex =  1;
          }


          map<int, std::vector<int>>::iterator it;
          map<int, map<int, std::vector<int>>>::iterator itx;
          map<int, int> min;
          map<int, int> max;
          int nextNode_ind = NewIndex; //(rand() % int(ueNodes.GetN())-2);
          int nextNode_TP = 10000000;
          int count = 0;
          struct stat buffer;

          std::string line;
          std::ifstream infile("/people/belo700/RD2C/RC/integration/control/AgentDecisions.txt"); //fileID.c_str());
          NewIndex = interface[ID];
          while (std::getline(infile, line))
          {
               std::cout << "Please print something!!" << std::endl;
               std::cout << line << std::endl;
               std::string delimiter = ":";
               size_t pos = 0;
               pos = line.find(delimiter);
               std::string token;
               token = line.substr(0, pos);
               line.erase(0, pos + delimiter.length());
               int ID = std::stoi(token) - 1;
               int NewIndex = std::stoi(line) + 1;
	       //std::cout << nodes[1].GetN() << std::endl;
	       //std::cout << nodes[1].Get(ID)->GetObject<Ipv4>() << std::endl;
	       //std::cout << nodes[1].Get(ID)->GetObject<Ipv4>()->GetNInterfaces() << std::endl;
	       //if (not nodes[1].Get(ID)->GetObject<Ipv4>()->IsUp(NewIndex)){
               //   nodes[1].Get(ID)->GetObject<Ipv4>()->SetUp(NewIndex);
               //}
	       for (int x = 0; x < 4 ; x++){
	           nodes[2].Get(ID)->GetObject<Ipv4>()->SetUp(x+1);
	       }
	       //nodes[1].Get(ID)->GetObject<Ipv4>()->SetUp(NewIndex);
               //std::cout << "LOOOOOOOOKKKKKEEEE BELOW!!!" << std::endl;
               //std::cout << NewIndex << std::endl;
               //std::cout << interface[ID] << std::endl;
               /*Ptr<Ipv4> ipv4_3 = nodes[1].Get(ID)->GetObject<Ipv4>();
               Ipv4InterfaceAddress add12 = ipv4_3->GetAddress(NewIndex,0);
               Ipv4InterfaceAddress add1 = ipv4_3->GetAddress(interface[ID],0);
               ipv4_3->AddAddress (NewIndex, add1);
               ipv4_3->AddAddress (interface[ID], add12);
               ipv4_3->RemoveAddress (interface[ID], add1.GetLocal());
               ipv4_3->RemoveAddress (NewIndex, add12.GetLocal());*/
               previous[ID] = interface[ID];
               nodes[2].Get(ID)->GetObject<Ipv4>()->SetDown(interface[ID]);
               interface[ID] = NewIndex;
               
           }
           Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
	   Simulator::Schedule(MilliSeconds(period_routing), changeRoute, nodes, nextNode_ind, fileID);
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

  uint32_t rngRun = 1;
  cmd.AddValue ("RngRun", "Seed for random generator", rngRun);

  std::cout << "number of args: " << argc << std::endl;
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("helicsConfig", "Helics configuration file path", helicsConfigFileName);
  cmd.AddValue("microGridConfig", "NS3 MicroGrid configration file path", configFileName);
  cmd.AddValue("topologyConfig", "NS3 topology configuration file path", topologyConfigFileName);
  cmd.AddValue("pointFileDir", "Points file path", pointFileDir);
  cmd.AddValue("pcapFileDir", "PCAP output file path", pcapFileDir);
  cmd.Parse(argc, argv);

  readMicroGridConfig(configFileName, configObject);
  readMicroGridConfig(helicsConfigFileName, helicsConfigObject);
  readMicroGridConfig(topologyConfigFileName, topologyConfigObject);

  // Set random seed and run number
  int StaticSeed = std::stoi(configObject["Simulation"][0]["StaticSeed"].asString());
  if (StaticSeed == 1){
     RngSeedManager::SetSeed (std::stoi(configObject["Simulation"][0]["RandomSeed"].asString())); //12345); // Arbitrary seed value
  }else{
     RngSeedManager::SetSeed (std::time(NULL));
  }
  RngSeedManager::SetRun (rngRun);


  if (verbose)
  {
    //LogComponentEnable ("IntagrationExample", LOG_LEVEL_INFO);
    // LogComponentEnable ("HelicsSimulatorImpl", LOG_LEVEL_LOGIC);
    //LogComponentEnable ("GooseApplicationNew", LOG_LEVEL_INFO);
    // LogComponentEnable ("HelicsApplication", LOG_LEVEL_LOGIC);
    //LogComponentEnable ("Names", LOG_LEVEL_LOGIC);
  }

  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (999999999));
  //Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  
  std::cout << "Helics configuration file: " << helicsConfigFileName.c_str() << std::endl;
  std::cout << "MicroGrid configuration file: " << configFileName.c_str() << std::endl;


  HelicsHelper helicsHelper(std::stoi(helicsConfigObject["brokerPort"].asString()));
  std::cout << "Calling Calling Message Federate Constructor" << std::endl;
  helicsHelper.SetupApplicationFederate();

  std::string fedName = helics_federate->getName();
 

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (topologyConfigObject["Channel"][0]["P2PRate"].asString()));
  //p2p.SetChannelAttribute ("Delay", StringValue (topologyConfigObject["Channel"][0]["P2Pdelay"].asString()));

  CsmaHelper csma2;
  csma2.SetChannelAttribute("Delay", TimeValue (NanoSeconds (std::stoi(topologyConfigObject["Channel"][0]["CSMAdelay"].asString()))));

  NS_LOG_INFO ("Enabling OLSR Routing.");
  OlsrHelper olsr;
  
   //Ipv4StaticRoutingHelper staticRouting;
   
   Ipv4ListRoutingHelper list;
   //list.Add (staticRouting, 0);
   list.Add (olsr, 10);

  InternetStackHelper internetStack;
  InternetStackHelperMIM internetStackMIM;
  //internetStack.SetRoutingHelper (list);
  //internetStack.SetRoutingHelper (list);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0", "0.0.0.1");

  simTime = Seconds(std::stof(configObject["Simulation"][0]["SimTime"].asString()));
  float start = std::stof(configObject["Simulation"][0]["StartTime"].asString());
  int includeMIM = std::stoi(configObject["Simulation"][0]["includeMIM"].asString());
  // includeFDI is a new key -- guard its lookup so configs written before this
  // feature existed don't crash on a missing key (unlike includeMIM, which every
  // existing config already has). See DNP3/Modbus/MMS's identical addition.
  int includeFDI = configObject["Simulation"][0].isMember("includeFDI") ? std::stoi(configObject["Simulation"][0]["includeFDI"].asString()) : 0;

  NodeContainer Microgrid;
  Microgrid.Create(configObject["microgrid"].size());
  internetStack.Install(Microgrid);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  NodeContainer MIMNode;
  NodeContainer hubNode;

  bool ring = stoi(configObject["Simulation"][0]["UseDynTop"].asString()) == 1; //false; //true;
  bool star_bool = stoi(configObject["Simulation"][0]["UseDynTop"].asString()) == 0; ////true; //false;

  //Ring topology start
  NodeContainer nodes;
  YansWifiPhyHelper phy;
  if (ring){
      //generating the nodes following the number of nodes that was mentioned in the configuration file
      nodes.Create(topologyConfigObject["Node"].size());
      std::cout << "Creating the nodes " << topologyConfigObject["Node"].size() << " vs " << configObject["MIM"].size() << std::endl;
      //Dividing the nodes depending on whether they will serve as control center or man in the middle
      for (int h = 0; h < configObject["MIM"].size()-1; h++){
          MIMNode.Add(nodes.Get(h));
      }
      hubNode.Add(nodes.Get(configObject["MIM"].size()-1));
      std::cout << "MIM nodes have been added" << std::endl;

      YansWifiChannelHelper channel;
      WifiMacHelper mac;
      WifiHelper wifi;
      std::vector<NetDeviceContainer> NetRing;
      //Attaching the nodes to the InternetStacks
      if (std::stoi(topologyConfigObject["Node"][0]["UseWifi"].asString())==0){
          internetStackMIM.Install(MIMNode);
          internetStack.Install(hubNode);
      }else{
	  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 
	  channel.SetPropagationDelay("ns3::"+topologyConfigObject["Channel"][0]["WifiPropagationDelay"].asString()); // ConstantSpeedPropagationDelayModel");
	  Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
	  double minLoss = 98.0 - 40.0; 
	  double maxLoss = 98.0 + 40.0;
	  random->SetAttribute("Min", DoubleValue(minLoss));
	  random->SetAttribute("Max", DoubleValue(maxLoss));
	  channel.AddPropagationLoss("ns3::RandomPropagationLossModel", "Variable", PointerValue(random));
	  phy.SetChannel (channel.Create ());
	  std::string phyMode(topologyConfigObject["Channel"][0]["WifiRate"].asString()); //"DsssRate1Mbps");
	  if (topologyConfigObject["Channel"][0]["WifiStandard"].asString().compare("80211b") == 0){
	      wifi.SetStandard (WIFI_STANDARD_80211b);
	  }
          wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode",  StringValue(phyMode));

      }
      std::cout << "Added the hub Node" << std::endl;
      NetDeviceContainer NN;
      //Connecting the nodes as a ring
      if (true){ //(std::stoi(topologyConfigObject["Node"][0]["UseWifi"].asString())==1){
        for (int node = 0; node < topologyConfigObject["Node"].size(); node++){
          std::cout << "Node " << topologyConfigObject["Node"][node]["name"] << " is connected to the following "<< topologyConfigObject["Node"][node]["connections"].size() << " nodes:" << std::endl;
	  //int con = 0;
          for(int con = 0; con < topologyConfigObject["Node"][node]["connections"].size(); con++){
             std::cout << topologyConfigObject["Node"][node]["UseCSMA"].asString() << " " << topologyConfigObject["Node"][node]["UseWifi"].asString() <<  ": Connection " << con << " " << topologyConfigObject["Node"][node]["connections"][con] << std::endl;	     
	     if(std::stoi(topologyConfigObject["Node"][node]["UseCSMA"].asString())==1){
		 //CsmaHelper csma2;
		 int MTU = 1476;
		 for(auto it = topologyConfigObject["Node"][node].begin(); it != topologyConfigObject["Node"][node].end(); ++it) {
                     if (it.key().compare("MTU") == 0){
                         MTU = std::stoi(topologyConfigObject["Node"][node]["MTU"].asString());
		     }
		 }
	         csma2.SetDeviceAttribute("Mtu", UintegerValue(MTU));
                 NetDeviceContainer NetDev = csma2.Install(NodeContainer(nodes.Get(std::stoi(topologyConfigObject["Node"][node]["name"].asString())), nodes.Get(std::stoi(topologyConfigObject["Node"][node]["connections"][con].asString())))); 
	         NetRing.push_back(NetDev);
	     }else if (std::stoi(topologyConfigObject["Node"][node]["UseWifi"].asString())==1){
		     //Ssid ssid = Ssid ("ns3-80211ax-"+std::to_string(node));
		     //mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));
		     mac.SetType ("ns3::AdhocWifiMac");
		     NetDeviceContainer nodeDevices = wifi.Install (phy, mac, NodeContainer(nodes.Get(std::stoi(topologyConfigObject["Node"][node]["name"].asString())), nodes.Get(std::stoi(topologyConfigObject["Node"][node]["connections"][con].asString())))); 
		     NN.Add(nodeDevices.Get(0));
		     NN.Add(nodeDevices.Get(0));
	     }else{
		 std::cout << "Please print something" << std::endl;
		 std::cout << "I should be here " << topologyConfigObject["Node"][node]["name"].asString() << std::endl;
	         NetDeviceContainer NetDev = p2p.Install(NodeContainer(nodes.Get(std::stoi(topologyConfigObject["Node"][node]["name"].asString())), nodes.Get(std::stoi(topologyConfigObject["Node"][node]["connections"][con].asString()))));
                 NetRing.push_back(NetDev);
	     }
	     //if (){
	     /*for (int u = 0; u < NetRing.size(); u++){
		 //loss rate
                 Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
	         em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
	         NetRing[u].Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
	     }*/
	     //}
	   }
        }
      }
      if (std::stoi(topologyConfigObject["Node"][0]["UseWifi"].asString())==1){
	 // First item to do is to pass all the intergers of the Positionallocator through the json file
	 // Then I need to look into different LoyoutTypes that are supported by ns3
	 MobilityHelper mobility;
	 std::cout << "MinX " << topologyConfigObject["Gridlayout"][0]["MinX"].asString() << std::endl;
	 std::cout << "MinY " << topologyConfigObject["Gridlayout"][0]["MinY"].asString() << std::endl;
	 std::cout << "DeltaX " << topologyConfigObject["Gridlayout"][0]["DeltaX"].asString() << std::endl;
	 std::cout << "DeltaY " << topologyConfigObject["Gridlayout"][0]["DeltaY"].asString() << std::endl;
	 std::cout << "GridLayout " << topologyConfigObject["Gridlayout"][0]["GridWidth"].asString() << std::endl; 
	 mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
	                         "MinX", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["MinX"].asString())),
	                         "MinY", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["MinY"].asString())),
	                         "DeltaX", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["DeltaX"].asString())),
	                         "DeltaY", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["DeltaY"].asString())),
	                         "GridWidth", UintegerValue (std::stoi(topologyConfigObject["Gridlayout"][0]["GridWidth"].asString())),
	                         "LayoutType", StringValue (topologyConfigObject["Gridlayout"][0]["LayoutType"].asString()));
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (nodes);
	NetRing.push_back(NN);
        internetStackMIM.Install(MIMNode);
        internetStack.Install(hubNode);
	std::cout << "Finished adding to Stack " << topologyConfigObject["Gridlayout"][0]["SetPos"].asString() << std::endl;
      }
      if(std::stoi(topologyConfigObject["Gridlayout"][0]["SetPos"].asString()) == 1){
	  for (int x = 0; x < topologyConfigObject["Node"].size(); x++){
	      Ptr<ConstantPositionMobilityModel> mob = nodes.Get(std::stoi(topologyConfigObject["Node"][x]["name"].asString()))->GetObject<ConstantPositionMobilityModel>();
              Vector m_position = mob->GetPosition();
	      m_position.y = std::stoi(topologyConfigObject["Node"][x]["y"].asString());
	      m_position.x = std::stoi(topologyConfigObject["Node"][x]["x"].asString());
              mob->SetPosition(m_position);
	  }
      }
      //assigning the address to the nodes
      Ipv4AddressHelper ipv4Sub;
      std::string address = "10.1.1.0";
      ipv4Sub.SetBase(address.c_str(), "255.255.255.0", "0.0.0.1");
      std::cout << NetRing.size() << std::endl;
      for (int h = 0; h < NetRing.size(); h++){
	    std::cout << "I am in the NetRing Setup if statement " << h << std::endl;
	    if (h < topologyConfigObject["Node"].size() && std::stoi(topologyConfigObject["Node"][h]["UseWifi"].asString()) == 0){
	      Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
	      std::cout << topologyConfigObject["Node"][h] << std::endl;
	      em->SetAttribute ("ErrorRate", DoubleValue (std::stof(topologyConfigObject["Node"][h]["error"].asString())));//0.00001));
	      NetRing[h].Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
	    }
	    for (int x = 0; x < NetRing[h].GetN(); x++){
	      Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign(NetRing[h].Get(x));
              //Ipv4InterfaceContainer interfacesSubCC = ipv4Sub.Assign(NetRing[h].Get(1));
	    }
	    ipv4Sub.NewNetwork ();
	    std::cout << "DONE" << std::endl; 
	    
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

       MobilityHelper mobility;
         std::cout << "MinX " << topologyConfigObject["Gridlayout"][0]["MinX"].asString() << std::endl;
         std::cout << "MinY " << topologyConfigObject["Gridlayout"][0]["MinY"].asString() << std::endl;
         std::cout << "DeltaX " << topologyConfigObject["Gridlayout"][0]["DeltaX"].asString() << std::endl;
         std::cout << "DeltaY " << topologyConfigObject["Gridlayout"][0]["DeltaY"].asString() << std::endl;
         std::cout << "GridLayout " << topologyConfigObject["Gridlayout"][0]["GridLayout"].asString() << std::endl;
         mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["MinX"].asString())),
                                 "MinY", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["MinY"].asString())),
                                 "DeltaX", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["DeltaX"].asString())),
                                 "DeltaY", DoubleValue (std::stoi(topologyConfigObject["Gridlayout"][0]["DeltaY"].asString())),
                                 "GridWidth", UintegerValue (std::stoi(topologyConfigObject["Gridlayout"][0]["GridWidth"].asString())),
                                 "LayoutType", StringValue (topologyConfigObject["Gridlayout"][0]["LayoutType"].asString()));
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (hubNode);
	mobility.Install (MIMNode);
	mobility.Install (Microgrid);
  }
  Ptr<Ipv4StaticRouting> ControlRouting = ipv4RoutingHelper.GetStaticRouting(hubNode.Get(0)->GetObject<Ipv4>()); //star.GetHub()->GetObject<Ipv4>());
  // GOOSE-specific: this per-microgrid link is CSMA, not the
  // PointToPointHelper (p2p) every other protocol's copy of this file
  // uses here -- see file header note on why a shared/multicast-capable
  // segment is required. Kept for use below when installing the
  // publisher/subscriber applications and setting up the multicast
  // route on the publisher's device.
  CsmaHelper gooseCsma;
  gooseCsma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  gooseCsma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (500)));
  // A full substation's dataset serializes to several KB, which would
  // otherwise require IP fragmentation at the standard 1500-byte MTU --
  // ns-3's fragment reassembly does not deliver reassembled multicast
  // datagrams to the UDP socket (confirmed: HandleRead never fires for
  // a fragmented payload, but fires reliably once the payload fits in
  // one frame). Real GOOSE deployments size datasets to avoid
  // fragmentation in the first place; raising the MTU here is the
  // simulation-side equivalent so one substation's full point set can
  // still be published as a single GOOSE message.
  gooseCsma.SetDeviceAttribute ("Mtu", UintegerValue (9000));
  std::vector<NetDeviceContainer> gooseSegments (configObject["microgrid"].size ());
  for (i = 0; i < configObject["microgrid"].size(); i++){
	  auto ep_name = configObject["microgrid"][i]["name"].asString();
	  std::cout << "Microgrid network node: " << ep_name << " " << configObject["microgrid"].size() << std::endl;
	  Ptr<Node> tempNode = Microgrid.Get(i);
	  //Names::Add(ep_name, tempNode);
	  NetDeviceContainer NetDev = gooseCsma.Install(NodeContainer(tempNode,MIMNode.Get(i))); //star.GetSpokeNode(i)));
	  gooseSegments[i] = NetDev;
	  Ipv4AddressHelper ipv4Sub;
	  std::string address = "11."+std::to_string(i+2)+".0.0";
	  ipv4Sub.SetBase(address.c_str(), "255.255.255.0", "0.0.0.1");
	  Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign(NetDev);
  }
  PointToPointHelper p2ph2;
  std::string rate = configObject["DDoS"][0]["Rate"].asString();
  p2ph2.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (rate)));
  //p2ph2.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
  p2ph2.SetDeviceAttribute ("Mtu", UintegerValue (std::stoi(configObject["DDoS"][0]["PacketSize"].asString())));
  p2ph2.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (std::stoi(topologyConfigObject["Channel"][0]["delay"].asString()))));

  int numBots = std::stoi(configObject["DDoS"][0]["NumberOfBots"].asString());
  NodeContainer botNodes;
  botNodes.Create(numBots);
  int numThreads = std::stoi(configObject["DDoS"][0]["threadsPerAttacker"].asString());
  NetDeviceContainer botDeviceContainer[numBots];
  for (int i = 0; i < numBots; ++i)
  {
          if (configObject["DDoS"][0]["NodeType"][0].asString().find("CC") != std::string::npos){
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), hubNode.Get(0));
          }
	  else if (configObject["DDoS"][0]["NodeType"][0].asString().find("subNode") != std::string::npos){
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), Microgrid.Get(int(((i%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString())))));
          }
	  else{
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), MIMNode.Get(int(((i%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))))); //remoteHostContainer.Get(0));//We are currently attacking the remoteHost but I will need to change that in the future to be dynamic
          }
  }

  internetStack.Install(botNodes);
  Ipv4AddressHelper ipv4_n;
  ipv4_n.SetBase("30.0.0.0", "255.255.255.252");

  for (int j = 0; j < numBots; ++j)
  {
          ipv4_n.Assign(botDeviceContainer[j]);
          ipv4_n.NewNetwork();
  }


  //if (ring){
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  //}
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

  //reading the FDI (compromised-endpoint false data injection) configuration.
  //Missing "FDI" key is fine -- configObject["FDI"] just comes back empty/null
  //and the loops below run zero times, matching includeFDI defaulting to 0.
  std::map<std::string, std::string> attackFDI;
  for (uint32_t j = 1; j < configObject["FDI"].size(); j++){
     for(const auto& item : configObject["FDI"][j].getMemberNames() ){
	 std::string ID = "FDI-"+std::to_string(j)+"-"+item;
	 std::string my_str = configObject["FDI"][j][item].asString();
	 my_str.erase(remove(my_str.begin(), my_str.end(), '"'), my_str.end());
         attackFDI.insert(pair<std::string,std::string >(ID, my_str));
     }
  }

  std::string IDsFDI = configObject["FDI"][0]["listFDI"].asString();
  std::vector<std::string> valFDI;
  {
    size_t posFDI = 0;
    std::string tokenFDI;
    std::string VIFDI = IDsFDI;
    while ((posFDI = VIFDI.find(delimiter)) != std::string::npos) {
	    tokenFDI = VIFDI.substr(0, posFDI);
	    valFDI.push_back(tokenFDI);
	    VIFDI.erase(0, posFDI + delimiter.length());
    }
    valFDI.push_back(VIFDI);
  }

  uint16_t goosePort = 40000;
  // Kept only because the shared DDoS setup code further below still
  // references master_port (as UDP_SINK_PORT) -- not otherwise used by
  // GOOSE's own install sites, which use goosePort throughout instead.
  uint16_t master_port = 40000;
  ApplicationContainer goosePublisherApp, gooseSubscriberApp;
  // Indexed by microgrid i, so the later attack-config-gated loop can
  // set mitmFlag/attack attributes directly on the already-installed
  // subscriber object instead of a second Install() call (see file
  // header note on why GOOSE doesn't need a separate MIM install site).
  std::vector<Ptr<GooseApplicationNew> > gooseSubscriberByMicrogrid (configObject["microgrid"].size ());
  Ipv4StaticRoutingHelper gooseMulticastRouting;
  //changing the parameters of the nodes in the network
  Simulator::Schedule (Seconds (3.2), &updatePower);

  for (i = 0;i < configObject["microgrid"].size();i++)
  {
    auto ep_name = configObject["microgrid"][i]["name"].asString();
    std::cout << "Microgrid network node: " << ep_name << " " << Microgrid.GetN() << " " << configObject["microgrid"][i].size() << " " << i << std::endl;
    Ptr<Node> tempnode1 = Microgrid.Get(i); //star.GetSpokeNode (i);

    std::string IDx = "SS_";
    // BUG FIX: this used to unconditionally rebuild ep_name as "SS_"+(i+1),
    // which is only correct if the config's microgrid names are 1-indexed
    // (SS_1..SS_N). The real 123-bus config (integration/control/config/
    // grid.json) names them 0-indexed (SS_0..SS_9), matching the loop's
    // own 0-indexed `i` directly -- so the +1 silently renamed every
    // substation to the WRONG points file (SS_0 -> looks for
    // points_SS_1.csv, ..., SS_9 -> looks for points_SS_10.csv, which
    // doesn't exist -- initConfig's exit(-1) on a missing file would
    // crash the last substation's load for any protocol using this
    // shared loop, not just MMS/GOOSE). Using `i` directly matches
    // what's actually on disk for the current config.
    if (std::string(ep_name).find(IDx) != std::string::npos){
        ep_name = "SS_"+std::to_string(i);
    }
    std::cout << ep_name << std::endl;
    interface[i] = i+1;

    // GOOSE's multicast group is per-microgrid (224.10.<i>.1) so
    // different substations' GOOSE traffic doesn't collide -- real
    // GOOSE never leaves the local substation LAN in the first place.
    std::string gooseGroupStr = "224.10." + std::to_string (i) + ".1";
    Ipv4Address gooseGroup (gooseGroupStr.c_str ());

    // -- Publisher: the substation device (Microgrid.Get(i)) --
    GooseApplicationHelperNew goosePublisher ("ns3::UdpSocketFactory",
      InetSocketAddress (tempnode1->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), goosePort));
    goosePublisher.SetAttribute("LocalPort", UintegerValue(goosePort));
    goosePublisher.SetAttribute("RemoteAddress", AddressValue(gooseGroup));
    goosePublisher.SetAttribute("RemotePort", UintegerValue(goosePort));
    goosePublisher.SetAttribute("isMaster", BooleanValue(true));
    goosePublisher.SetAttribute("Name", StringValue(ep_name));
    goosePublisher.SetAttribute("PointsFilename", StringValue(pointFileDir+"/points_"+ep_name+".csv"));
    goosePublisher.SetAttribute("GooseID", StringValue("GCB_"+ep_name));
    goosePublisher.SetAttribute("JitterMinNs", DoubleValue (std::stoi(topologyConfigObject["Channel"][0]["jitterMin"].asString())));
    goosePublisher.SetAttribute("JitterMaxNs", DoubleValue (std::stoi(topologyConfigObject["Channel"][0]["jitterMax"].asString())));

    Ptr<GooseApplicationNew> publisher = goosePublisher.Install (tempnode1, std::string(ep_name));
    goosePublisherApp.Add(publisher);
    // Starts the publisher's own burst/heartbeat schedule -- see
    // attack_data's role dispatch (mitmFlag is false here, so this
    // takes the schedulePublish() branch); the interval argument is
    // unused in that branch.
    Simulator::Schedule(MilliSeconds(1005), &GooseApplicationNew::attack_data, publisher, 0);

    // A default multicast route on the publisher's own device is what
    // lets its outbound multicast packets actually get routed onto the
    // shared segment at all (see goose-application-new.cc's
    // makeMulticastConnection note, and src/csma/examples/csma-multicast.cc
    // for the real ns-3 pattern this follows).
    gooseMulticastRouting.SetDefaultMulticastRoute (tempnode1, gooseSegments[i].Get (0));

    // -- Subscriber: the insider/MIM node (MIMNode.Get(i)), always --
    // -- installed so real subscriber traffic exists regardless of --
    // -- whether this microgrid is also an attack target (see below). --
    GooseApplicationHelperNew gooseSubscriber ("ns3::UdpSocketFactory",
      InetSocketAddress (MIMNode.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), goosePort));
    gooseSubscriber.SetAttribute("LocalPort", UintegerValue(goosePort));
    gooseSubscriber.SetAttribute("RemoteAddress", AddressValue(gooseGroup));
    gooseSubscriber.SetAttribute("RemotePort", UintegerValue(goosePort));
    gooseSubscriber.SetAttribute("isMaster", BooleanValue(false));
    gooseSubscriber.SetAttribute("Name", StringValue(ep_name+"_sub"));
    gooseSubscriber.SetAttribute("PointsFilename", StringValue(pointFileDir+"/points_"+ep_name+".csv"));
    gooseSubscriber.SetAttribute("GooseID", StringValue("GCB_"+ep_name));

    Ptr<GooseApplicationNew> subscriber = gooseSubscriber.Install (MIMNode.Get(i), std::string(ep_name+"_sub"));
    gooseSubscriberApp.Add(subscriber);
    gooseSubscriberByMicrogrid[i] = subscriber;
  }

  int control = std::stoi(configObject["Controller"][0]["use"].asString());
  if (control){
      std::vector<NodeContainer> nodes;
      nodes.push_back(hubNode);
      nodes.push_back(MIMNode);
      nodes.push_back(Microgrid);
      Simulator::Schedule(MilliSeconds(2005+period_routing), changeRoute, nodes, 2, configObject["Controller"][0]["actionFile"].asString());// was 6005
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

  //Adding the rogue GOOSE publisher (attack-config-gated attributes on
  //the already-installed subscriber -- see file header note on why
  //GOOSE has no separate MIM install site the way DNP3/Modbus/MMS do)
  if (includeMIM == 1){
    for (int x = 0; x < val.size(); x++){ //std::stoi(configObject["MIM"][0]["NumberAttackers"].asString()); x++){
      int MIM_ID = std::stoi(val[x]) + 1; //x+1;

      // GOOSE has no in-path interception (see file header note) --
      // no Ipv4L3ProtocolMIM/victimAddr redirection is applicable here,
      // unlike DNP3/Modbus/MMS's unicast client-server model, since a
      // rogue publisher works by adding a competing multicast sender,
      // not by redirecting existing traffic.
      Ptr<GooseApplicationNew> rogue = gooseSubscriberByMicrogrid[MIM_ID-1];
      if (!rogue) { continue; } // shouldn't happen -- every microgrid gets a subscriber above

      rogue->SetAttribute("mitmFlag", BooleanValue(true));
      // Gives handle_rogue_publish's internal JSON parsing access to
      // the same MIM attack_type/attack_chance/PointStart/PointStop
      // entries already parsed into `attack` above -- reusing
      // --microGridConfig's own path, since that's where NATIG's
      // attack config actually lives (configObject["MIM"], read at
      // the top of this function).
      rogue->SetAttribute("AttackConf", StringValue(configFileName));
      rogue->SetAttribute("ID", UintegerValue(MIM_ID));
      rogue->SetAttribute("AttackSelection", UintegerValue(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"])));
      rogue->SetAttribute("RealVal", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-real_val"]));

      if (std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 2 || std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 4){
         if(attack["MIM-"+std::to_string(MIM_ID)+"-scenario_id"] == "b"){
            rogue->SetAttribute("Value_attck_max", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-attack_val"]));
            rogue->SetAttribute("Value_attck_min", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-real_val"]));
            rogue->SetAttribute("NodeID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-node_id"]));
            rogue->SetAttribute("PointID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-point_id"]));
         }
         if(attack["MIM-"+std::to_string(MIM_ID)+"-scenario_id"] == "a"){
            rogue->SetAttribute("Value_attck", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-attack_val"]));
            rogue->SetAttribute("NodeID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-node_id"]));
            rogue->SetAttribute("PointID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-point_id"]));
          }
      }

      if(std::stoi(attack["MIM-"+std::to_string(MIM_ID)+"-attack_type"]) == 3){
         rogue->SetAttribute("Value_attck", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-attack_val"]));
         rogue->SetAttribute("NodeID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-node_id"]));
         rogue->SetAttribute("PointID", StringValue (attack["MIM-"+std::to_string(MIM_ID)+"-point_id"]));
      }

      rogue->SetAttribute("AttackStartTime", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-Start"]));
      rogue->SetAttribute("AttackEndTime", StringValue(attack["MIM-"+std::to_string(MIM_ID)+"-End"]));
    }
  }

  //Compromised-endpoint FDI: unlike the rogue-publisher MIM attack above,
  //this does not add a competing publisher -- it sets attributes directly
  //on the real per-microgrid publisher itself (goosePublisherApp, not
  //gooseSubscriberByMicrogrid), simulating a compromised meter/RTU that
  //lies about its own readings. A fabricated value here flows through the
  //normal publish path, so it naturally trips the same
  //datasetChangedSinceLastPublish()/burst logic a real change would --
  //no stNum-forging needed since this is the authoritative publisher.
  //See DNP3/Modbus/MMS's identical wiring.
  if (includeFDI == 1){
    for (int x = 0; x < valFDI.size(); x++){
      int FDI_ID = std::stoi(valFDI[x]) + 1;
      int mgIndex = FDI_ID - 1; //matches goosePublisherApp's install order in the microgrid loop above
      Ptr<GooseApplicationNew> publisher = DynamicCast<GooseApplicationNew>(goosePublisherApp.Get(mgIndex));
      if (!publisher) { continue; }

      publisher->SetAttribute("FdiFlag", BooleanValue(true));
      publisher->SetAttribute("FdiID", UintegerValue(FDI_ID));
      publisher->SetAttribute("Value_attck", StringValue(attackFDI["FDI-"+std::to_string(FDI_ID)+"-attack_val"]));
      publisher->SetAttribute("NodeID", StringValue(attackFDI["FDI-"+std::to_string(FDI_ID)+"-node_id"]));
      publisher->SetAttribute("PointID", StringValue(attackFDI["FDI-"+std::to_string(FDI_ID)+"-point_id"]));
      publisher->SetAttribute("AttackChance", DoubleValue(std::stod(attackFDI["FDI-"+std::to_string(FDI_ID)+"-attack_chance"])));
      publisher->SetAttribute("AttackStartTime", StringValue(attackFDI["FDI-"+std::to_string(FDI_ID)+"-Start"]));
      publisher->SetAttribute("AttackEndTime", StringValue(attackFDI["FDI-"+std::to_string(FDI_ID)+"-End"]));
    }
  }



  goosePublisherApp.Start (Seconds (start));
  goosePublisherApp.Stop (simTime);
  gooseSubscriberApp.Start (Seconds (start));
  gooseSubscriberApp.Stop (simTime);


  std::cout << "Setting up Bots" << std::endl;
  int BOT_START = std::stof(configObject["DDoS"][0]["Start"].asString());;
    int BOT_STOP = std::stof(configObject["DDoS"][0]["End"].asString());;
    std::string str_on_time = configObject["DDoS"][0]["TimeOn"].asString();
    std::string str_off_time = configObject["DDoS"][0]["TimeOff"].asString();
    int TCP_SINK_PORT = 9000;
    int UDP_SINK_PORT = master_port; //mimPort[2]-10;
    int MAX_BULK_BYTES = std::stof(configObject["DDoS"][0]["PacketSize"].asString()); //20971520000;
    std::string DDOS_RATE = configObject["DDoS"][0]["Rate"].asString(); //"2000kb/s";

    bool DDoS = std::stoi(configObject["DDoS"][0]["Active"].asString());

    /*if (DDoS){
            for (int k = 0; k < botNodes.GetN(); ++k)
            {
                Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (botNodes.Get(k)->GetObject<Ipv4> ());
                if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
                remoteHostStaticRouting->AddNetworkRouteTo (Microgrid.Get(k)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), MIMNode.Get(k)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(),1); //gateway, 1);
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                remoteHostStaticRouting->AddNetworkRouteTo (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), MIMNode.Get(k)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(),1); //gateway, 1);
                }
	    }
    }*/
    //Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    //Based on https://gist.github.com/Saket-Upadhyay/c4c702716233cab91eb31b6d547aaeab
    Time interPacketInterval{Seconds(1.0)};
    bool usePing = std::stoi(configObject["DDoS"][0]["usePing"].asString());
    //int numThreads = std::stoi(configObject["DDoS"][0]["threadsPerAttacker"].asString());
    if (DDoS){
            ApplicationContainer onOffApp[botNodes.GetN()];
	    //for (int i = 0; i < 8 ; i++){
	    for (int t = 0; t < 1; t++){
             if (usePing){
               if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                 V4PingHelper pingHelper(hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                 ApplicationContainer apps = pingHelper.Install(botNodes);
                 apps.Start(Seconds(BOT_START));
                 apps.Stop(Seconds(BOT_STOP));
               }else  if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
		  for (int k = 0; k < botNodes.GetN(); ++k)
                  {
                      V4PingHelper pingHelper(Microgrid.Get((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                      ApplicationContainer apps = pingHelper.Install(botNodes);
                      apps.Start(Seconds(BOT_START));
                      apps.Stop(Seconds(BOT_STOP));
		  }
               }
            }
                for (int k = 0; k < botNodes.GetN(); ++k)
                {
                    if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
                        //OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Microgrid.Get(k+6)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), UDP_SINK_PORT))); //remoteHostAddr, UDP_SINK_PORT)));
			//Config::SetDefault ("ns3::Ipv4RawSocketImpl::Protocol", StringValue ("2"));
			InetSocketAddress dst = InetSocketAddress (Microgrid.Get((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
			OnOffHelper onoff = OnOffHelper ("ns3::Ipv4RawSocketFactory", dst);
                        onoff.SetConstantRate(DataRate(DDOS_RATE));
			//onoff.SetConstantRate (DataRate (150000));
			//onoff.SetAttribute ("PacketSize", UintegerValue (1500));
                        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                        onOffApp[k] = onoff.Install(botNodes.Get(k));
                    }
                    else if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                        //OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), UDP_SINK_PORT))); //remoteHostAddr, UDP_SINK_PORT)));
			//Config::SetDefault ("ns3::Ipv4RawSocketImpl::Protocol", StringValue ("2"));
                        InetSocketAddress dst = InetSocketAddress (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                        OnOffHelper onoff = OnOffHelper ("ns3::Ipv4RawSocketFactory", dst);
                        onoff.SetConstantRate(DataRate(DDOS_RATE));
			//onoff.SetConstantRate (DataRate (150000));
			//onoff.SetAttribute ("PacketSize", UintegerValue (1500));
                        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                        onOffApp[k] = onoff.Install(botNodes.Get(k));
                    }
		    else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
                        InetSocketAddress dst = InetSocketAddress (MIMNode.Get((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                        OnOffHelper onoff = OnOffHelper ("ns3::Ipv4RawSocketFactory", dst);
                        onoff.SetConstantRate(DataRate(DDOS_RATE));
                        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                        onOffApp[k] = onoff.Install(botNodes.Get(k));
                    }
                    onOffApp[k].Start(Seconds(BOT_START));
                    onOffApp[k].Stop(Seconds(BOT_STOP));

                    if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
                        //PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                        //     Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
			InetSocketAddress dst = InetSocketAddress (Microgrid.Get((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
			PacketSinkHelper sink = PacketSinkHelper ("ns3::Ipv4RawSocketFactory", dst);
                        ApplicationContainer UDPSinkApp = sink.Install(Microgrid.Get((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))); //remoteHost);
                        UDPSinkApp.Start(Seconds(BOT_START));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));
                    }
		    else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
                        //PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                        //     Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
                        InetSocketAddress dst = InetSocketAddress (MIMNode.Get((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                        PacketSinkHelper sink = PacketSinkHelper ("ns3::Ipv4RawSocketFactory", dst);
                        ApplicationContainer UDPSinkApp = sink.Install(MIMNode.Get((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))); //remoteHost);
                        UDPSinkApp.Start(Seconds(BOT_START));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));
                    }
                }
                if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                        //PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                        //     Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
			InetSocketAddress dst = InetSocketAddress (hubNode.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                        PacketSinkHelper sink = PacketSinkHelper ("ns3::Ipv4RawSocketFactory", dst);
                        ApplicationContainer UDPSinkApp = sink.Install(hubNode.Get(0));
                        UDPSinkApp.Start(Seconds(BOT_START));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));

                }
		UDP_SINK_PORT += 1;
	      }
	    
     }

    std::cout << "Done Setting up the bots " << std::endl;

  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  if(!dirExists(pcapFileDir.c_str())) {
    mkdir(pcapFileDir.c_str(), 0777);
  }

  NodeContainer endpointNodes;
    for (int i = 0; i < hubNode.GetN(); i++){
        endpointNodes.Add (hubNode.Get (i));
    }
    for (int i = 0; i < MIMNode.GetN(); i++){
        endpointNodes.Add (MIMNode.Get (i));
    }
    for (int i = 0; i < Microgrid.GetN(); i++){
        endpointNodes.Add (Microgrid.Get (i));
    }

    flowMonitor = flowHelper.Install(endpointNodes);
    flowMonitor->SetAttribute("DelayBinWidth", DoubleValue(0.5));
    flowMonitor->SetAttribute("JitterBinWidth", DoubleValue(0.5));
    flowMonitor->SetAttribute("PacketSizeBinWidth", DoubleValue(50));
    int mon = std::stoi(configObject["Simulation"][0]["MonitorPerf"].asString());
    Simulator::Schedule (Seconds (0.2), &Throughput);

    if (mon) {
        gooseCsma.EnablePcapAll (pcapFileDir+"goose-csma", false);
        if (DDoS){
            p2p.EnablePcapAll (pcapFileDir+"p2p-DDoS", false);
            csma2.EnablePcapAll (pcapFileDir+"csma-DDoS", false);
            phy.EnablePcapAll (pcapFileDir+"wifi-DDoS", false);
        }else{
	    p2p.EnablePcapAll (pcapFileDir+"p2p", false);
            csma2.EnablePcapAll (pcapFileDir+"csma", false);
            phy.EnablePcapAll (pcapFileDir+"wifi", false);
        }
    }


  Simulator::Stop (simTime);
  Simulator::Run ();

  // BUG FIX: no explicit HELICS finalize/disconnect existed anywhere in
  // this codebase -- teardown relied entirely on helics_federate's
  // global shared_ptr destructor to close the broker connection
  // implicitly. Calling finalize() explicitly here is good practice
  // regardless of the exit mechanism below: it cleanly notifies the
  // broker of federate departure rather than leaving that to an
  // unpredictable destructor.
  if (helics_federate)
    {
      helics_federate->finalize ();
    }
  Simulator::Destroy ();

  // BUG FIX: every run consistently crashed (SIGABRT, no exception
  // message) immediately after this point, during the C++ runtime's
  // exit-time destruction of global/static objects -- confirmed via
  // extensive std::cerr instrumentation to happen strictly after
  // Simulator::Destroy() and even after manually resetting
  // helics_federate ourselves, meaning it's inside something with no
  // source-level visibility to us (likely HELICS/zmq library-internal
  // static teardown; no working debugger was available in this
  // container to go further -- see session notes). _exit() skips that
  // destructor chain entirely rather than working around a crash we
  // can't currently instrument. Safe here specifically because all
  // output of research value (pcap captures, flow-monitor data) is
  // written incrementally during the run itself, not at this point --
  // confirmed by real, non-empty pcap files after this fix.
  _exit (0);
}
