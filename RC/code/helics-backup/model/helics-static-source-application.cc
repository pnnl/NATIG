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
#include "ns3/helics-static-source-application.h"
#include "helics/helics.hpp"

#include <algorithm>
#include <sstream>
#include <string>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HelicsStaticSourceApplication");

NS_OBJECT_ENSURE_REGISTERED (HelicsStaticSourceApplication);

TypeId
HelicsStaticSourceApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HelicsStaticSourceApplication")
    .SetParent<HelicsApplication> ()
    .AddConstructor<HelicsStaticSourceApplication> ()
    .AddAttribute ("Destination", 
                   "The destination of outgoing helics messages",
                   StringValue (),
                   MakeStringAccessor (&HelicsStaticSourceApplication::m_destination),
                   MakeStringChecker ())
  ;
  return tid;
}

HelicsStaticSourceApplication::HelicsStaticSourceApplication (void)
{
  NS_LOG_FUNCTION (this);
}

HelicsStaticSourceApplication::~HelicsStaticSourceApplication (void)
{
  NS_LOG_FUNCTION (this);
}

void
HelicsStaticSourceApplication::SetDestination (const std::string &destination)
{
  NS_LOG_FUNCTION (this << destination);

  m_destination = destination;
}

std::string
HelicsStaticSourceApplication::GetDestination (void) const
{
  NS_LOG_FUNCTION (this);

  return m_destination;
}

void
HelicsStaticSourceApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::DoDispose ();
}

void 
HelicsStaticSourceApplication::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::StartApplication();

  if (m_destination.empty()) {
    NS_FATAL_ERROR ("HelicsStaticSourceApplication is missing destination");
  }
}

void 
HelicsStaticSourceApplication::StopApplication (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::StopApplication();
}

void
HelicsStaticSourceApplication::DoFilter (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string());

  NS_FATAL_ERROR ("HelicsStaticSourceApplication should not filter messages");
}

void
HelicsStaticSourceApplication::DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << id.getName() << time << message->to_string());

  NS_FATAL_ERROR ("HelicsStaticSourceApplication should not receive endpoint events");
}

void
HelicsStaticSourceApplication::DoRead (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string());

  NS_LOG_INFO ("sending message on to " << m_destination);

  helics_federate->sendMessage (m_endpoint_id, m_destination, message->data.data(), message->data.size());
}

} // Namespace ns3
