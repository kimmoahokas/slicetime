/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 * Copyright (c) 2010 RWTH Aachen University
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

// this file is an adapted version of tap-bridge-helper.cc

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/enum.h"
#include "ns3/wifi-emu-bridge.h"
#include "ns3/names.h"
#include "wifi-emu-bridge-helper.h"

NS_LOG_COMPONENT_DEFINE ("WifiEmuBridgeHelper");

namespace ns3 {

WifiEmuBridgeHelper::WifiEmuBridgeHelper ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.SetTypeId ("ns3::WifiEmuBridge");
}

void 
WifiEmuBridgeHelper::SetAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (n1 << &v1);
  m_deviceFactory.Set (n1, v1);
}

  Ptr<NetDevice>
WifiEmuBridgeHelper::Install (Ptr<Node> node, Ptr<NetDevice> nd)
{
  NS_LOG_FUNCTION (node << nd);
  NS_LOG_LOGIC ("Install WifiEmuBridge on node " << node->GetId () << " bridging net device " << nd);

  Ptr<WifiEmuBridge> bridge = m_deviceFactory.Create<WifiEmuBridge> ();
  node->AddDevice (bridge);
  bridge->SetBridgedNetDevice (nd);

  return bridge;
}

} // namespace ns3
