/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 RWTH Aachen University
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
 * Author: Hendrik vom Lehn <vomlehn@cs.rwth-aachen.de>
 */

#include "wifi-emu-comm-udp.h"
#include "wifi-emu-msg.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/global-value.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define DEBUG_TIME 0

NS_LOG_COMPONENT_DEFINE("WifiEmuCommUdp");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (WifiEmuCommUdp);

GlobalValue g_wifiEmuCommRecvAddress = GlobalValue ("WifiEmuCommUdpReceiveAddress",
  "The address on which packets for the wifi emulation are received",
  Ipv4AddressValue ("0.0.0.0"),
  MakeIpv4AddressChecker());

GlobalValue g_wifiEmuCommRecvPort = GlobalValue ("WifiEmuCommUdpReceivePort",
  "The port on which packets for the wifi emulation are received",
  UintegerValue (1984),
  MakeUintegerChecker<uint16_t>());


TypeId
WifiEmuCommUdp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::WifiEmuCommUdp")
    .SetParent<WifiEmuComm> ()
    ;
  return tid;
}


WifiEmuCommUdp::WifiEmuCommUdp ()
{
  NS_LOG_FUNCTION_NOARGS ();

  // get port and address to receive on (are stored in a global variable)
  UintegerValue localPort;
  g_wifiEmuCommRecvPort.GetValue (localPort);
  Ipv4AddressValue localAddress;
  g_wifiEmuCommRecvAddress.GetValue (localAddress);

  // create socket
  NS_LOG_LOGIC("Creating socket to receive wifi emulation messages");
  m_sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_IP);
  NS_ABORT_MSG_IF (m_sock < 0, "WifiEmuCommUdp::CommInit(): Could not create socket!");

  // bind socket
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl ((localAddress.Get ()).Get ());
  sa.sin_port = htons (localPort.Get ());
  int bound = bind (m_sock, (struct sockaddr *)&sa, sizeof (struct sockaddr));
  NS_ABORT_MSG_IF (bound < 0, "WifiEmuCommUdp::CommInit(): Could not bind socket!");

  // start reader thread (in WifiEmuComm)
  CommStart();
}

WifiEmuCommUdp::~WifiEmuCommUdp ()
{
  NS_LOG_FUNCTION_NOARGS ();

  // close socket
  close(m_sock);
}

void WifiEmuCommUdp::CommSend(uint16_t client_id, uint8_t* msg, int len)
{
  NS_LOG_FUNCTION (client_id << msg << len);

  // get sockaddr
  std::map<uint16_t, struct sockaddr_in>::iterator it;
  it = m_allAddresses.find(client_id);
  if(it == m_allAddresses.end ())
        {
          NS_LOG_LOGIC("Unknown clientid, cannot send packet.");
          free(msg);
          return;
        }
  struct sockaddr_in sa = it->second;

#if DEBUG_TIME
  if(((struct wifi_emu_msg_header *)msg)->type == WIFI_EMU_MSG_FRAME)
    {
    struct timeval t;
    gettimeofday(&t, NULL);
    printf("SIM_SendToDriver\t%lu\t%06lu\n", t.tv_sec, t.tv_usec);
    fflush(stdout);
    }
#endif

  // send packet
  socklen_t sa_size = sizeof(sa);
  if(sendto(m_sock, msg, len, 0, (struct sockaddr*)&sa, sa_size) != len)
    {
    NS_LOG_WARN("Could not send UDP datagram: " << strerror(errno));
    free(msg);
    return;
    }

  free(msg);
}

int WifiEmuCommUdp::CommRecv(uint16_t &client_id, uint8_t &type, uint8_t* msg)
{
  NS_LOG_FUNCTION (client_id << msg);

  struct sockaddr_in sa;
  int len;

  // receive packet
  for(;;)
    {
    socklen_t sa_size = sizeof(sa);
    len = recvfrom(m_sock, msg, WIFI_EMU_MSG_LEN, 0, (struct sockaddr*)&sa, &sa_size);

#if DEBUG_TIME
  if(((struct wifi_emu_msg_header *)msg)->type == WIFI_EMU_MSG_FRAME)
      {
      struct timeval t;
      gettimeofday(&t, NULL);
      printf("SIM_GotFromDriver\t%lu\t%06lu\n", t.tv_sec, t.tv_usec);
      fflush(stdout);
      }
#endif

    if (len < 0)
      {
        NS_LOG_WARN("Could not receive UDP datagram: " << strerror(errno));
        continue;
      }
    if (len < (int)sizeof(struct wifi_emu_msg_header))
      {
        NS_LOG_WARN("Received UDP datagram was too small");
        continue;
      }
    break;
    }
  NS_LOG_LOGIC("Received UDP datagram");
  struct wifi_emu_msg_header* header = (struct wifi_emu_msg_header*)msg;

  // store sockaddr on register 
  if(header->type == WIFI_EMU_MSG_REQ_REGISTER)
    {
    if(m_allAddresses.count(ntohs(header->client_id)) == 0)
      {
      m_allAddresses.insert(std::pair<uint16_t, struct sockaddr_in> (ntohs(header->client_id), sa));
      NS_LOG_LOGIC("Inserted socket address for client " << ntohs(header->client_id));
      }
    }
  
  // remove sockaddr on unregister
  if(header->type == WIFI_EMU_MSG_UNREGISTER)
    {
    if(m_allAddresses.count(ntohs(header->client_id)) > 0)
      {
      m_allAddresses.erase(client_id);
      NS_LOG_LOGIC("Removed socket address for client " << ntohs(header->client_id));
      }
    }

  // return data
  client_id = ntohs(header->client_id);
  type = header->type;
  return len;
}

} // namespace ns3



