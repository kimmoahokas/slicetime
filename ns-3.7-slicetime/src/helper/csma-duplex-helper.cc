/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * adaptions by: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

// this file is an adapted version of helper/csma-helper.cc

#include "csma-duplex-helper.h"
#include "ns3/simulator.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/csma-duplex-net-device.h"
#include "ns3/csma-duplex-channel.h"
#include "ns3/pcap-writer.h"
#include "ns3/config.h"
#include "ns3/packet.h"
#include "ns3/names.h"
#include <string>

namespace ns3 {

CsmaDuplexHelper::CsmaDuplexHelper ()
{
  m_queueFactory.SetTypeId ("ns3::DropTailQueue");
  m_deviceFactory.SetTypeId ("ns3::CsmaDuplexNetDevice");
  m_channelFactory.SetTypeId ("ns3::CsmaDuplexChannel");
}

void 
CsmaDuplexHelper::SetQueue (std::string type,
                      std::string n1, const AttributeValue &v1,
                      std::string n2, const AttributeValue &v2,
                      std::string n3, const AttributeValue &v3,
                      std::string n4, const AttributeValue &v4)
{
  m_queueFactory.SetTypeId (type);
  m_queueFactory.Set (n1, v1);
  m_queueFactory.Set (n2, v2);
  m_queueFactory.Set (n3, v3);
  m_queueFactory.Set (n4, v4);
}

void 
CsmaDuplexHelper::SetDeviceAttribute (std::string n1, const AttributeValue &v1)
{
  m_deviceFactory.Set (n1, v1);
}

void 
CsmaDuplexHelper::SetChannelAttribute (std::string n1, const AttributeValue &v1)
{
  m_channelFactory.Set (n1, v1);
}

void 
CsmaDuplexHelper::SetDeviceParameter (std::string n1, const AttributeValue &v1)
{
  SetDeviceAttribute (n1, v1);
}

void 
CsmaDuplexHelper::SetChannelParameter (std::string n1, const AttributeValue &v1)
{
  SetChannelAttribute (n1, v1);
}

void 
CsmaDuplexHelper::EnablePcap (std::string filename, uint32_t nodeid, uint32_t deviceid, bool promiscuous)
{
  std::ostringstream oss;
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::CsmaDuplexNetDevice/";
  Config::MatchContainer matches = Config::LookupMatches (oss.str ());
  if (matches.GetN () == 0)
    {
      return;
    }
  oss.str ("");
  oss << filename << "-" << nodeid << "-" << deviceid << ".pcap";
  Ptr<PcapWriter> pcap = Create<PcapWriter> ();
  pcap->Open (oss.str ());
  pcap->WriteEthernetHeader ();
  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid;
  if (promiscuous)
    {
      oss << "/$ns3::CsmaDuplexNetDevice/PromiscSniffer";
    }
  else
    {
      oss << "/$ns3::CsmaDuplexNetDevice/Sniffer";
    }
  Config::ConnectWithoutContext (oss.str (), MakeBoundCallback (&CsmaDuplexHelper::SniffEvent, pcap));
}

void 
CsmaDuplexHelper::EnablePcap (std::string filename, NetDeviceContainer d, bool promiscuous)
{
  for (NetDeviceContainer::Iterator i = d.Begin (); i != d.End (); ++i)
    {
      Ptr<NetDevice> dev = *i;
      EnablePcap (filename, dev->GetNode ()->GetId (), dev->GetIfIndex (), promiscuous);
    }
}

void 
CsmaDuplexHelper::EnablePcap (std::string filename, Ptr<NetDevice> nd, bool promiscuous)
{
  EnablePcap (filename, nd->GetNode ()->GetId (), nd->GetIfIndex (), promiscuous);
}

void 
CsmaDuplexHelper::EnablePcap (std::string filename, std::string ndName, bool promiscuous)
{
  Ptr<NetDevice> nd = Names::Find<NetDevice> (ndName);
  EnablePcap (filename, nd->GetNode ()->GetId (), nd->GetIfIndex (), promiscuous);
}

void
CsmaDuplexHelper::EnablePcap (std::string filename, NodeContainer n, bool promiscuous)
{
  NetDeviceContainer devs;
  for (NodeContainer::Iterator i = n.Begin (); i != n.End (); ++i)
    {
      Ptr<Node> node = *i;
      for (uint32_t j = 0; j < node->GetNDevices (); ++j)
        {
          devs.Add (node->GetDevice (j));
        }
    }
  EnablePcap (filename, devs, promiscuous);
}

void
CsmaDuplexHelper::EnablePcapAll (std::string filename, bool promiscuous)
{
  EnablePcap (filename, NodeContainer::GetGlobal (), promiscuous);
}

void 
CsmaDuplexHelper::EnableAscii (std::ostream &os, uint32_t nodeid, uint32_t deviceid)
{
  Packet::EnablePrinting ();
  std::ostringstream oss;
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::CsmaDuplexNetDevice/MacRx";
  Config::Connect (oss.str (), MakeBoundCallback (&CsmaDuplexHelper::AsciiRxEvent, &os));
  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::CsmaDuplexNetDevice/TxQueue/Enqueue";
  Config::Connect (oss.str (), MakeBoundCallback (&CsmaDuplexHelper::AsciiEnqueueEvent, &os));
  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::CsmaDuplexNetDevice/TxQueue/Dequeue";
  Config::Connect (oss.str (), MakeBoundCallback (&CsmaDuplexHelper::AsciiDequeueEvent, &os));
  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::CsmaDuplexNetDevice/TxQueue/Drop";
  Config::Connect (oss.str (), MakeBoundCallback (&CsmaDuplexHelper::AsciiDropEvent, &os));
}
void 
CsmaDuplexHelper::EnableAscii (std::ostream &os, NetDeviceContainer d)
{
  for (NetDeviceContainer::Iterator i = d.Begin (); i != d.End (); ++i)
    {
      Ptr<NetDevice> dev = *i;
      EnableAscii (os, dev->GetNode ()->GetId (), dev->GetIfIndex ());
    }
}
void
CsmaDuplexHelper::EnableAscii (std::ostream &os, NodeContainer n)
{
  NetDeviceContainer devs;
  for (NodeContainer::Iterator i = n.Begin (); i != n.End (); ++i)
    {
      Ptr<Node> node = *i;
      for (uint32_t j = 0; j < node->GetNDevices (); ++j)
        {
          devs.Add (node->GetDevice (j));
        }
    }
  EnableAscii (os, devs);
}

void
CsmaDuplexHelper::EnableAsciiAll (std::ostream &os)
{
  EnableAscii (os, NodeContainer::GetGlobal ());
}

NetDeviceContainer
CsmaDuplexHelper::Install (Ptr<Node> node) const
{
  Ptr<CsmaDuplexChannel> channel = m_channelFactory.Create ()->GetObject<CsmaDuplexChannel> ();
  return Install (node, channel);
}

NetDeviceContainer
CsmaDuplexHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return Install (node);
}

NetDeviceContainer
CsmaDuplexHelper::Install (Ptr<Node> node, Ptr<CsmaDuplexChannel> channel) const
{
  return NetDeviceContainer (InstallPriv (node, channel));
}

NetDeviceContainer
CsmaDuplexHelper::Install (Ptr<Node> node, std::string channelName) const
{
  Ptr<CsmaDuplexChannel> channel = Names::Find<CsmaDuplexChannel> (channelName);
  return NetDeviceContainer (InstallPriv (node, channel));
}

NetDeviceContainer
CsmaDuplexHelper::Install (std::string nodeName, Ptr<CsmaDuplexChannel> channel) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return NetDeviceContainer (InstallPriv (node, channel));
}

NetDeviceContainer
CsmaDuplexHelper::Install (std::string nodeName, std::string channelName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  Ptr<CsmaDuplexChannel> channel = Names::Find<CsmaDuplexChannel> (channelName);
  return NetDeviceContainer (InstallPriv (node, channel));
}

NetDeviceContainer 
CsmaDuplexHelper::Install (const NodeContainer &c) const
{
  Ptr<CsmaDuplexChannel> channel = m_channelFactory.Create ()->GetObject<CsmaDuplexChannel> ();

  return Install (c, channel);
}

NetDeviceContainer 
CsmaDuplexHelper::Install (const NodeContainer &c, Ptr<CsmaDuplexChannel> channel) const
{
  NetDeviceContainer devs;

  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); i++)
    {
      devs.Add (InstallPriv (*i, channel));
    }

  return devs;
}

NetDeviceContainer 
CsmaDuplexHelper::Install (const NodeContainer &c, std::string channelName) const
{
  Ptr<CsmaDuplexChannel> channel = Names::Find<CsmaDuplexChannel> (channelName);
  return Install (c, channel);
}

Ptr<NetDevice>
CsmaDuplexHelper::InstallPriv (Ptr<Node> node, Ptr<CsmaDuplexChannel> channel) const
{
  Ptr<CsmaDuplexNetDevice> device = m_deviceFactory.Create<CsmaDuplexNetDevice> ();
  device->SetAddress (Mac48Address::Allocate ());
  node->AddDevice (device);
  Ptr<Queue> queue = m_queueFactory.Create<Queue> ();
  device->SetQueue (queue);
  device->Attach (channel);

  return device;
}

void 
CsmaDuplexHelper::InstallStar (Ptr<Node> hub, NodeContainer spokes, 
                         NetDeviceContainer& hubDevices, NetDeviceContainer& spokeDevices)
{
  for (uint32_t i = 0; i < spokes.GetN (); ++i)
    {
      NodeContainer nodes (hub, spokes.Get (i));
      NetDeviceContainer nd = Install (nodes);
      hubDevices.Add (nd.Get (0));
      spokeDevices.Add (nd.Get (1));
    }
}

void 
CsmaDuplexHelper::InstallStar (std::string hubName, NodeContainer spokes, 
                         NetDeviceContainer& hubDevices, NetDeviceContainer& spokeDevices)
{
  Ptr<Node> hub = Names::Find<Node> (hubName);
  InstallStar (hub, spokes, hubDevices, spokeDevices);
}

void 
CsmaDuplexHelper::SniffEvent (Ptr<PcapWriter> writer, Ptr<const Packet> packet)
{
  writer->WritePacket (packet);
}

void 
CsmaDuplexHelper::AsciiEnqueueEvent (std::ostream *os, std::string path, Ptr<const Packet> packet)
{
  *os << "+ " << Simulator::Now ().GetSeconds () << " ";
  *os << path << " " << *packet << std::endl;
}

void 
CsmaDuplexHelper::AsciiDequeueEvent (std::ostream *os, std::string path, Ptr<const Packet> packet)
{
  *os << "- " << Simulator::Now ().GetSeconds () << " ";
  *os << path << " " << *packet << std::endl;
}

void 
CsmaDuplexHelper::AsciiDropEvent (std::ostream *os, std::string path, Ptr<const Packet> packet)
{
  *os << "d " << Simulator::Now ().GetSeconds () << " ";
  *os << path << " " << *packet << std::endl;
}

void 
CsmaDuplexHelper::AsciiRxEvent (std::ostream *os, std::string path, Ptr<const Packet> packet)
{
  *os << "r " << Simulator::Now ().GetSeconds () << " ";
  *os << path << " " << *packet << std::endl;
}

} // namespace ns3
