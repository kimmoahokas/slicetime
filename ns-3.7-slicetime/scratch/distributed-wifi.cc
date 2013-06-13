// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/core-module.h"
#include "ns3/helper-module.h"
#include "ns3/mobility-module.h"


/*
 * A simple simulation which bridges a CMSA channel with udp tunnels on each side
 http://code.google.com/p/ns-3/source/browse/examples/tap-wifi-dumbbell.cc?r=88434ff8f0a5f0e85cf45b83a66faf236554162c
 */

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("AndroidTapTest");

void setUpSync() {
  // use synchronized simulation
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  //GlobalValue::Bind ("SimulatorImplementationType", StringValue("ns3::SyncSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));


  // set the synchronized simulation options
  // (the address and port the client listens on has to be broadcast, since the server
  //  sends to one port only and therefore the two instance have to be either on different
  //  machines or on the same one in which case they can only receive on the same port if it
  //  is on a broadcast address)
  
  NS_LOG_INFO("Setting up Sync client..");
  Config::SetDefault("ns3::SyncClient::RecvTimeout", UintegerValue(0)); // disable resending of packets
  Config::SetDefault("ns3::SyncClient::ClientDescription", StringValue ("ns-3 client"));
  Config::SetDefault("ns3::SyncClient::ServerAddress", Ipv4AddressValue("127.0.0.1"));
  Config::SetDefault("ns3::SyncClient::ServerPort", UintegerValue(1234));
  Config::SetDefault("ns3::SyncClient::ClientAddress", Ipv4AddressValue("127.0.0.1"));
  Config::SetDefault("ns3::SyncClient::ClientPort", UintegerValue(17544));
  Config::SetDefault("ns3::SyncClient::ClientId", UintegerValue(0));
}

int main(int argc, char *argv[])
{
  uint32_t delay = 0;
  std::string dataRate = "10Mb/s";
  bool useWifi = false;

  CommandLine cmd;
  cmd.AddValue("DataRate", "csma channel data rate", dataRate);
  cmd.AddValue ("delay", "csma channel delay in microseconds", delay);
  cmd.AddValue ("useWifi", "if true use wifi channel instead of csma", useWifi);
  cmd.Parse(argc, argv);

  setUpSync();

  

  // setup the bridge devices which connects to the other simulation
  GlobalValue::Bind("SyncTunnelReceiveAddress", Ipv4AddressValue("192.168.1.1"));
  GlobalValue::Bind("SyncTunnelReceivePort", UintegerValue(7543));


  NS_LOG_INFO("Setup nodes..");

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::NqstaWifiMac", 
    "Ssid", SsidValue (ssid),
    "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::NqapWifiMac", 
    "Ssid", SsidValue (ssid),
    "BeaconGeneration", BooleanValue (true),
    "BeaconInterval", TimeValue (Seconds (2.5)));

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
  stack.Install (wifiStaNodes);

  
  Ipv4AddressHelper address;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  //address.Assign (staDevices);
  address.Assign (apDevices);
  
  //setup tap bridge
  NS_LOG_INFO("Setup tap bridge");
  
  SyncTunnelBridgeHelper syncBridge1;
  SyncTunnelBridgeHelper syncBridge2;

  syncBridge1.SetAttribute("TunnelFlowId", IntegerValue(17));
  syncBridge1.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.1.2"));
  syncBridge1.SetAttribute("TunnelDestinationPort", UintegerValue(7543));
  syncBridge1.Install(wifiStaNodes.Get(0), staDevices.Get(0));

  syncBridge2.SetAttribute("TunnelFlowId", IntegerValue(18));
  syncBridge2.SetAttribute("TunnelDestinationAddress", Ipv4AddressValue("192.168.1.2"));
  syncBridge2.SetAttribute("TunnelDestinationPort", UintegerValue(7543));
  syncBridge2.Install(wifiStaNodes.Get(1), staDevices.Get(1));
  
  // start simulation
  NS_LOG_INFO("Start simulation..");
  //csma.EnablePcapAll("tcpdump", true);
  Simulator::Stop(Seconds (20.0));
  Simulator::Run();
  Simulator::Destroy();


}
