// Author: Suraj Prabhakaran <suraj.prabhakaran@rwth-aachen.de>

/**
 *  Scenario Chain-HVM
 *  This is a scenario where there is a chain of routers connected to a HVM node each. 
 *  The routers have IP Addresses in the range 10.1.x.x and the link between HVMs and Routers are in the range 192.168.x.x
 *  Note: All the HVM nodes must have IP Addresses 192.168.x.3
 */

#include <iostream>
#include <fstream>
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/core-module.h"
#include "ns3/csma-helper.h"
#include "ns3/application-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/v4ping-helper.h"
#include "ns3/sync-tunnel-bridge-helper.h"
#include "ns3/bridge-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

static void PrintRtt (std::string context, bool timeout, uint16_t seq, Time rtt)
{
  if(!timeout)
    std::cout << "got ping reply for seq " << seq << " with rtt " << rtt << std::endl;
  else
    std::cout << "got no ping reply for seq " << seq << std::endl;
}

int 
main (int argc, char *argv[])
{

  CommandLine cmd;
  cmd.Parse (argc, argv);

  int hvmnodes=2;
  
  // Select simulation type 
  //GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::SyncSimulatorImpl"));
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

  // Set synchronized emulation attributes
  Config::SetDefault ("ns3::SyncClient::RecvTimeout", UintegerValue(0)); // disable resending of packets
  Config::SetDefault ("ns3::SyncClient::ClientDescription", StringValue ("ns-3 client"));
  Config::SetDefault ("ns3::SyncClient::ServerAddress", Ipv4AddressValue("10.0.0.4"));
  Config::SetDefault ("ns3::SyncClient::ServerPort", UintegerValue(17543));
  Config::SetDefault ("ns3::SyncClient::ClientAddress", Ipv4AddressValue("10.0.0.255"));
  Config::SetDefault ("ns3::SyncClient::ClientPort", UintegerValue(17544));
  Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(1));

  // Enable Checksums
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue(true));

  // Create nodes
  NodeContainer nodes, routers;
  nodes.Create(hvmnodes);
  routers.Create(hvmnodes);

  // Set channel delay
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1000)));

  // Install the stack
  InternetStackHelper stack;
  stack.Install(nodes);
  stack.Install(routers);

  // Links
  NetDeviceContainer Node2RouterLink, Router2RouterLink;
  std::ostringstream Node2RouterIp, Router2RouterIp;
  Ipv4AddressHelper address;

  // Set the initial tunnel details
  int flowId=1;
  int destPort=7545;
  SyncTunnelBridgeHelper syncbridge;
  syncbridge.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("10.0.0.4"));
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("10.0.0.3"));
  GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7544));

  for(int i=0;i<hvmnodes;i++)
  {

        Node2RouterIp<<"192.168."<<i<<".0";
        Router2RouterIp<<"10.1."<<i<<".0";

        Node2RouterLink = csma.Install(NodeContainer(nodes.Get(i),routers.Get(i)));
        address.SetBase(Ipv4Address(Node2RouterIp.str().c_str()), "255.255.255.0","0.0.0.1");
        address.Assign(Node2RouterLink);  
        syncbridge.SetAttribute("TunnelFlowId", IntegerValue(flowId));
        syncbridge.SetAttribute("TunnelDestinationPort", UintegerValue(destPort));
	syncbridge.Install(nodes.Get(i), Node2RouterLink.Get(0));

	flowId++;
	destPort++;

        if(i!=hvmnodes-1)
        { 	
		Router2RouterLink = csma.Install(NodeContainer(routers.Get(i),routers.Get(i+1)));
	        address.SetBase(Ipv4Address(Router2RouterIp.str().c_str()), "255.255.255.0","0.0.0.1");
	        address.Assign(Router2RouterLink);     		
	} 

	Node2RouterIp.str("");
	Router2RouterIp.str("");
  } 

  // Generate Routing Table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install a pinger and ping random node
  Ipv4Address pingRemote = Ipv4Address("192.168.0.3");
  V4PingPeriodicHelper ping = V4PingPeriodicHelper(pingRemote);
  ping.SetAttribute("Timeout", TimeValue (Seconds (1)));
  ping.SetAttribute("Interval", TimeValue (Seconds (1)));
  ApplicationContainer apps = ping.Install(routers.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4PingPeriodic/Rtt", MakeCallback(&PrintRtt));

  // Start simulation
  Simulator::Stop (Seconds (1000000.0));
  Simulator::Run ();
  Simulator::Destroy ();

}

