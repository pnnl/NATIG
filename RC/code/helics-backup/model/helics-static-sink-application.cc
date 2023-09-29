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
#include "ns3/helics-static-sink-application.h"
#include "helics/helics.hpp"

#include <algorithm>
#include <sstream>
#include <string>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HelicsStaticSinkApplication");

NS_OBJECT_ENSURE_REGISTERED (HelicsStaticSinkApplication);

TypeId
HelicsStaticSinkApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HelicsStaticSinkApplication")
    .SetParent<HelicsApplication> ()
    .AddConstructor<HelicsStaticSinkApplication> ()
    .AddAttribute ("Destination", 
                   "The destination of incoming helics messages",
                   StringValue (),
                   MakeStringAccessor (&HelicsStaticSinkApplication::m_destination),
                   MakeStringChecker ())
  ;
  return tid;
}

HelicsStaticSinkApplication::HelicsStaticSinkApplication ()
{
  NS_LOG_FUNCTION (this);
}

HelicsStaticSinkApplication::~HelicsStaticSinkApplication()
{
  NS_LOG_FUNCTION (this);
}

void
HelicsStaticSinkApplication::SetDestination (const std::string &destination)
{
  NS_LOG_FUNCTION (this << destination);
  m_destination = destination;
}

std::string
HelicsStaticSinkApplication::GetDestination (void) const
{
  return m_destination;
}

void
HelicsStaticSinkApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::DoDispose ();
}

void 
HelicsStaticSinkApplication::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::StartApplication();

  if (m_destination.empty()) {
    NS_FATAL_ERROR ("HelicsStaticSinkApplication is missing destination");
  }
}

void 
HelicsStaticSinkApplication::StopApplication (void)
{
  NS_LOG_FUNCTION (this);

  HelicsApplication::StopApplication();
}

void
HelicsStaticSinkApplication::DoFilter (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string());

  NS_FATAL_ERROR ("HelicsStaticSinkApplication should not filter messages");
}

void
HelicsStaticSinkApplication::DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << id.getName() << time << message->to_string());

  Send(m_destination, time, std::move (message));
}

void
HelicsStaticSinkApplication::DoRead (std::unique_ptr<helics::Message> message)
{
  NS_LOG_FUNCTION (this << message->to_string());

  NS_FATAL_ERROR ("HelicsStaticSinkApplication should not read from socket");
}

} // Namespace ns3
