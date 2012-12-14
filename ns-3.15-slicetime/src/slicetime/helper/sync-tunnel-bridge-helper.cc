/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 * Copyright (c) 2009 RWTH Aachen University
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
 * adaptions by: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

// this file is adapted from helpers/tap-bridge-helper.cc

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/enum.h"
#include "ns3/sync-tunnel-bridge.h"
#include "ns3/names.h"
#include "sync-tunnel-bridge-helper.h"

NS_LOG_COMPONENT_DEFINE ("SyncTunnelBridgeHelper");

namespace ns3 {

SyncTunnelBridgeHelper::SyncTunnelBridgeHelper ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.SetTypeId ("ns3::SyncTunnelBridge");
}

void 
SyncTunnelBridgeHelper::SetAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (n1 << &v1);
  m_deviceFactory.Set (n1, v1);
}


  Ptr<NetDevice>
SyncTunnelBridgeHelper::Install (Ptr<Node> node, Ptr<NetDevice> nd)
{
  NS_LOG_FUNCTION (node << nd);
  NS_LOG_LOGIC ("Install SyncTunnelBridge on node " << node->GetId () << " bridging net device " << nd);

  Ptr<SyncTunnelBridge> bridge = m_deviceFactory.Create<SyncTunnelBridge> ();
  node->AddDevice (bridge);
  bridge->SetBridgedNetDevice (nd);

  return bridge;
}

} // namespace ns3
