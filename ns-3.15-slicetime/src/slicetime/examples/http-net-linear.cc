// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include <iostream>
#include <fstream>
#include <sstream>

#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/application-container.h"
#include "ns3/sync-tunnel-bridge-helper.h"
#include "ns3/udp-echo-server.h"
#include "ns3/http-server.h"
#include "ns3/http-client.h"
#include "ns3/random-variable.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <sys/time.h>


/*
 * An example to build a network of http clients and servers.
 * There is a chain of routers of which each is connected to a node running an http server and client.
 * The http servers are running all the time and the clients use the random periodic request feature of the HttpClient.
 * The node connected to the first router does not have an http client and server, but contains a TapBridge so that external
 * programs can be used to make HTTP requests in this network with background traffic.
 * The routing is done with GlobalRouteManager which calculates the optimal routes with "god-knowledge".
 * However this route-calculation takes far too long for larger networks and also the TTL would be a problem in such a case.
 * Therefore the HttpNetStars example was created to support large network sizes.
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HttpNetLinear");

// starts the periodic requests in all http clients
void startClients (std::vector<Ptr<HttpClient> > httpClients, std::vector<Ipv4Address> serverAddresses)
{
  std::cout << "Starting HTTP requests ..." << std::endl;
  for (uint32_t i=0; i<httpClients.size(); i++)
    httpClients[i]->StartPeriodicRequests(serverAddresses, 16, 1000, 1, 60); // mean length, length limit, min pause, max pause
}

int main (int argc, char *argv[])
{
  int netLength = 10;

  // initalize RNG
  timeval curtime;
  gettimeofday(&curtime, NULL);
  SeedManager::SetSeed((uint32_t)(curtime.tv_sec ^ curtime.tv_usec));


  // setup synchronization
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  //GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::SyncSimulatorImpl"));
  Config::SetDefault ("ns3::SyncClient::RecvTimeout", UintegerValue(0));
  Config::SetDefault ("ns3::SyncClient::ClientDescription", StringValue ("ns-3 client"));
  Config::SetDefault ("ns3::SyncClient::ServerAddress", Ipv4AddressValue("10.0.0.4"));
  Config::SetDefault ("ns3::SyncClient::ServerPort", UintegerValue(17543));
  Config::SetDefault ("ns3::SyncClient::ClientAddress", Ipv4AddressValue("10.0.0.255"));
  Config::SetDefault ("ns3::SyncClient::ClientPort", UintegerValue(17544));
  Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(1));


  // enable checksums
//  Config::SetDefault ("ns3::Ipv4L3Protocol::CalcChecksum", BooleanValue (true)); 
//  Config::SetDefault ("ns3::Icmpv4L4Protocol::CalcChecksum", BooleanValue (true)); 
//  Config::SetDefault ("ns3::TcpL4Protocol::CalcChecksum", BooleanValue (true)); 
//  Config::SetDefault ("ns3::UdpL4Protocol::CalcChecksum", BooleanValue (true)); 
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue(true));

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1450)); 

  // create nodes
  NodeContainer routerNodes;
  routerNodes.Create (netLength);
  NodeContainer serverNodes;
  serverNodes.Create (netLength);
  NodeContainer clientNodes;
  clientNodes.Create (netLength);
  NodeContainer tunnelNode;
  tunnelNode.Create (1);

  NodeContainer allNodes = NodeContainer (routerNodes, serverNodes, clientNodes, tunnelNode);

  // install network stack on all nodes
  InternetStackHelper internet;
  internet.Install (allNodes);

  // setup channels between routers and clients/servers
  CsmaHelper csma;
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mb/s"));
 
  std::vector<NetDeviceContainer> routerServerLinks (netLength);  
  std::vector<NetDeviceContainer> routerClientLinks (netLength);  
  for (int i=0; i < netLength; i++) {
    routerServerLinks[i] = csma.Install (NodeContainer (routerNodes.Get(i), serverNodes.Get(i)) );
    routerClientLinks[i] = csma.Install (NodeContainer (routerNodes.Get(i), clientNodes.Get(i)) );
  }

  // setup channel between gateway and first router
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  NetDeviceContainer firstLink = csma.Install (NodeContainer (tunnelNode.Get(0), routerNodes.Get(0)));

  // setup channels between routers
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (1)));
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  std::vector<NetDeviceContainer> routerLinks (netLength-1);  
  for (int i=0; i < netLength-1; i++) {
    routerLinks[i] = csma.Install (NodeContainer (routerNodes.Get(i), routerNodes.Get(i+1)) );
  }
  
  // setup ip addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer firstInterfaces = ipv4.Assign (firstLink);

  std::vector<Ipv4InterfaceContainer> routerServerInterfaces (netLength);
  std::vector<Ipv4InterfaceContainer> routerClientInterfaces (netLength);
  std::vector<Ipv4InterfaceContainer> routerInterfaces (netLength-1);
  for (int i=0; i < netLength; i++) {
    if(i < netLength-1)
    {
      std::ostringstream subnetR;
      subnetR<<"192.168."<<i+1<<".0";
      ipv4.SetBase (subnetR.str().c_str(), "255.255.255.252");
      routerInterfaces[i] = ipv4.Assign(routerLinks[i]);
    }

    std::ostringstream subnetS;
    subnetS<<"192.168."<<i+1<<".4";
    ipv4.SetBase (subnetS.str().c_str(), "255.255.255.252");
    routerServerInterfaces[i] = ipv4.Assign(routerServerLinks[i]);

    std::ostringstream subnetC;
    subnetC<<"192.168."<<i+1<<".8";
    ipv4.SetBase (subnetC.str().c_str(), "255.255.255.252");
    routerClientInterfaces[i] = ipv4.Assign(routerClientLinks[i]);

  }

  // create list of server addresses
  std::vector<Ipv4Address> serverAddresses (netLength);
  for (int i=0; i<netLength; i++)
  {
    serverAddresses[i] = routerServerInterfaces[i].GetAddress(1);
  }

  // print IP addresses
  for (int i=0; i<netLength; i++) {
    std::cout << "server " << i <<  " " << routerServerInterfaces[i].GetAddress(1) << std::endl;
    std::cout << "client " << i << " " << routerClientInterfaces[i].GetAddress(1) << std::endl;
    std::cout << "router " << i << " to server: " << routerServerInterfaces[i].GetAddress(0) << std::endl;
    std::cout << "router " << i << " to client: " << routerClientInterfaces[i].GetAddress(0) << std::endl;
    if(i > 0)
      std::cout << "router " << i << " to previous router: " << routerInterfaces[i-1].GetAddress(1) << std::endl;
    if(i == 0)
      std::cout << "router " << i << " to gateway: " << firstInterfaces.GetAddress(1) << std::endl;
    if(i < netLength-1)
      std::cout << "router " << i << " to next router: " << routerInterfaces[i].GetAddress(0) << std::endl;
    std::cout << std::endl;
  }
 
  // turn on routing
  std::cout << "Calculating Routes ..." << std::endl;
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // install http servers
  ApplicationContainer serverApps;
  for (int i=0; i < netLength; i++) {
    Ptr<HttpServer> webServer = CreateObject<HttpServer>();
    serverNodes.Get(i)->AddApplication(webServer);
    serverApps.Add(webServer);
  }
  serverApps.Start (Seconds(0.0));
  serverApps.Stop (Seconds(3600.0));

  // install http clients
  ApplicationContainer clientApps;
  std::vector<Ptr<HttpClient> > httpClients (netLength);
  for (int i=0; i < netLength; i++) {
    Ptr<HttpClient> webClient = CreateObject<HttpClient>();
    httpClients[i] = webClient;
    clientNodes.Get(i)->AddApplication(webClient);
    clientApps.Add(webClient);
  }
  clientApps.Start (Seconds(0.0));
  clientApps.Stop (Seconds(3600.0));
  
  // schedule to start the periodic requests
  Simulator::Schedule(Seconds(1.0), startClients, httpClients, serverAddresses);

  // install bridge

  SyncTunnelBridgeHelper syncBridge1;
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("10.0.0.3"));
  GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7544));
  syncBridge1.SetAttribute("TunnelFlowId", IntegerValue(1));
  syncBridge1.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("10.0.0.4"));
  syncBridge1.SetAttribute("TunnelDestinationPort", UintegerValue(7545));
  syncBridge1.Install(tunnelNode.Get(0), firstLink.Get(0));


  // start the simulation
  Simulator::Stop (Seconds (3600.0));
  Simulator::Run ();
  Simulator::Destroy ();
}
