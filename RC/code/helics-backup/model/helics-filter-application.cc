/*
Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See the top-level NOTICE for additional details. All rights reserved.

SPDX-License-Identifier: BSD-3-Clause
*/

#include <iostream>
#include <fstream>
#include <vector>

#include "ns3/log.h"
#include "ns3/string.h"

#include "ns3/helics.h"
#include "ns3/helics-application.h"
#include "ns3/helics-filter-application.h"
#include "helics/helics.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HelicsFilterApplication");

NS_OBJECT_ENSURE_REGISTERED (HelicsFilterApplication);

TypeId
HelicsFilterApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HelicsFilterApplication")
    .SetParent<HelicsApplication> ()
    .AddConstructor<HelicsFilterApplication> ()
  ;
  return tid;
}

HelicsFilterApplication::HelicsFilterApplication ()
{
  NS_LOG_FUNCTION (this);
}

HelicsFilterApplication::~HelicsFilterApplication()
{
  NS_LOG_FUNCTION (this);
}

void
HelicsFilterApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::DoDispose ();
}

void 
HelicsFilterApplication::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::StartApplication();
}

void 
HelicsFilterApplication::StopApplication (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::StopApplication();
}

void
HelicsFilterApplication::DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << id.getName() << time << message->to_string());

  Send (message->original_dest, time, std::make_unique<helics::Message> (*message));
}

void
HelicsFilterApplication::DoRead (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string());

  message->dest = message->original_dest;
  helics_federate->sendMessage (m_endpoint_id, std::move (message));
}

} // Namespace ns3
