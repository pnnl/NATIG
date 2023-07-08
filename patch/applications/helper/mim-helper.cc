/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "mim-helper.h"
#include "ns3/mim-server.h"
#include "ns3/mim-client.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"

namespace ns3 {

MIMServerHelper::MIMServerHelper (uint16_t port)
{
  m_factory.SetTypeId (MIMServer::GetTypeId ());
  SetAttribute ("Port", UintegerValue (port));
}

void
MIMServerHelper::SetAttribute (
  std::string name,
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
MIMServerHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MIMServerHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MIMServerHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
MIMServerHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<MIMServer> ();
  node->AddApplication (app);

  return app;
}

MIMClientHelper::MIMClientHelper (Address address, uint16_t port)
{
  m_factory.SetTypeId (MIMClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (address));
  SetAttribute ("RemotePort", UintegerValue (port));
}

MIMClientHelper::MIMClientHelper (Address address)
{
  m_factory.SetTypeId (MIMClient::GetTypeId ());
  SetAttribute ("RemoteAddress", AddressValue (address));
}

void
MIMClientHelper::SetAttribute (
  std::string name,
  const AttributeValue &value)
{
  m_factory.Set (name, value);
}

void
MIMClientHelper::SetFill (Ptr<Application> app, std::string fill)
{
  app->GetObject<MIMClient>()->SetFill (fill);
}

void
MIMClientHelper::SetFill (Ptr<Application> app, uint8_t fill, uint32_t dataLength)
{
  app->GetObject<MIMClient>()->SetFill (fill, dataLength);
}

void
MIMClientHelper::SetFill (Ptr<Application> app, uint8_t *fill, uint32_t fillLength, uint32_t dataLength)
{
  app->GetObject<MIMClient>()->SetFill (fill, fillLength, dataLength);
}

ApplicationContainer
MIMClientHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MIMClientHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MIMClientHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
MIMClientHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<MIMClient> ();
  node->AddApplication (app);

  return app;
}

} // namespace ns3
