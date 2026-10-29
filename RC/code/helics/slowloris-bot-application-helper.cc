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
 * Helper implementation for SlowlorisBotApplication.
 *
 * Author: Kenneth Watts (ken.watts@gmail.com)
 *
 * Portions of this file were drafted with AI assistance (Claude,
 * Anthropic) and reviewed/adapted by the author.
 */

#include "slowloris-bot-application-helper.h"
#include "ns3/uinteger.h"
#include "ns3/address.h"

namespace ns3 {

SlowlorisBotApplicationHelper::SlowlorisBotApplicationHelper (Address address, uint16_t port)
{
  m_factory.SetTypeId ("ns3::SlowlorisBotApplication");
  m_factory.Set ("RemoteAddress", AddressValue (address));
  m_factory.Set ("RemotePort", UintegerValue (port));
}

void
SlowlorisBotApplicationHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
SlowlorisBotApplicationHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
SlowlorisBotApplicationHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }
  return apps;
}

Ptr<SlowlorisBotApplication>
SlowlorisBotApplicationHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<SlowlorisBotApplication> app = m_factory.Create<SlowlorisBotApplication> ();
  node->AddApplication (app);
  return app;
}

} // namespace ns3
