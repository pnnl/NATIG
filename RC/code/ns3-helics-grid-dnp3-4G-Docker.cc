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
//#include "ns3/gtk-config-store.h"
#include "ns3/csma-module.h"

#include "jsoncpp/ns3/helics-helper.h"
#include "jsoncpp/ns3/dnp3-application-helper-new.h"
#include "jsoncpp/ns3/dnp3-simulator-impl.h"

#include <json/json.h>
#include <json/forwards.h>
#include <json/writer.h>

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
std::map<int, int> previous;
std::map<int, std::map<int, std::vector<int>>> route_perf;
//std::map<int, std::vector<int>> reward;
std::map<int, map<int,std::vector<int>>> reward;
std::map<int, std::string> monitor;
std::map<int, int> interface;
std::map<int, float> epsilon;
float epsilon_min = 0.04; //0.01;
float epsilon_decay = 0.99; //0.99;
float alpha = 0.35; //0.35;
float gamma_ = 0.8; //0.3; //0.04;
int period_routing = 100; //1500;

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

void trTocsv(){
    std::ifstream trFile("edge_performance.tr");
    std::ofstream csvFile("output.csv");

    std::string line;
    std::vector<std::string> row;

    while (std::getline(trFile, line)) {
        row.clear();

        // Split the line by tab delimiter
        size_t pos = 0;
        std::string token;
        while ((pos = line.find('\t')) != std::string::npos) {
            token = line.substr(0, pos);
            row.push_back(token);
            line.erase(0, pos + 1);
        }
        row.push_back(line); // Add the last token

        // Write the row to the CSV file
        for (size_t i = 0; i < row.size(); ++i) {
            csvFile << row[i];
            if (i != row.size() - 1) {
                csvFile << ",";
            }
        }
        csvFile << "\n";
    }

    trFile.close();
    csvFile.close();
    Simulator::Schedule (Seconds (1), &trTocsv);
}

void StartEndSignal (std::string status){
   std::cout << "The attack has " << status << std::endl;
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
	int ID2 = 0;
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
                ss1 << ip[1];
                int ID;
                ss1 >> ID;
                string temp = ss1.str(); //.substr(ss1.str().length() - 2);
		/*if (ss1.str().length() > 2){
                    temp = ss1.str().substr(ss1.str().length() - 2);
                }*/
		//std::string::size_type i2 = temp.find(".");
                //std::string s = ".";
                //if (i2 != std::string::npos){
                //  temp.erase(i2, s.length());
                //}
                /*std::stringstream ss2;
                ss2 << temp;
                int ID2;
                ss2 >> ID2;*/

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

                if (ID > 0){
                    std::cout << "I am HERE -------------------------- " << ss1.str() << std::endl;
                    std::cout << "Choosing interface " << std::to_string(interface[ID2]) << "for" << std::to_string(ID) << std::endl;
                    if (route_perf.find(ID2) == route_perf.end()){
                          std::map<int, std::vector<int>> xxx;
                          route_perf[ID2] = xxx;
                    }
		    double lost = (double)flow->second.lostPackets;
		    route_perf[ID2][interface[ID2]].push_back(std::pow(2,(flow->second.rxPackets-lost)/(flow->second.delaySum.GetSeconds()+flow->second.jitterSum.GetSeconds())));
		    //route_perf[ID2][interface[ID2]].push_back(std::pow(2,(1-((double)flow->second.txPackets-(double)flow->second.rxPackets)/(double)flow->second.txPackets)*(((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024))); //std::pow(10,(((flow->second.rxPackets-lost)/flow->second.delaySum.GetSeconds()))));

		}


		if (((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024 > 0 )
		{
			double rx = (double)flow->second.rxBytes;
			if (ID == 0){
	 		    netStatsOut << Simulator::Now ().GetSeconds () << " " <<  t.sourcePort << " (" << proto << " " << t.sourceAddress << " / " << t.sourcePort << " --> " << t.destinationAddress << " / " << t.destinationPort << ") " << ((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024 << " " << flow->second.lostPackets << " " <<  (double)flow->second.rxBytes << " " << flow->second.txBytes << " " << ((double)flow->second.txPackets-(double)flow->second.rxPackets)/(double)flow->second.txPackets << " " << (flow->second.delaySum.GetSeconds()/flow->second.rxPackets) << " " << flow->second.txPackets << " " << flow->second.rxPackets << " " << (flow->second.jitterSum.GetSeconds()/(flow->second.rxPackets))  << endl;
                            monitor[t.sourcePort] = netStatsOut.str();
			}else{
			    netStatsOut << Simulator::Now ().GetSeconds () << " " <<  t.destinationPort << " (" << proto << " " << t.sourceAddress << " / " << t.sourcePort << " --> " << t.destinationAddress << " / " << t.destinationPort << ") " << ((double)flow->second.rxBytes*8)/((double)flow->second.timeLastRxPacket.GetSeconds()-(double)flow->second.timeFirstTxPacket.GetSeconds())/1024 << " " << flow->second.lostPackets << " " <<  (double)flow->second.rxBytes << " " << flow->second.txBytes << " " << ((double)flow->second.txPackets-(double)flow->second.rxPackets)/(double)flow->second.txPackets << " " << (flow->second.delaySum.GetSeconds()/flow->second.rxPackets) << " " << flow->second.txPackets << " " << flow->second.rxPackets << " " << (flow->second.jitterSum.GetSeconds()/(flow->second.rxPackets))  << endl;
                            monitor[t.destinationPort] = netStatsOut.str();
			}
                        tp_transmitted[flow->first] = (double)flow->second.rxBytes;
		}
		ID2 = ID2 + 1;
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
      for (int j = 1; j < subNodes.Get(i)->GetNDevices(); j++){
        addrTrans << subNodes.Get(i)->GetObject<Ipv4>()->GetAddress(j,0).GetLocal() << ": " << ueNodes.Get(j-1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << endl;
    }
  }

  std::cout << "The translation table" << std::endl;
  std::cout << addrTrans.str().c_str() << std::endl;

  FILE * addFile;
  std::string loc = std::getenv("RD2C");
  std::string loc1 = loc + "/integration/control/add.txt";
  std::cout << loc1 << std::endl;
  addFile = fopen (loc1.c_str(),"w");

  if (addFile!=NULL)
  {
          fprintf(addFile, addrTrans.str().c_str());
          fclose (addFile);
  }
}

void setRoutingTable(NodeContainer remoteHostContainer, NodeContainer subNodes, NodeContainer MIM, NodeContainer ueNodes, Ipv4Address gateway){

    // This sets the paths from the MIM nodes and UE nodes to the CC
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHostContainer.Get(0)->GetObject<Ipv4> ());
  for (int i = 0; i < ueNodes.GetN(); i++){
      remoteHostStaticRouting->AddNetworkRouteTo (ueNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), gateway, 1);
  }
  for (int j = 0; j < subNodes.GetN(); j++){
    for (int x = 1; x < subNodes.Get(j)->GetNDevices(); x++){
        remoteHostStaticRouting->AddNetworkRouteTo (subNodes.Get(j)->GetObject<Ipv4>()->GetAddress(x,0).GetLocal(), Ipv4Mask ("255.255.0.0"), gateway, 1, 0);
    }
  }
  

  // This sets the path from the UE nodes to the subNode passing by the MIM nodes
  // Gets the MIM node that corresponds to the subNodes node interface
  for (int i = 0; i < ueNodes.GetN(); i++){
    Ptr<Ipv4StaticRouting> ueNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get(i)->GetObject<Ipv4>());
    int cc = 0;
    Ptr<Ipv4> ipv4 = ueNodes.Get(i)->GetObject<Ipv4>();
    std::cout << "I just got the channel from the node " << ueNodes.Get(i)->GetNDevices() << std::endl;
    for (int j = 1; j < ueNodes.Get(i)->GetNDevices(); j++){
	 cc = 1;
         Ptr< NetDevice > level1 = ueNodes.Get(i)->GetDevice(j);
	 for (int x = 1; x < level1->GetNode()->GetNDevices(); x++){
              Ptr< NetDevice > level2 = level1->GetNode()->GetDevice(j);
              ueNodeStaticRouting->AddNetworkRouteTo(level2->GetNode()->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), Ipv4Mask("255.255.0.0"), level1->GetNode()->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), 1, 0);
	 }
    }
  }

  // MIM to the subNodes and MIM to the CC nodes that is not the default paths which is set above
  for (int i = 0; i < MIM.GetN(); i++){
    Ipv4Address addr2_ = ueNodes.Get(i)->GetObject<Ipv4>()->GetAddress (2, 0).GetLocal ();
    Ptr<Ipv4StaticRouting> subNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (MIM.Get(i)->GetObject<Ipv4>());
    for (int j = 1; j < MIM.Get(i)->GetNDevices(); j++){
         int cc = 1;
         Ptr< NetDevice > level1 = MIM.Get(i)->GetDevice(j);
         for (int x = 1; x < level1->GetNode()->GetNDevices(); x++){
              Ptr< NetDevice > level2 = level1->GetNode()->GetDevice(j);
              subNodeStaticRouting->AddNetworkRouteTo(level2->GetNode()->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), Ipv4Mask("255.255.0.0"), level1->GetNode()->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), 1, 0);
         }
    }
  }

  //SubNodes to CC
  for (int i = 0; i < subNodes.GetN(); i++){
    int cc = 0;
    Ptr<Ipv4StaticRouting> subNodeStaticRouting3 = ipv4RoutingHelper.GetStaticRouting (subNodes.Get(i)->GetObject<Ipv4>());
    for (int j = 1; j < subNodes.Get(i)->GetNDevices(); j++){
         int cc = 1;
         Ptr< NetDevice > level1 = subNodes.Get(i)->GetDevice(j);
         for (int x = 1; x < level1->GetNode()->GetNDevices(); x++){
              Ptr< NetDevice > level2 = level1->GetNode()->GetDevice(j);
              subNodeStaticRouting3->AddNetworkRouteTo(level2->GetNode()->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), Ipv4Mask("255.255.0.0"), level1->GetNode()->GetObject<Ipv4>()->GetAddress(cc,0).GetLocal(), 1, 0);
         }
    }
  }
}

void changeRoute (std::vector<NodeContainer> nodes, Ipv4Address gateway, int index, std::string fileID) {
          std::cout << "Getting the available interfaces" << std::endl;
          Ipv4StaticRoutingHelper ipv4RoutingHelper;
          //for (int index = 1; index < ueNodes.GetN()+1; index ++){
          Ptr<Ipv4> ipv4_2 = nodes[1].Get(0)->GetObject<Ipv4>();
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
          //string temp(ss1.str().back());
          std::cout << "I am " << ip[1] << std::endl;
          string temp = ss1.str(); //.substr(ss1.str().length() - 2);
          if (ss1.str().length() > 2){
                temp = ss1.str().substr(ss1.str().length() - 2);
          }
          int ID = 0;

          int NewIndex = 1 + (rand() % int(nodes[1].GetN()));//random - (rand() % int(ueNodes.GetN()/2));
          if (NewIndex > nodes[1].GetN()){
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
          std::ifstream infile("/people/belo700/RD2C/RC/integration/control/AgentDecisions11.txt"); //fileID.c_str());
          //std::fstream infile;
          //infile.open(fileID.c_str(), ios::in);
          //if (infile.is_open()) {
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
               int ID = std::stoi(token)-1;
               int NewIndex = std::stoi(line)+1;
               for (int x = 0; x < nodes[1].GetN() ; x++){
                   nodes[1].Get(ID)->GetObject<Ipv4>()->SetUp(x+1);
               }
               std::cout << "LOOOOOOOOKKKKKEEEE BELOW!!!" << std::endl;
               std::cout << NewIndex << std::endl;
               std::cout << interface[ID] << std::endl;
               if (NewIndex != interface[ID]){
		       previous[ID] = interface[ID];
                    nodes[1].Get(ID)->GetObject<Ipv4>()->SetDown(interface[ID]);
                    interface[ID] = NewIndex;
                }
           }
	  Simulator::Schedule(MilliSeconds(period_routing), changeRoute, nodes, gateway, nextNode_ind, fileID);
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

  HelicsHelper helicsHelper(std::stoi(helicsConfigObject["brokerPort"].asString()));
  std::cout << "Calling Calling Message Federate Constructor" << std::endl;
  helicsHelper.SetupApplicationFederate();

  std::string fedName = helics_federate->getName();

   simTime = Seconds(std::stof(configObject["Simulation"][0]["SimTime"].asString()));
   float start = std::stof(configObject["Simulation"][0]["StartTime"].asString());
   includeMIM = std::stoi(configObject["Simulation"][0]["includeMIM"].asString());
   numBots = std::stoi(configObject["DDoS"][0]["NumberOfBots"].asString());
   int numThreads = std::stoi(configObject["DDoS"][0]["threadsPerAttacker"].asString());

 /* Dnp3SimulatorImpl *hb=new Dnp3SimulatorImpl();
  Ptr<Dnp3SimulatorImpl> hb2(hb);
  hb->Unref();
  Simulator::SetImplementation(hb2);
*/
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


  PointToPointHelper p2ph2;
  std::string rate = configObject["DDoS"][0]["Rate"].asString();
  p2ph2.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (rate)));
  //p2ph2.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (topologyConfigObject["Channel"][0]["P2PRate"].asString())));
  p2ph2.SetDeviceAttribute ("Mtu", UintegerValue (std::stoi(configObject["DDoS"][0]["PacketSize"].asString())));
  p2ph2.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (std::stoi(topologyConfigObject["Channel"][0]["delay"].asString()))));

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
  numNodePairs = std::stoi(topologyConfigObject["5GSetup"][0]["numUE"].asString());
  enbNodes.Create (numNodePairs);
  ueNodes.Create (numNodePairs);

  NodeContainer botNodes;
  botNodes.Create(numBots);

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

  //MobilityHelper ueMob;
  //ueMob.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
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
  int numSSPUE = configObject["microgrid"].size(); //10;
  NodeContainer subNodes;
  subNodes.Create (numSSPUE);
  internet.Install (subNodes);

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
       NodeContainer csmaSubNodes_temp (ueNodes.Get(i%ueNodes.GetN()), MIM.Get(i), subNodes.Get(j));
       csmaSubNodes.push_back(csmaSubNodes_temp);
    }
  }

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  csma.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

  Ipv4InterfaceContainer inter;
  Ipv4InterfaceContainer inter_MIM;
  for (int i = 0; i < csmaSubNodes.size(); i++){
    NetDeviceContainer internetDevicesSub = csma.Install (csmaSubNodes[i]);

    Ipv4AddressHelper ipv4Sub;
    std::string address = "172."+std::to_string(i+17)+".0.0";
    ipv4Sub.SetBase (address.c_str(), "255.255.0.0", "0.0.0.1");
    Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign (internetDevicesSub);

    inter.Add(interfacesSub.Get(2));
    inter_MIM.Add(interfacesSub.Get(1));
  }

  /*Ipv4InterfaceContainer inter;
  Ipv4InterfaceContainer inter_MIM;
  std::stringstream addrTrans;
  for (int i = 0; i < csmaSubNodes.size(); i++){
    NetDeviceContainer internetDevicesSub = csma.Install (csmaSubNodes[i]);

    //Assign IP Address
    Ipv4AddressHelper ipv4Sub;
    std::string address = "172."+std::to_string(17+i)+".0.0";
    ipv4Sub.SetBase (address.c_str(), "255.255.0.0", "0.0.0.1");
    Ipv4InterfaceContainer interfacesSub = ipv4Sub.Assign (internetDevicesSub);
    addrTrans << csmaSubNodes[i].Get(2)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << ": " << csmaSubNodes[i].Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << endl;

    inter.Add(interfacesSub.Get(2));
    inter_MIM.Add(interfacesSub.Get(1));
  }
  std::cout << "The translation table" << std::endl;
  std::cout << addrTrans.str().c_str() << std::endl;
  
  FILE * addFile;
  addFile = fopen ("/rd2c/integration/control/add.txt","w");
  if (addFile!=NULL)
  {
	  fprintf(addFile, addrTrans.str().c_str());
	  fclose (addFile);
  }*/

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

  //Ptr<Node> ueNodeZero = ueNodes.Get (0);
  //Ptr<Ipv4> ipv4 = ueNodeZero->GetObject<Ipv4> (); 


  //Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  /*for (int i = 0; i < ueNodes.GetN(); i++){
      for (int x = 0; x < subNodes.GetN()/ueNodes.GetN(); x++){
          remoteHostStaticRouting->AddNetworkRouteTo (ueNodes.Get(i)->GetObject<Ipv4>()->GetAddress(2+x,0).GetLocal(), Ipv4Mask ("255.0.0.0"),gateway, 1);
      }
  }*/
  /*for (int i = 0; i < subNodes.GetN(); i ++){
    remoteHostStaticRouting->AddNetworkRouteTo (subNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), ueNodes.Get(i%ueNodes.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(),1); //gateway, 1);
  }

  for (int i = 0; i < MIM.GetN(); i++){
    Ipv4Address addr3_ = MIM.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
    Ptr<Ipv4StaticRouting> ueNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNodes.Get(i%ueNodes.GetN())->GetObject<Ipv4>());
    ueNodeStaticRouting->AddNetworkRouteTo(subNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask("255.255.0.0"), MIM.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 2+int(i/ueNodes.GetN()));
    Ipv4Address addr2_ = ueNodes.Get(i%ueNodes.GetN())->GetObject<Ipv4>()->GetAddress (2+int(i/ueNodes.GetN()), 0).GetLocal ();
    Ptr<Ipv4StaticRouting> subNodeStaticRouting = ipv4RoutingHelper.GetStaticRouting (MIM.Get(i)->GetObject<Ipv4>());
    for (int j = 0; j < remoteHostContainer.GetN(); j++){
        subNodeStaticRouting->AddNetworkRouteTo (remoteHostContainer.Get(j)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.0.0.0"), addr2_, 1);
    }
    subNodeStaticRouting->AddNetworkRouteTo (subNodes.Get(i)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.0.0.0"), 1);
  }

  for (int i = 0; i < subNodes.GetN(); i++){
    Ptr<Ipv4> ipv4_2 = MIM.Get(i)->GetObject<Ipv4>();
    Ipv4Address addr5_ = ipv4_2->GetAddress(1,0).GetLocal();
    Ptr<Ipv4StaticRouting> subNodeStaticRouting3 = ipv4RoutingHelper.GetStaticRouting (subNodes.Get(i)->GetObject<Ipv4>());
    for (int j = 0; j < remoteHostContainer.GetN(); j++){
        subNodeStaticRouting3->AddNetworkRouteTo (remoteHostContainer.Get(j)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.0.0.0"), addr5_, 1);
    }
  }*/
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

  /*NetDeviceContainer botDeviceContainer[numBots];
  for (int i = 0; i < numBots; ++i)
  {
	  botDeviceContainer[i] = p2ph.Install(botNodes.Get(i), remoteHostContainer.Get(0));//We are currently attacking the remoteHost but I will need to change that in the future to be dynamic
  }

  internet.Install(botNodes);
  Ipv4AddressHelper ipv4_n;
  ipv4_n.SetBase("30.0.0.0", "255.255.255.252");

  for (int j = 0; j < numBots; ++j)
  {
	  ipv4_n.Assign(botDeviceContainer[j]);
	  ipv4_n.NewNetwork();
  }*/
  NetDeviceContainer botDeviceContainer[numBots];
  for (int i = 0; i < numBots; ++i)
  {
	  if (configObject["DDoS"][0]["NodeType"][0].asString().find("CC") != std::string::npos){
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), remoteHostContainer.Get(0));
	  }else if (configObject["DDoS"][0]["NodeType"][0].asString().find("UE") != std::string::npos){
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), ueNodes.Get(int(((i%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString())))%ueNodes.GetN()));
	  }else if (configObject["DDoS"][0]["NodeType"][0].asString().find("subNode") != std::string::npos){
              botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), subNodes.Get(int(((i%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString())))%subNodes.GetN()));
          }else{
	      botDeviceContainer[i] = p2ph2.Install(botNodes.Get(i), MIM.Get(int(((i%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%MIM.GetN()))); //remoteHostContainer.Get(0));//We are currently attacking the remoteHost but I will need to change that in the future to be dynamic
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
  //Ptr<Ipv4> ip2 = MIM.Get(1)->GetObject<Ipv4>();
  //ip2->GetObject<Ipv4L3ProtocolMIM> ()->victimAddr = remoteHostAddr;
  //ip->GetObject<Ipv4L3ProtocolMIM> ()->sourceAddr = Ipv4Address("7.0.0.2");
  uint16_t port = 20000;
  uint16_t master_port = 40000;
  ApplicationContainer dnpOutstationApp, dnpMasterApp;
  std::vector<uint16_t> mimPort;
  /*
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
	     dnp3Master.SetAttribute("RemoteAddress", AddressValue(inter.GetAddress(i)));//star.GetSpokeIpv4Address (i)));
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
	     
	     Dnp3ApplicationHelperNew dnp3Outstation ("ns3::UdpSocketFactory", InetSocketAddress (inter.GetAddress(i), port)); //star.GetSpokeIpv4Address (i), port));
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
	     Simulator::Schedule(MilliSeconds(1005), &Dnp3ApplicationNew::periodic_poll, master, 0);
	     Simulator::Schedule(MilliSeconds(3005), &Dnp3ApplicationNew::send_control_analog, master, 
			           Dnp3ApplicationNew::DIRECT, 0, -16);
	     master_port += 1;
	     
      }
  */
  for (i = 0;i < configObject["microgrid"].size();i++)
      {
	    //if (systemId == i){
	    std::string delimiter = ".";
            std::ostringstream ss;
            ss << subNodes.Get(i)->GetObject<Ipv4>()->GetAddress(i+1,0).GetLocal();//ipHeader.GetSource();
            std::vector<std::string> ip;
            std::string ip_ = ss.str();
            size_t pos = 0;
            while ((pos = ip_.find(delimiter)) != std::string::npos) {
               ip.push_back(ip_.substr(0, pos));
               ip_.erase(0, pos + delimiter.length());
            }

            std::stringstream ss1;
            ss1 << ip[1];
            //int IDx;
            //ss1 >> IDx;
            std::cout << "I am " << ip[1] << std::endl;
	    string temp = ss1.str();
	    /*if (ss1.str().length() > 2){
                temp = ss1.str().substr(ss1.str().length() - 2);
	    }*/
	    /*std::string::size_type i2 = temp.find(".");
            std::string s = ".";
            if (i2 != std::string::npos){
              temp.erase(i2, s.length());
            }*/
            std::stringstream ss2;
            ss2 << temp;
            int ID2;
            ss2 >> ID2;
            interface[i] = i+1;

            mimPort.push_back(master_port);
            auto ep_name = configObject["microgrid"][i]["name"].asString();
	    std::string IDx = "SS_";
            if (std::string(ep_name).find(IDx) != std::string::npos){
                ep_name = "SS_"+std::to_string(i+1);
            }
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
             //			           Dnp3ApplicationNew::DIRECT, 0, -16);
	     master_port += 1;
	    //}
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
		   dnp3MIM1.SetAttribute ("ID", UintegerValue(MIM_ID));
                   dnp3MIM1.SetAttribute ("AttackConf", StringValue(configFileName));
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
    int UDP_SINK_PORT = mimPort[2]-10;
    int MAX_BULK_BYTES = std::stof(configObject["DDoS"][0]["PacketSize"].asString()); //20971520000;
    std::string DDOS_RATE = configObject["DDoS"][0]["Rate"].asString(); //"2000kb/s";
    bool DDoS = std::stoi(configObject["DDoS"][0]["Active"].asString());

    if (DDoS){
	    Simulator::Schedule(Seconds(BOT_START), StartEndSignal, "started");
            Simulator::Schedule(Seconds(BOT_STOP), StartEndSignal, "ended");
            ApplicationContainer onOffApp[botNodes.GetN()];
            for (int k = 0; k < botNodes.GetN(); ++k)
            {
                Ptr<Node> tempnode = ueNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%ueNodes.GetN());
                int intID = 1;
                if (configObject["DDoS"][0]["NodeType"][0].asString().find("CC") != std::string::npos){
                    tempnode = remoteHostContainer.Get(0);
                }else if (configObject["DDoS"][0]["NodeType"][0].asString().find("MIM") != std::string::npos){
                    tempnode = MIM.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%MIM.GetN());
                }else if (configObject["DDoS"][0]["NodeType"][0].asString().find("subNode") != std::string::npos){
                    tempnode = subNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%subNodes.GetN());
                }

                Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (botNodes.Get(k)->GetObject<Ipv4> ());
                if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
                    remoteHostStaticRouting->AddNetworkRouteTo (subNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%subNodes.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), tempnode->GetObject<Ipv4>()->GetAddress(intID,0).GetLocal(),1); //gateway, 1);
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
                    remoteHostStaticRouting->AddNetworkRouteTo (MIM.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%MIM.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), tempnode->GetObject<Ipv4>()->GetAddress(intID,0).GetLocal(),1); //gateway, 1);
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("UE") != std::string::npos){
                    remoteHostStaticRouting->AddNetworkRouteTo (ueNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%ueNodes.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), Ipv4Mask ("255.255.0.0"), tempnode->GetObject<Ipv4>()->GetAddress(intID,0).GetLocal(),1); //gateway, 1);
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                    remoteHostStaticRouting->AddNetworkRouteTo (remoteHostAddr, Ipv4Mask ("255.255.0.0"), tempnode->GetObject<Ipv4>()->GetAddress(intID,0).GetLocal(),1); //gateway, 1);
                }


                if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
			InetSocketAddress dst = InetSocketAddress(subNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%subNodes.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                    OnOffHelper onoff = OnOffHelper ("ns3::Ipv4RawSocketFactory", dst);
                    onoff.SetConstantRate(DataRate(DDOS_RATE));
                    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                    onOffApp[k] = onoff.Install(botNodes.Get(k));
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
                    InetSocketAddress dst = InetSocketAddress(MIM.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%MIM.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                    OnOffHelper onoff = OnOffHelper ("ns3::Ipv4RawSocketFactory", dst);
                    onoff.SetConstantRate(DataRate(DDOS_RATE));
                    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                    onOffApp[k] = onoff.Install(botNodes.Get(k));
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("UE") != std::string::npos){
                    InetSocketAddress dst = InetSocketAddress(ueNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%ueNodes.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                    OnOffHelper onoff = OnOffHelper ("ns3::Ipv4RawSocketFactory", dst);
                    onoff.SetConstantRate(DataRate(DDOS_RATE));
                    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                    onOffApp[k] = onoff.Install(botNodes.Get(k));
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                    InetSocketAddress dst = InetSocketAddress(remoteHostAddr);
                    OnOffHelper onoff = OnOffHelper ("ns3::Ipv4RawSocketFactory", dst);
                    onoff.SetConstantRate(DataRate(DDOS_RATE));
                    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_on_time+"]"));
                    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant="+str_off_time+"]"));
                    onOffApp[k] = onoff.Install(botNodes.Get(k));
                }
                onOffApp[k].Start(Seconds(BOT_START));
                onOffApp[k].Stop(Seconds(BOT_STOP));
		if (configObject["DDoS"][0]["endPoint"].asString().find("subNode") != std::string::npos){
                        InetSocketAddress dst = InetSocketAddress (subNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%subNodes.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                        PacketSinkHelper UDPsink = PacketSinkHelper ("ns3::Ipv4RawSocketFactory", dst);
                        ApplicationContainer UDPSinkApp = UDPsink.Install(subNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%subNodes.GetN())); //remoteHost);
                        UDPSinkApp.Start(Seconds(BOT_START));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));
                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("MIM") != std::string::npos){
                        InetSocketAddress dst = InetSocketAddress (MIM.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%MIM.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                        PacketSinkHelper UDPsink = PacketSinkHelper ("ns3::Ipv4RawSocketFactory", dst);
                        ApplicationContainer UDPSinkApp = UDPsink.Install(MIM.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%MIM.GetN())); //remoteHost);
                        UDPSinkApp.Start(Seconds(BOT_START));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));

                }
                else if (configObject["DDoS"][0]["endPoint"].asString().find("UE") != std::string::npos){
                        InetSocketAddress dst = InetSocketAddress (ueNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%ueNodes.GetN())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
                        PacketSinkHelper UDPsink = PacketSinkHelper ("ns3::Ipv4RawSocketFactory", dst);
                        ApplicationContainer UDPSinkApp = UDPsink.Install(ueNodes.Get(((k%numThreads)+std::stoi(configObject["DDoS"][0]["NodeID"][0].asString()))%ueNodes.GetN())); //remoteHost);
                        UDPSinkApp.Start(Seconds(BOT_START));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));

                }
            }
            if (configObject["DDoS"][0]["endPoint"].asString().find("CC") != std::string::npos){
                        InetSocketAddress dst = InetSocketAddress (remoteHostAddr);
                        PacketSinkHelper UDPsink = PacketSinkHelper ("ns3::Ipv4RawSocketFactory", dst);
                        ApplicationContainer UDPSinkApp = UDPsink.Install(remoteHost);
                        UDPSinkApp.Start(Seconds(BOT_START));
                        UDPSinkApp.Stop(Seconds(BOT_STOP));

            }
            UDP_SINK_PORT += 1;
    }
    std::cout << "Done Setting up the bots " << std::endl;

    /*AsciiTraceHelper ascii;
    p2ph.EnableAsciiAll (ascii.CreateFileStream ("edge_performance.tr"));
    Simulator::Schedule (Seconds (0.2), &trTocsv);
    //csma.EnableAsciiAll (ascii.CreateFileStream ("edge_performance_csma.tr"));


    flowMonitor = flowHelper.InstallAll();
    flowMonitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    flowMonitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    flowMonitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));
    int mon = std::stoi(configObject["Simulation"][0]["MonitorPerf"].asString());
    if (mon){
        Simulator::Schedule (Seconds (0.2), &Throughput);
    }
    if (DDoS){
	    p2ph.EnablePcapAll (pcapFileDir+"p2p-DDoS", false);
    }else{
	    p2ph.EnablePcapAll (pcapFileDir+"p2p", false);
    }

  lteHelper->EnableTraces ();
  enablePcapAllBaseTime("radics-exercise2-utility1-1day", remoteHostContainer, ncP2P_nodes);*/
  flowMonitor = flowHelper.InstallAll(); //flowHelper.Install(endpointNodes);
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

  Simulator::Stop (simTime);
  Simulator::Run ();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy ();
  return 0;
}
