/*
Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See the top-level NOTICE for additional details. All rights reserved.

SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef HELICS_STATIC_SINK_APPLICATION_H
#define HELICS_STATIC_SINK_APPLICATION_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable-stream.h"

#include <map>
#include <string>

#include "helics-id-tag.h"
#include "helics-application.h"
#include "helics/helics.hpp"

namespace ns3 {

class Socket;
class Packet;
class InetSocketAddress;
class Inet6SocketAddress;

/**
 * \ingroup helicsapplication
 * \brief A Helics Application
 *
 * Every packet sent should be returned by the server and received here.
 */
class HelicsStaticSinkApplication : public HelicsApplication
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  HelicsStaticSinkApplication ();

  virtual ~HelicsStaticSinkApplication ();

  /**
   * \brief set the name of the destination node
   * \param name name
   */
  void SetDestination (const std::string &name);

  std::string GetDestination (void) const;

  virtual void DoEndpoint (helics::Endpoint id, helics::Time time, std::unique_ptr<helics::Message> message) override;

protected:
  virtual void DoDispose (void) override;
  virtual void StartApplication (void) override;
  virtual void StopApplication (void) override;
  virtual void DoFilter (std::unique_ptr<helics::Message> message) override;
  virtual void DoRead (std::unique_ptr<helics::Message> message) override;
  
  std::string m_destination; //!< name of this application
};

} // namespace ns3

#endif /* HELICS_STATIC_SINK_APPLICATION_H */
