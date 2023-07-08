/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See the top-level NOTICE for additional details. All rights reserved.

SPDX-License-Identifier: BSD-3-Clause
*/

#include "ns3/core-module.h"
#include "ns3/helics-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HelicsExample");

int 
main (int argc, char *argv[])
{
  bool verbose = true;

  HelicsHelper helics;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  helics.SetupCommandLine(cmd);
  cmd.Parse (argc,argv);

  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::HelicsSimulatorImpl"));

  helics.SetupApplicationFederate();
  NS_LOG_INFO ("Simulator Impl bound, about to Run simulator");

  Simulator::Run ();
  Simulator::Stop (Seconds (10.0));
  Simulator::Destroy ();
  return 0;
}


