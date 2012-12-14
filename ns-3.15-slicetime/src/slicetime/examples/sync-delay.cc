// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/core-module.h"
//#include "ns3/helper-module.h"
#include "ns3/csma-helper.h"
#include "ns3/internet-module.h"

#include <iostream>

/*
 * Simple example to demonstrate how to use the synchronized simulation implementation for ns-3
 * Inside the simulation are two nodes of which one is bridged with an udp tunnel to the outside world.
 * This simple setup can be used to ping the inner node from the outside and measure the delay.
 */

using namespace ns3;

int main(int argc, char *argv[])
{
  unsigned long delay = 0;
  CommandLine cmd;
  cmd.AddValue("delay", "the channel delay in microseconds", delay);
  cmd.Parse(argc, argv);

  // enable checksums in communication protocols
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue(true));
  
  // use synchronized simulation
  GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::SyncSimulatorImpl"));
  //GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

  // set the synchronized simulation options
  // (the address and port the client listens on has to be broadcast, since the server
  //  sends to one port only and therefore the two instance have to be either on different
  //  machines or on the same one in which case they can only receive on the same port if it
  //  is on a broadcast address)
  Config::SetDefault ("ns3::SyncClient::RecvTimeout", UintegerValue(0)); // disable resending of packets
  Config::SetDefault ("ns3::SyncClient::ClientDescription", StringValue ("ns-3 client"));
  Config::SetDefault ("ns3::SyncClient::ServerAddress", Ipv4AddressValue("127.0.0.1"));
  Config::SetDefault ("ns3::SyncClient::ServerPort", UintegerValue(17543));
  Config::SetDefault ("ns3::SyncClient::ClientAddress", Ipv4AddressValue("127.0.0.255"));
  Config::SetDefault ("ns3::SyncClient::ClientPort", UintegerValue(17544));
  Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(1));

  // create simulation nodes
  NodeContainer nodes;
  nodes.Create(2);

  // install duplex csma channel and devices
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (delay)));
  NetDeviceContainer devices = csma.Install(nodes);

  // install IP stack and set addresses
  InternetStackHelper stack;
  stack.Install(nodes); 
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.0.0", "255.255.255.0", "0.0.0.1");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // setup the bridge device
  //SyncTunnelBridgeHelper syncBridge;
  //syncBridge.SetAttribute("TunnelFlowId", IntegerValue(17));
  //syncBridge.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("10.0.0.4"));
  //syncBridge.SetAttribute("TunnelDestinationPort", UintegerValue(7545));
  //GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("10.0.0.3"));
  //GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7544));
  // install the bridge on node 0
  //syncBridge.Install(nodes.Get(0), devices.Get(0));

  // start simulation
  Simulator::Stop (Seconds (600.0));
  Simulator::Run ();
  Simulator::Destroy ();


}
