/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See the top-level NOTICE for additional details. All rights reserved.

SPDX-License-Identifier: BSD-3-Clause
*/

#include "helics.h"

namespace ns3 {

std::shared_ptr<helics::CombinationFederate> helics_federate;
helics::Endpoint helics_endpoint;

std::ostream& operator << (std::ostream& stream, const helics::Message &message)
{
  stream << "Message(" << message.time
      << ", " << message.flags
      << ", " << message.data.size() << " bytes"
      << ", " << message.source
      << ", " << message.dest
      << ", " << message.original_source
      << ", " << message.original_dest
      << ")";
  return stream;
}

std::ostream& operator << (std::ostream& stream, std::unique_ptr<helics::Message> message)
{
  stream << "Message(" << message->time
      << ", " << message->flags
      << ", " << message->data.size() << " bytes"
      << ", " << message->source
      << ", " << message->dest
      << ", " << message->original_source
      << ", " << message->original_dest
      << ")";
  return stream;
}

}

