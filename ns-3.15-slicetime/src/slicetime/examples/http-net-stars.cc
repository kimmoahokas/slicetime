// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/application-container.h"
#include "ns3/sync-tunnel-bridge-helper.h"
#include "ns3/udp-echo-server.h"
#include "ns3/http-server.h"
#include "ns3/http-client.h"
#include "ns3/random-variable.h"
#include "ns3/abort.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-static-routing-helper.h"
#include <sys/time.h>
#include <iostream>
#include <fstream>


/* This is an example which supports to build a large network of nodes running both an http server and client.
 * The structure consits of a chain of core routers which are connected with 10Gb/s links.
 * To each of these routers a star is attached. The connection between the star router and the core router has a speed 
 * of 1Gb/s. The star's outer nodes are connected to the star router with 100Mb/s. Only these outer nodes contain an 
 * http server and client. The HTTP clients make periodic requests to a randomly chosen HTTP server.
 * The routing tables of all nodes are created manually because the topology is fixed and an algorithmic generation with 
 * the GlobalRoutingTable module did not scale.
 * Connected to the first star is a gateway node which can be used to make HTTP requests with an external program. 
 * The choice of parameters for the HTTPClients determines how much background traffic the network has.
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HttpNetStars");

// helper variables and function to determine how much slower than realtime the simulation is running
struct timeval timeval_old;
bool first_timeval = true;
double time_sum = 0;
double time_sum_after_30 = 0;
uint64_t numSeconds = 0;

// prints each second how much time the last simulated second took and is thereby a measure how slow the simulation is running
void printTime() {
  struct timeval timeval_now;
  gettimeofday(&timeval_now, NULL);
  if(first_timeval) {
    first_timeval = false;
  }
  else {
    double time_diff = (timeval_now.tv_sec - timeval_old.tv_sec) + (timeval_now.tv_usec - timeval_old.tv_usec) / 1000000.0;
    time_sum += time_diff;
    if(numSeconds > 30)
      time_sum_after_30 += time_diff;
    std::cout << "This is second " << numSeconds << ", last second took: " << time_diff << " s, average time per second: " << time_sum/numSeconds << " s, average time after 30 s: " << time_sum_after_30/(numSeconds-30) << " s" << std::endl;
  }
  timeval_old = timeval_now;
  numSeconds++;

  Simulator::Schedule(Seconds(1), &printTime);
}

// starts the periodic requests of all http clients
void startClients (std::vector<Ptr<HttpClient> > httpClients, std::vector<Ipv4Address> serverAddresses)
{
  for (uint32_t i=0; i<httpClients.size(); i++)
    httpClients[i]->StartPeriodicRequests(serverAddresses, 2, 8, 10, 120); // mean length, length limit, min pause, max pause
}

// helper function which constructs an Ipv4Address by giving the four octets
Ipv4Address helperConstructIp(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  return Ipv4Address(a<<24 | b<<16 | c<<8 | d); // Ipv4Address takes host order
}

// helper function to determine (and create) the interface index of a given device
int32_t helperGetIfIndex (Ptr<Ipv4> ipv4, Ptr<NetDevice> dev) {
  int32_t ifIndex = ipv4->GetInterfaceForDevice(dev);
  if(ifIndex == -1)
    ifIndex = ipv4->AddInterface(dev);

  return ifIndex;
}

// helper to assign an ip address to a given interface
void helperAssignIp (Ptr<Ipv4> ipv4, int32_t ifIndex, Ipv4Address addr, Ipv4Mask mask) {
  Ipv4InterfaceAddress ipv4A = Ipv4InterfaceAddress (Ipv4Address (addr), Ipv4Mask (mask));
  ipv4->AddAddress (ifIndex, ipv4A);
  ipv4->SetMetric (ifIndex, 1);
  ipv4->SetUp (ifIndex);
}

int main (int argc, char *argv[])
{
  // size of the network
  uint8_t numStars = 40;
  uint8_t nodesPerStar = 150 ; // without center (and gateway at first star)

  NS_ABORT_IF(numStars == 0 || numStars > 252); // because of TTL=255
  NS_ABORT_IF(nodesPerStar == 0 || nodesPerStar > 252); // addresses for gateway and center node needed

  // initalize RNG
  timeval curtime;
  gettimeofday(&curtime, NULL);
  SeedManager::SetSeed((uint32_t)(curtime.tv_sec ^ curtime.tv_usec));


  // Select simulation type. Synchronization or Realtime 
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::SyncSimulatorImpl"));
  //GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));

  // setup synchronization
  Config::SetDefault ("ns3::SyncClient::RecvTimeout", UintegerValue(0));
  Config::SetDefault ("ns3::SyncClient::ClientDescription", StringValue ("ns-3 client"));
  Config::SetDefault ("ns3::SyncClient::ServerAddress", Ipv4AddressValue("10.0.0.4"));
  Config::SetDefault ("ns3::SyncClient::ServerPort", UintegerValue(17543));
  Config::SetDefault ("ns3::SyncClient::ClientAddress", Ipv4AddressValue("10.0.0.255"));
  Config::SetDefault ("ns3::SyncClient::ClientPort", UintegerValue(17544));
  Config::SetDefault ("ns3::SyncClient::ClientId", UintegerValue(4));

  // Enable Checksums
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue(true));

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1400)); // ! encapsulation in udp tunnel !

  //
  // create nodes
  //
  NodeContainer allIpNodes;
  NodeContainer allClientServerNodes;

  // backbone nodes
  NodeContainer backboneNodes;
  backboneNodes.Create(numStars);
  allIpNodes.Add(backboneNodes);
  
  // nodes in each star (nodesPerStar + center node)
  std::vector<NodeContainer> starNodes (numStars);
  for(uint8_t i=0; i<numStars; i++) 
  {
    starNodes[i] = NodeContainer ();
    starNodes[i].Create(nodesPerStar+1);
    allIpNodes.Add(starNodes[i]);
    for(uint8_t j=0; j<nodesPerStar; j++)
       allClientServerNodes.Add(starNodes[i].Get(j+1));
    
  }
  // gateway node
  NodeContainer gatewayNode;
  gatewayNode.Create(1);

  //
  // install network stack on all IP nodes
  //
  InternetStackHelper internet;
  internet.Install (allIpNodes);

  //
  // setup channels
  //
  CsmaHelper csma;

  // between backbone nodes
  csma.SetChannelAttribute ("DataRate", StringValue ("10Gb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (100)));
  std::vector<NetDeviceContainer> backboneLinks (numStars-1);
  for(uint8_t i=0; i<numStars-1; i++) {
    backboneLinks[i] = csma.Install (NodeContainer (backboneNodes.Get(i), backboneNodes.Get(i+1)));
  }

  // between backbone nodes and stars
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (25)));
  std::vector<NetDeviceContainer> backboneStarLinks (numStars);
  for(uint8_t i=0; i<numStars; i++) {
    backboneStarLinks[i] = csma.Install (NodeContainer (backboneNodes.Get(i), starNodes[i].Get(0)));
  }

  // inside the stars
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (10)));
  std::vector<std::vector<NetDeviceContainer> > starLinks (numStars);
  for(uint8_t i=0; i<numStars; i++) {
    starLinks[i] = std::vector<NetDeviceContainer> (nodesPerStar);
    for(uint8_t j=0; j<nodesPerStar; j++) {
      starLinks[i][j] = csma.Install (NodeContainer (starNodes[i].Get(0), starNodes[i].Get(j+1)));
    }
  }

  // between gateway node and star 0
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mb/s"));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (5)));
  NetDeviceContainer gatewayStar0Link = csma.Install (NodeContainer (starNodes[0].Get(0), gatewayNode.Get(0)));
  

  //
  // setup ip addresses and routing
  //

  // The ip addresses and routing tables are set manually for all nodes.
  // The core routers have the address 192.168.0.x
  // The star routers have the address 192.168.x.1 and the star's outer nodes have the addresses 192.168.x.[>=2]
  // The addresses are used on all interfaces of a node.

  // in backbone nodes
  for(uint8_t i=0; i<numStars; i++) {
    Ptr<Node> node = backboneNodes.Get(i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ipv4s = ipv4RoutingHelper.GetStaticRouting (ipv4);
 
    Ipv4Address thisIp = helperConstructIp(192,168,0,i+1);
    Ipv4Mask thisNetmask = Ipv4Mask("255.255.255.255");

    // to star
    int32_t ifIndexStar = helperGetIfIndex (ipv4, backboneStarLinks[i].Get(0));
    helperAssignIp (ipv4, ifIndexStar, thisIp, thisNetmask);
    ipv4s->AddHostRouteTo(helperConstructIp(192,168,i+1,1), ifIndexStar);
    ipv4s->AddNetworkRouteTo (helperConstructIp(192,168,i+1,0), Ipv4Mask("255.255.255.0"), helperConstructIp(192,168,i+1,1), ifIndexStar);

    // to previous nodes
    if(i>0) {
      int32_t ifIndexPrev = helperGetIfIndex(ipv4, backboneLinks[i-1].Get(1));
      helperAssignIp (ipv4, ifIndexPrev, thisIp, thisNetmask);
      Ipv4Address prevIp = helperConstructIp(192,168,0,i);
      ipv4s->AddHostRouteTo(prevIp, ifIndexPrev);
      for(int32_t j=1; j<=i; j++)
        ipv4s->AddNetworkRouteTo (helperConstructIp(192,168,j,0), Ipv4Mask("255.255.255.0"), prevIp, ifIndexPrev);
      for(int32_t j=1; j<i; j++)
        ipv4s->AddHostRouteTo (helperConstructIp(192,168,0,j), prevIp, ifIndexPrev);
    }
    // to next nodes
    if(i<numStars-1) {
      int32_t ifIndexNext = helperGetIfIndex(ipv4, backboneLinks[i].Get(0));
      helperAssignIp (ipv4, ifIndexNext, thisIp, thisNetmask);
      Ipv4Address nextIp = helperConstructIp(192,168,0,i+2);
      ipv4s->AddHostRouteTo(nextIp, ifIndexNext);
      for(int32_t j=i+2; j<=numStars; j++)
        ipv4s->AddNetworkRouteTo (helperConstructIp(192,168,j,0), Ipv4Mask("255.255.255.0"), nextIp, ifIndexNext);
      for(int32_t j=i+3; j<=numStars; j++)
        ipv4s->AddHostRouteTo (helperConstructIp(192,168,0,j), nextIp, ifIndexNext);
    }
  }

  // in star center nodes
  for(uint8_t i=0; i<numStars; i++) {
    Ptr<Node> node = starNodes[i].Get(0);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
   Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ipv4s = ipv4RoutingHelper.GetStaticRouting (ipv4);

    Ipv4Address thisIp = helperConstructIp(192,168,i+1,1);
    Ipv4Mask thisNetmask = Ipv4Mask("255.255.255.255");

    // to backbone
    int32_t ifIndexBackbone = helperGetIfIndex (ipv4, backboneStarLinks[i].Get(1));
    helperAssignIp (ipv4, ifIndexBackbone, thisIp, thisNetmask);
    ipv4s->AddHostRouteTo(helperConstructIp(192,168,0,i+1), ifIndexBackbone);
    for(uint8_t j=0; j<numStars; j++) // default route would lead to cycles between this node and backbone node for inexisting nodes
      if(j != i)
        ipv4s->AddNetworkRouteTo (helperConstructIp(192,168,j+1,0), Ipv4Mask("255.255.255.0"), helperConstructIp(192,168,0,i+1), ifIndexBackbone);

    // to outer nodes
    for(uint8_t j=0; j<nodesPerStar; j++) {
      int32_t ifIndexOuter = helperGetIfIndex(ipv4, starLinks[i][j].Get(0));
      helperAssignIp (ipv4, ifIndexOuter, thisIp, thisNetmask);
      ipv4s->AddHostRouteTo(helperConstructIp (192,168,i+1,j+2), ifIndexOuter);
    }
  }

  std::vector<Ipv4Address> serverAddresses (allClientServerNodes.GetN());
  uint32_t serverAddrIdx = 0;

  // in star outer nodes
  for(uint8_t i=0; i<numStars; i++) {
    for(uint8_t j=0; j<nodesPerStar; j++) {
      Ptr<Node> node = starNodes[i].Get(j+1);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> ipv4s = ipv4RoutingHelper.GetStaticRouting (ipv4);

      Ipv4Address thisIp = helperConstructIp(192,168,i+1,j+2);
      Ipv4Mask thisNetmask = Ipv4Mask("255.255.255.255");
      serverAddresses[serverAddrIdx++] = thisIp;

      // to center node
      int32_t ifIndex = helperGetIfIndex(ipv4, starLinks[i][j].Get(1));
      helperAssignIp (ipv4, ifIndex, thisIp, thisNetmask);
      ipv4s->AddHostRouteTo(helperConstructIp(192,168,i+1,1), ifIndex);
      ipv4s->SetDefaultRoute(helperConstructIp(192,168,i+1,1), ifIndex);
    }
  }

  // routing from first star's center node to gateway
  // (the gateway node itself has no ip address inside the simulation)
  Ipv4Address gatewayAddress = helperConstructIp(192,168,1,nodesPerStar+2);
  {
    Ptr<Node> node = starNodes[0].Get(0);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ipv4s = ipv4RoutingHelper.GetStaticRouting (ipv4);

    int32_t ifIndex = helperGetIfIndex(ipv4, gatewayStar0Link.Get(0));
    helperAssignIp (ipv4, ifIndex, Ipv4Address("192.168.1.1"), Ipv4Mask("255.255.255.255"));
    ipv4s->AddHostRouteTo(gatewayAddress, ifIndex);
  }

  //
  // install applications
  //
  std::cout<<"installing applications"<<std::endl;

  // http servers
  ApplicationContainer serverApps;
  for (uint32_t i=0; i < allClientServerNodes.GetN(); i++) {
    Ptr<HttpServer> webServer = CreateObject<HttpServer>();
    allClientServerNodes.Get(i)->AddApplication(webServer);
    serverApps.Add(webServer);
  }
  serverApps.Start (Seconds(0.0));
  serverApps.Stop (Seconds(3600.0));

  // http clients
  ApplicationContainer clientApps;
  std::vector<Ptr<HttpClient> > httpClients (allClientServerNodes.GetN());
  for (uint32_t i=0; i < allClientServerNodes.GetN(); i++) {
    Ptr<HttpClient> webClient = CreateObject<HttpClient>();
    httpClients[i] = webClient;
    allClientServerNodes.Get(i)->AddApplication(webClient);
    clientApps.Add(webClient);
  }
  //clientApps.Start (Seconds(0.0));
  //clientApps.Stop (Seconds(3600.0));
  

  //
  // install bridge
  //
  SyncTunnelBridgeHelper syncBridge1;
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("10.0.0.3"));
  GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7544));
  syncBridge1.SetAttribute("TunnelFlowId", IntegerValue(1));
  syncBridge1.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("10.0.0.4"));
  syncBridge1.SetAttribute("TunnelDestinationPort", UintegerValue(7545));
  syncBridge1.Install(gatewayNode.Get(0), gatewayStar0Link.Get(1));

  // schedule helper function to print time
  Simulator::Schedule(Seconds(0), &printTime);

  // print the IP address of the gateway to be used on the other end of the bridge
  std::cout << "The IP address in the VM should be: " << gatewayAddress << " (netmask 255.255.255.255, gateway 192.168.1.1)" << std::endl;

  // schedule http client requests
  Simulator::Schedule(Seconds(1.0), startClients, httpClients, serverAddresses);

  //
  // start the simulation
  //
  Simulator::Stop (Seconds (3600.0));
  Simulator::Run ();
  Simulator::Destroy ();
}
