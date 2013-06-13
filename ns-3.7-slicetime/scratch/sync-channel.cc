// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/core-module.h"
#include "ns3/helper-module.h"

#include <iostream>

/*
 * A simple simulation which bridges a CMSA channel with udp tunnels on each side
 */

using namespace ns3;

int main(int argc, char *argv[])
{
  // use synchronized simulation
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::SyncSimulatorImpl"));

  // set the synchronized simulation options
  // (the address and port the client listens on has to be broadcast, since the server
  //  sends to one port only and therefore the two instance have to be either on different
  //  machines or on the same one in which case they can only receive on the same port if it
  //  is on a broadcast address)
  Config::SetDefault ("ns3::SyncClient::RecvTimeout", UintegerValue(0)); // disable resending of packets
  Config::SetDefault ("ns3::SyncClient::ClientDescription", StringValue ("ns-3 client"));
  Config::SetDefault ("ns3::SyncClient::ServerAddress", Ipv4AddressValue("192.168.1.x"));
  Config::SetDefault ("ns3::SyncClient::ServerPort", UintegerValue(17543));
  Config::SetDefault ("ns3::SyncClient::ClientAddress", Ipv4AddressValue("192.168.1.255"));
  Config::SetDefault ("ns3::SyncClient::ClientPort", UintegerValue(17544));
  Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(1));


  // create simulation nodes
  NodeContainer nodes;
  nodes.Create(2);

  // install csma channel and devices
  CsmaDuplexHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  NetDeviceContainer devices = csma.Install(nodes);

  // setup the bridge devices which connects to the other simulation
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("192.168.1.x"));
  GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7543));

  SyncTunnelBridgeHelper syncBridge1;
  SyncTunnelBridgeHelper syncBridge2;

  syncBridge1.SetAttribute("TunnelFlowId", IntegerValue(17));
  syncBridge1.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.1.x"));
  syncBridge1.SetAttribute("TunnelDestinationPort", UintegerValue(7543));
  syncBridge1.Install(nodes.Get(0), devices.Get(0));

  syncBridge2.SetAttribute("TunnelFlowId", IntegerValue(18));
  syncBridge2.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.1.x"));
  syncBridge2.SetAttribute("TunnelDestinationPort", UintegerValue(7543));
  syncBridge2.Install(nodes.Get(1), devices.Get(1));

  // start simulation
  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();
  Simulator::Destroy ();


}
