// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include <iostream>
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/core-module.h"
#include "ns3/csma-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/sync-tunnel-bridge-helper.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-global-routing-helper.h"


/*
 * A simple simulation which bridges two CMSA channels with a router between them
 */

using namespace ns3;

int main(int argc, char *argv[])
{
  // use synchronized simulation
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::SyncSimulatorImpl"));

  // enable checksums in communication protocols
  Config::SetDefault ("ns3::Ipv4L3Protocol::CalcChecksum", BooleanValue (true));
  Config::SetDefault ("ns3::Icmpv4L4Protocol::CalcChecksum", BooleanValue (true));
  Config::SetDefault ("ns3::TcpL4Protocol::CalcChecksum", BooleanValue (true));
  Config::SetDefault ("ns3::UdpL4Protocol::CalcChecksum", BooleanValue (true));

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
  NodeContainer allNodes;
  allNodes.Create(3);

  NodeContainer channelOneNodes = NodeContainer(allNodes.Get(0), allNodes.Get(1));
  NodeContainer channelTwoNodes = NodeContainer(allNodes.Get(2), allNodes.Get(1));
  


  // install csma channel and devices
  CsmaHelper csmaOne;
  csmaOne.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  csmaOne.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  NetDeviceContainer devicesOne = csmaOne.Install(channelOneNodes);

  CsmaHelper csmaTwo;
  csmaTwo.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  csmaTwo.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  NetDeviceContainer devicesTwo = csmaTwo.Install(channelTwoNodes);

  // setup the bridge devices which connects to the other simulation
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("192.168.1.x"));
  GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7543));

  SyncTunnelBridgeHelper syncBridge1;
  SyncTunnelBridgeHelper syncBridge2;

  syncBridge1.SetAttribute("TunnelFlowId", IntegerValue(17));
  syncBridge1.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.1.x"));
  syncBridge1.SetAttribute("TunnelDestinationPort", UintegerValue(7543));
  syncBridge1.Install(channelOneNodes.Get(0), devicesOne.Get(0));

  syncBridge2.SetAttribute("TunnelFlowId", IntegerValue(18));
  syncBridge2.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.1.x"));
  syncBridge2.SetAttribute("TunnelDestinationPort", UintegerValue(7543));
  syncBridge2.Install(channelTwoNodes.Get(0), devicesTwo.Get(0));

  // Install network stacks on the nodes
  InternetStackHelper internet;
  internet.Install (allNodes);

  // set IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  ipv4.Assign (devicesOne);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  ipv4.Assign (devicesTwo);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // start simulation
  Simulator::Stop (Seconds (3600.0));
  Simulator::Run ();
  Simulator::Destroy ();


}
