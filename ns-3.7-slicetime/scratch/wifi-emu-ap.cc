// Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>

#include "ns3/core-module.h"
#include "ns3/common-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/mobility-module.h"
#include "ns3/contrib-module.h"
#include "ns3/wifi-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

NS_LOG_COMPONENT_DEFINE ("WifiEmuAp");

using namespace ns3;

int main (int argc, char *argv[])
{
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  Config::SetDefault ("ns3::RealtimeSimulatorImpl::SynchronizationMode", EnumValue (RealtimeSimulatorImpl::SYNC_HARD_LIMIT));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2304"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2304"));

  NodeContainer c;
  c.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (0) ); 

  YansWifiChannelHelper wifiChannel ;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  Ssid ssid = Ssid ("wifi-default");
  wifiMac.SetType ("ns3::NqstaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (wifiPhy, wifiMac, c.Get(0));
  NetDeviceContainer devices = staDevice;
  wifiMac.SetType ("ns3::NqapWifiMac", "Ssid", SsidValue (ssid),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (2.5)));
  NetDeviceContainer apDevice = wifi.Install (wifiPhy, wifiMac, c.Get(1));
  devices.Add (apDevice);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  InternetStackHelper internet;
  internet.Install (c.Get(1));

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (apDevice);

  WifiEmuBridgeHelper wbridge;
  wbridge.SetAttribute("ClientId", UintegerValue(1));
  wbridge.Install(c.Get(0), staDevice.Get(0));

  Simulator::Stop (Seconds (3600.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

