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
 * Helper for SlowlorisBotApplication. Standard ns-3 ObjectFactory-based
 * helper pattern, mirroring modbus-application-helper-new.h.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#ifndef SLOWLORIS_BOT_APPLICATION_HELPER_H
#define SLOWLORIS_BOT_APPLICATION_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/slowloris-bot-application.h"

namespace ns3 {

/**
 * \ingroup applications
 * \brief A helper to make it easier to instantiate a SlowlorisBotApplication
 * on a set of nodes.
 */
class SlowlorisBotApplicationHelper
{
public:
  /**
   * \param address the victim's address (sets the RemoteAddress attribute).
   * \param port the victim's listening port (sets the RemotePort attribute).
   */
  SlowlorisBotApplicationHelper (Address address, uint16_t port);

  void SetAttribute (std::string name, const AttributeValue &value);

  ApplicationContainer Install (NodeContainer c) const;
  ApplicationContainer Install (Ptr<Node> node) const;

private:
  Ptr<SlowlorisBotApplication> InstallPriv (Ptr<Node> node) const;

  ObjectFactory m_factory;
};

} // namespace ns3

#endif /* SLOWLORIS_BOT_APPLICATION_HELPER_H */
