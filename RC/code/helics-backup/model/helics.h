/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See the top-level NOTICE for additional details. All rights reserved.

SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef HELICS_H
#define HELICS_H

#include <ostream>
#include <memory>

#include "helics/helics.hpp"

namespace ns3 {

extern std::shared_ptr<helics::CombinationFederate> helics_federate;
extern helics::Endpoint helics_endpoint;

std::ostream& operator << (std::ostream& stream, const helics::Message &message);

std::ostream& operator << (std::ostream& stream, std::unique_ptr<helics::Message> message);

}

#endif /* HELICS_H */

