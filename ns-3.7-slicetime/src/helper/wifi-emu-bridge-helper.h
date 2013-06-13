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

// this file is an adapted version of tap-bridge-helper.h

#ifndef WIFI_EMU_BRIDGE_HELPER_H
#define WIFI_EMU_BRIDGE_HELPER_H

#include "net-device-container.h"
#include "ns3/object-factory.h"
#include "ns3/wifi-emu-bridge.h"
#include <string>

namespace ns3 {

class Node;
class AttributeValue;

/**
 * Helper to install WifiEmuBridge on a node
 */
class WifiEmuBridgeHelper
{
public:
  /*
   * Constructs a WifiEmuBridgeHelper
   */
  WifiEmuBridgeHelper ();

  /*
   * Sets an attribute in the underlying WifiEmuBridge
   *
   * \param n1 the name
   * \param v1 the value
   */
  void SetAttribute (std::string n1, const AttributeValue &v1);

  /*
   * Installs the bridge on a node and forms the bridge with the specified netdevice.
   *
   * \param node the node to install the bridge on
   * \param nd the netdevice to bridge to
   */
  Ptr<NetDevice> Install (Ptr<Node> node, Ptr<NetDevice> nd);

private:
  ObjectFactory m_deviceFactory;
};

} // namespace ns3


#endif /* WIFI_EMU_BRIDGE_HELPER_H */
