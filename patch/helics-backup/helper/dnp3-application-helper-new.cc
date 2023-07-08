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

#include "dnp3-application-helper-new.h"
#include "ns3/string.h"
#include "ns3/inet-socket-address.h"
#include "ns3/names.h"
#include "ns3/dnp3-application-new.h"
#include "ns3/pointer.h"
#include "ns3/boolean.h"

using namespace std;

namespace ns3 {

Dnp3ApplicationHelperNew::Dnp3ApplicationHelperNew (std::string protocol, Address address)
{
  m_factory.SetTypeId ("ns3::Dnp3ApplicationNew");
  m_factory.Set ("Protocol", StringValue (protocol));
  m_factory.Set ("LocalAddress", AddressValue (address));
}

void
Dnp3ApplicationHelperNew::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
Dnp3ApplicationHelperNew::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
Dnp3ApplicationHelperNew::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
Dnp3ApplicationHelperNew::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Dnp3ApplicationNew>
Dnp3ApplicationHelperNew::Install (Ptr<Node> node, const std::string &name)
{
  cout << "App name" << name << endl;
  return InstallPriv (node, name);
}

Ptr<Dnp3ApplicationNew>
Dnp3ApplicationHelperNew::InstallPriv (Ptr<Node> node) const
{
  Ptr<Dnp3ApplicationNew> app = m_factory.Create<Dnp3ApplicationNew> ();
  BooleanValue ptr;
  app->GetAttribute ("isMaster", ptr);
  StringValue name;
  app->GetAttribute ("Name", name);
  app->SetEndpointName (name.Get(), false);

  node->AddApplication (app);

  return app;
}

Ptr<Dnp3ApplicationNew>
Dnp3ApplicationHelperNew::InstallPriv (Ptr<Node> node, const std::string &name)
{
  Ptr<Dnp3ApplicationNew> app = m_factory.Create<Dnp3ApplicationNew> ();
  BooleanValue ptr;
  app->GetAttribute ("isMaster", ptr);
  
  app->SetEndpointName (name, false);
  node->AddApplication (app);

  return app;
}

} // namespace ns3
