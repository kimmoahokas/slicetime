// Author: Kimmo Ahokas <kimmo.ahokas@aalto.fi>

#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/core-module.h"
#include "ns3/helper-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("AndroidTapTest");

int main(int argc, char *argv[])
{
  uint32_t delay = 0;
  std::string dataRate = "10Mb/s";
  bool duplex = false;

  CommandLine cmd;
  cmd.AddValue("DataRate", "csma channel data rate", dataRate);
  cmd.AddValue ("Delay", "csma channel delay in microseconds", delay);
  cmd.AddValue("Duplex", "set to true to enable duplex csma", duplex);
  cmd.Parse(argc, argv);


  // use synchronized simulation
  //GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("SimulatorImplementationType", StringValue("ns3::SyncSimulatorImpl"));
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


  NS_LOG_INFO("Setup nodes..");
  // create simulation nodes
  NodeContainer nodes;
  nodes.Create(2);
  NetDeviceContainer csmaDevices;

  // decide if we use duplex channel or not
  if (duplex) {
    CsmaDuplexHelper csma;
    // install csma channel and devices to nodes 0, 1 and 2
    csma.SetChannelAttribute("DataRate", StringValue (dataRate));
    csma.SetChannelAttribute("Delay", TimeValue (MicroSeconds (delay)));
    csmaDevices = csma.Install(nodes);
  } else {
    CsmaHelper csma;
    // install csma channel and devices to nodes 0, 1 and 2
    csma.SetChannelAttribute("DataRate", StringValue (dataRate));
    csma.SetChannelAttribute("Delay", TimeValue (MicroSeconds (delay)));
    csmaDevices = csma.Install(nodes);
  }
  

  //install internet stack to all nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  //install ipv4 addresses to csma devices
  Ipv4AddressHelper address;
  address.SetBase ("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (csmaDevices);

  
  //setup tap bridge
  NS_LOG_INFO("Setup tap bridge");
  
  //install tap bridge to node 0
  TapBridgeHelper tapBridge0 (interfaces.GetAddress (0));
  tapBridge0.SetAttribute ("Mode", StringValue("UseBridge"));
  tapBridge0.SetAttribute ("DeviceName", StringValue("br0-tap1"));
  tapBridge0.Install (nodes.Get (0), csmaDevices.Get (0));
  
  //install tap bridge to node 1
  TapBridgeHelper tapBridge1 (interfaces.GetAddress (1));
  tapBridge1.SetAttribute ("Mode", StringValue("UseBridge"));
  tapBridge1.SetAttribute ("DeviceName", StringValue("br1-tap1"));
  tapBridge1.Install (nodes.Get (1), csmaDevices.Get (1));
  
  // start simulation
  NS_LOG_INFO("Start simulation..");
  //csma.EnablePcapAll("tcpdump", true);
  Simulator::Stop(Seconds (20.0));
  Simulator::Run();
  Simulator::Destroy();
}
