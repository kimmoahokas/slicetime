// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/core-module.h"
#include "ns3/csma-helper.h"
#include "ns3/application-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/v4ping-helper.h"
#include "ns3/sync-tunnel-bridge-helper.h"
#include "ns3/ipv4-address-helper.h"

#include <iostream>

/*
 * Simple example to demonstrate how to use the synchronized simulation implementation for ns-3
 * Inside the simulation are two nodes of which one is bridged with an udp tunnel to the outside world.
 * If two instances are run together with the synchronization server, the nodes which are not bridged
 * ping each other.
 */

using namespace ns3;

static void PrintRtt (std::string context, bool timeout, uint16_t seq, Time rtt)
{
  if(!timeout)
    std::cout << "got ping reply for seq " << seq << " with rtt " << rtt << std::endl;
  else
    std::cout << "got no ping reply for seq " << seq << std::endl;
}

int main(int argc, char *argv[])
{
  // command line parameter to determine which of the sides we are
  // use as follows: './waf --run "sync-ping --side=0"'
  int side = 0;
  CommandLine cmd;
  cmd.AddValue("side", "which side shall this simulation be (0/1)", side);
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
  Config::SetDefault ("ns3::SyncClient::ServerAddress", Ipv4AddressValue("10.0.0.4"));
  Config::SetDefault ("ns3::SyncClient::ServerPort", UintegerValue(17543));
  Config::SetDefault ("ns3::SyncClient::ClientAddress", Ipv4AddressValue("10.0.0.255"));
  Config::SetDefault ("ns3::SyncClient::ClientPort", UintegerValue(17544));
  if(side == 0)
  {
    Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(7));
  }
  else
  {
    Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(2));
  }


  // create simulation nodes
  NodeContainer nodes;
  nodes.Create(2);

  // install csma channel and devices
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  NetDeviceContainer devices = csma.Install(nodes);

  // install IP stack and set addresses
  // (one side has addresses 192.168.0.1,2, the other side has 192.168.0.3,4)
  InternetStackHelper stack;
  stack.Install(nodes); 
  Ipv4AddressHelper ipv4;
  if(side == 0)
    ipv4.SetBase("192.168.0.0", "255.255.255.0", "0.0.0.1");
  else
    ipv4.SetBase("192.168.0.0", "255.255.255.0", "0.0.0.3");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // setup the bridge device which connects to the other simulation
  SyncTunnelBridgeHelper syncBridge;
  // ports have to be swapped on the other side
  UintegerValue tunSendPort;
  UintegerValue tunRecvPort;
  if(side == 0)
  {
    tunSendPort = UintegerValue(7545);
    tunRecvPort = UintegerValue(7544);
  }
  else
  {
    tunSendPort = UintegerValue(7544);
    tunRecvPort = UintegerValue(7545);
  }
  // set other simulation parameters
  syncBridge.SetAttribute("TunnelFlowId", IntegerValue(17));
  syncBridge.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("10.0.0.4"));
  syncBridge.SetAttribute("TunnelDestinationPort", tunSendPort);
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("10.0.0.3"));
  GlobalValue::Bind("SyncTunnelReceivePort", tunRecvPort);
  // install the bridge on node 0
  syncBridge.Install(nodes.Get(0), devices.Get(0));

  // install an ping client on node 1, which pings node 1 in the other simulation
  Ipv4Address pingRemote;
  if(side == 0)
    pingRemote = Ipv4Address("192.168.0.3");
  else
    pingRemote = Ipv4Address("192.168.0.2");
  V4PingPeriodicHelper ping = V4PingPeriodicHelper(pingRemote);
  ping.SetAttribute("Timeout", TimeValue (Seconds (1)));
  ping.SetAttribute("Interval", TimeValue (Seconds (1)));
  ApplicationContainer apps = ping.Install(nodes.Get(1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(100000.0));

  // print the RTT of the sent ping
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4PingPeriodic/Rtt", MakeCallback(&PrintRtt));

  // start simulation
  Simulator::Stop (Seconds (100000.0));
  Simulator::Run ();
  Simulator::Destroy ();


}
