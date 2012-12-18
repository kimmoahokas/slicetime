/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/slicetime-module.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("android-wifi-infra");

int 
main (int argc, char *argv[])
{

// use synchronized simulation
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::SyncSimulatorImpl"));
  
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  //LOG_INFO ("Creating Simulation");

  // set the synchronized simulation options
  // (the address and port the client listens on has to be broadcast, since the server
  //  sends to one port only and therefore the two instance have to be either on different
  //  machines or on the same one in which case they can only receive on the same port if it
  //  is on a broadcast address)
  Config::SetDefault ("ns3::SyncClient::RecvTimeout", UintegerValue(0)); // disable resending of packets
  Config::SetDefault ("ns3::SyncClient::ClientDescription", StringValue ("ns-3 client"));
  Config::SetDefault ("ns3::SyncClient::ServerAddress", Ipv4AddressValue("192.168.3.3"));
  Config::SetDefault ("ns3::SyncClient::ServerPort", UintegerValue(17600));
  Config::SetDefault ("ns3::SyncClient::ClientAddress", Ipv4AddressValue("192.168.3.255"));
  Config::SetDefault ("ns3::SyncClient::ClientPort", UintegerValue(17601));
  Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(1));
  //LOG_INFO ("Connect to the synchornizer");

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  //stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("192.168.11.0", "255.255.255.0");
  //address.Assign (staDevices);
  address.Assign (apDevices);
 
  // setup the bridge devices which connects to the other simulation
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("192.168.3.3"));
  GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7600));


  /*
   * When using the SyncTunnelBridge UseLocal mode, the mac address of
   * underlying ns-3 NetDevice must match mac address of virtual machine.
   * There's no auto-configuration currently, so change mac address by hand.
   */
  staDevices.Get(0)->SetAddress (Mac48Address("00:00:00:00:00:04"));
  staDevices.Get(1)->SetAddress (Mac48Address("00:00:00:00:00:05"));

  SyncTunnelBridgeHelper syncBridge1;
  SyncTunnelBridgeHelper syncBridge2;

  syncBridge1.SetAttribute("TunnelFlowId", IntegerValue(1));
  syncBridge1.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.3.1"));
  syncBridge1.SetAttribute("TunnelDestinationPort", UintegerValue(7601));
  syncBridge1.SetAttribute("Mode", StringValue("UseLocal"));
  syncBridge1.Install(wifiStaNodes.Get(0), staDevices.Get(0));

  syncBridge2.SetAttribute("TunnelFlowId", IntegerValue(2));
  syncBridge2.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.3.1"));
  syncBridge2.SetAttribute("TunnelDestinationPort", UintegerValue(7602));
  syncBridge2.SetAttribute("Mode", StringValue("UseLocal"));
  syncBridge2.Install(wifiStaNodes.Get(1), staDevices.Get(1));
  //LOG_INFO ("Attach the bridge to the nodes");
 
 
 phy.EnablePcap ("third", apDevices.Get (0));
 
 
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (20.0));


  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
