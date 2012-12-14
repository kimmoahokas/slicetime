/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 RWTH Aachen University
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
 *         Elias Weingaertner <weingaertner@cs.rwth-aachen.de>
 */


#include "sync-client.h"
#include "ns3/assert.h"
#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

#include <string.h>
#include <stdio.h>
#include <cstring>
#include <errno.h>
#include <stdlib.h>

NS_LOG_COMPONENT_DEFINE ("SyncClient");

namespace ns3 {


NS_OBJECT_ENSURE_REGISTERED (SyncClient);

TypeId 
SyncClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SyncClient")
    .SetParent<Object> ()
    .AddConstructor<SyncClient> ()
    .AddAttribute ("ClientAddress",
                   "The address on which the client listens for sync data",
                   Ipv4AddressValue ("0.0.0.0"),
                   MakeIpv4AddressAccessor (&SyncClient::m_clientAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("ClientPort",
                   "The port on which the client listens for sync data",
                   UintegerValue (17544),
                   MakeUintegerAccessor (&SyncClient::m_clientPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ServerPort",
                   "The port to which the client connects on the server side",
                   UintegerValue (17543),
                   MakeUintegerAccessor (&SyncClient::m_serverPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ServerAddress",
                   "The address to which the client connects",
                   Ipv4AddressValue ("127.0.0.1"),
                   MakeIpv4AddressAccessor (&SyncClient::m_serverAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("ClientId",
                   "The ID with which the client registers at the sync server",
                   UintegerValue (13),
                   MakeUintegerAccessor (&SyncClient::m_clientId),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ClientDescription",
                   "A description of this client",
                   StringValue ("ns-3 client"),
                   MakeStringAccessor (&SyncClient::m_clientDescription),
                   MakeStringChecker ())
    .AddAttribute ("RecvTimeout",
                   "The number of seconds after which the finish packets shall be resent if no run permission is received. 0 means don't resend.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&SyncClient::m_recvTimeout),
                   MakeUintegerChecker<uint16_t> ())
    ;
  return tid;
}

SyncClient::SyncClient ()
{
  NS_LOG_FUNCTION_NOARGS ();

  m_sock = -1;
  m_seqNr = 0;
  m_periodId = 0;
  m_lastPacket = NULL;
  
  // create buffers for packets so they don't have to be malloced each time
  m_regPacket_len = sizeof (SyncCom::SyncPacket) + sizeof (SyncCom::RegisterClient);
  m_regPacket = (struct SyncCom::SyncPacket*) malloc (m_regPacket_len);
  NS_ABORT_MSG_IF(m_regPacket == NULL, "SyncClient::SyncClient(): malloc failed");
  m_regPacket->packetType = SyncCom::PACKETTYPE_REGISTER;
  m_regPacket_data = (SyncCom::RegisterClient*)&(m_regPacket->data);
  m_unregPacket_len = sizeof (SyncCom::SyncPacket) + sizeof (SyncCom::UnregisterClient);
  m_unregPacket = (struct SyncCom::SyncPacket*) malloc (m_unregPacket_len);
  NS_ABORT_MSG_IF(m_unregPacket == NULL, "SyncClient::SyncClient(): malloc failed");
  m_unregPacket->packetType = SyncCom::PACKETTYPE_UNREGISTER;
  m_unregPacket_data = (SyncCom::UnregisterClient*)&(m_unregPacket->data);
  m_finishedPacket_len = sizeof (SyncCom::SyncPacket) + sizeof (SyncCom::Finished);
  m_finishedPacket = (struct SyncCom::SyncPacket*) malloc (m_finishedPacket_len);
  NS_ABORT_MSG_IF(m_finishedPacket == NULL, "SyncClient::SyncClient(): malloc failed");
  m_finishedPacket->packetType = SyncCom::PACKETTYPE_FINISHED;
  m_finishedPacket_data = (struct SyncCom::Finished*)&(m_finishedPacket->data);

  // create buffer for runperm-packet with maximum packet size
  // (the buffer has the biggest size of all four packet types, 
  //  in case some other type of packet is received (which should not happen))
  m_runpermPacket_real_len = sizeof (SyncCom::SyncPacket) + sizeof (SyncCom::RunPermission);
  m_runpermPacket_recv_len = MAX4(m_regPacket_len, m_unregPacket_len, m_runpermPacket_real_len, m_finishedPacket_len);
  m_runpermPacket = (struct SyncCom::SyncPacket*) malloc (m_runpermPacket_recv_len);
  NS_ABORT_MSG_IF(m_runpermPacket == NULL, "SyncClient::SyncClient(): malloc failed");
  m_runpermPacket_data = (struct SyncCom::RunPermission*)&(m_runpermPacket->data);
}

SyncClient::~SyncClient ()
{
  NS_LOG_FUNCTION_NOARGS ();

  // free the packet memory
  free(m_regPacket);
  free(m_unregPacket);
  free(m_runpermPacket);
  free(m_finishedPacket);
}

void SyncClient::ConnectAndSendRegister ()
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_LOG_LOGIC("Creating socket for synchronization");
  // create socket
  {
    m_sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_IP);
    NS_ABORT_MSG_IF (m_sock < 0, "SyncClient::ConnectAndSendRegister(): Could not create socket!");

    // set SO_REUSEADDR so that multiple instance can receive on the same port
    // (works only for broadcast)
    int val_on = 1;
    int sock_reuse=setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &val_on, sizeof(val_on));
    NS_ABORT_MSG_IF(sock_reuse != 0, "SyncClient::ConnectAndSendRegister(): Could not enable address reuse!");

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(m_clientAddress.Get ());
    sa.sin_port = htons (m_clientPort);

    int bound = bind (m_sock, (struct sockaddr *)&sa, sizeof (struct sockaddr));
    NS_ABORT_MSG_IF(bound < 0, "SyncClient::ConnectAndSendRegister(): Could not bind socket!");
  }

  // init struct for sending
  m_dest.sin_family = AF_INET;
  m_dest.sin_port = htons(m_serverPort);
  m_dest.sin_addr.s_addr = htonl(m_serverAddress.Get ());

  NS_LOG_LOGIC("Sending register packet to sync server");
  // send register packet
  m_regPacket_data->clientID = htons(m_clientId);
  m_regPacket_data->clientType = SyncCom::CLIENT_TYPE_REMOTE_SIMULATION;
  snprintf(m_regPacket_data->client_Description, SyncCom::CLIENT_DESCR_LENGTH, "%s", m_clientDescription.c_str());
  SendPacket(m_regPacket, m_regPacket_len);
}

void SyncClient::SendUnregAndDisconnect()
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT_MSG (m_sock >= 0, "SyncClient::SendUnregAndDisconnect(): Socket is not connected!");

  NS_LOG_LOGIC("Sending unregister packet to sync server");
  // send unregister packet
  m_unregPacket_data->clientID = htons(m_clientId);
  m_unregPacket_data->reason = SyncCom::UNREGISTER_REASON_REGULAR;
  SendPacket(m_unregPacket, m_unregPacket_len);

  NS_LOG_LOGIC("Closing the socket used for synchronization");
  // close socket
  close(m_sock);
  m_sock = -1;
  m_seqNr = 0;
  m_periodId = 0;
}

void SyncClient::SendFinished (uint32_t runTime, uint32_t realTime)
{
  NS_LOG_FUNCTION (runTime << realTime);

  // construct and send finished packet
  NS_LOG_LOGIC("Sending Finished packet (periodid = " << m_periodId << ")");
  m_finishedPacket_data->clientId = htons(m_clientId);
  m_finishedPacket_data->periodId = htonl(m_periodId);
  m_finishedPacket_data->runTime = htonl(runTime);
  m_finishedPacket_data->realTime = htonl(realTime);
  SendPacket(m_finishedPacket, m_finishedPacket_len);
}

uint32_t SyncClient::WaitForRunPermission ()
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_ASSERT_MSG (m_sock >= 0, "SyncClient::WaitForRunPermission(): Socket is not connected!");

  if (m_recvTimeout == 0) // don't resend packets
    {
    // go into infite loop and wait for runpermission packet
    for(;;)
      {
      NS_LOG_LOGIC("Waiting for run permission packet");
 
      // receive data
      int bytes_received = recvfrom(m_sock, m_runpermPacket, m_runpermPacket_recv_len, 0, NULL, 0);
      NS_LOG_LOGIC("Received something");

      if (bytes_received == m_runpermPacket_real_len)
        {
        if (m_runpermPacket->packetType == SyncCom::PACKETTYPE_RUNPERMISSION)
          {
          uint32_t runTime = ntohl(m_runpermPacket_data->runTime);
          uint32_t recvPeriodId = ntohl(m_runpermPacket_data->periodId);
 
          // run permissions may be resent, so we have to check that we received a fresh packet
          if(recvPeriodId > m_periodId)
            {
            NS_LOG_LOGIC("Got run permission (runTime = " << runTime << ", periodId = " << recvPeriodId << ")");
            m_periodId = recvPeriodId;
            return runTime;
            }
          else
            NS_LOG_LOGIC("Got an old run permission again, ignoring ...");
          }
         else
          NS_LOG_LOGIC("Wrong packet type (!= run permission), ignoring ...");
        }
      else
        NS_LOG_LOGIC("Wrong packet length for run permission, ignoring ...");
      }
    }
  else // resend packets after timeout
    {
    // go into infite loop and wait for runpermission packet
    for(;;)
      {
      NS_LOG_LOGIC("Waiting for run permission packet");
 
      // use select to have a timeout if no data arrives
      struct timeval timeout;
      timeout.tv_sec = m_recvTimeout;
      timeout.tv_usec = 0;
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(m_sock, &readfds);
      int select_val = select(m_sock+1, &readfds, NULL, NULL, &timeout);
      NS_ABORT_MSG_IF(select_val < 0, "SyncClient::WaitForRunPermission(): select failed (could not read from sync socket)");
 
      // if nothing changed, we had an timeout and have to retransmit the last packet
      if(select_val == 0)
        {
        NS_LOG_LOGIC("Timeout occured while waiting for run permission, resending last packet");
        int bytes_sent = sendto(m_sock, m_lastPacket, m_lastPacketLen, 0, (struct sockaddr*)&m_dest, sizeof (m_dest));
        NS_ABORT_MSG_IF(bytes_sent == -1, "SyncClient::WaitForRunPermsission(): Could not send synchronization packet!");
        }
      // otherwise, we received some data
      else
        {
        NS_LOG_LOGIC("Received something");
        int bytes_received = recvfrom(m_sock, m_runpermPacket, m_runpermPacket_recv_len, 0, NULL, 0);
 
        if (bytes_received == m_runpermPacket_real_len)
          {
          if (m_runpermPacket->packetType == SyncCom::PACKETTYPE_RUNPERMISSION)
            {
            uint32_t runTime = ntohl(m_runpermPacket_data->runTime);
            uint32_t recvPeriodId = ntohl(m_runpermPacket_data->periodId);
 
            // run permissions may be resent, so we have to check that we received a fresh packet
            if(recvPeriodId > m_periodId)
              {
              NS_LOG_LOGIC("Got run permission (runTime = " << runTime << ", periodId = " << recvPeriodId << ")");
              m_periodId = recvPeriodId;
              return runTime;
              }
            else
              NS_LOG_LOGIC("Got an old run permission again, ignoring ...");
            }
           else
            NS_LOG_LOGIC("Wrong packet type (!= run permission), ignoring ...");
          }
        else
          NS_LOG_LOGIC("Wrong packet length for run permission, ignoring ...");
        }
      }
    }
}

void SyncClient::SendPacket (struct SyncCom::SyncPacket *packet, size_t len)
{
  NS_LOG_FUNCTION (packet << len);

  NS_ASSERT_MSG (m_sock >= 0, "SyncClient::SendPacket(): Socket is not connected!");

  // increase the sequence number and write it to the packet
  m_seqNr++;
  packet->seqNr = htonl(m_seqNr);

  // send packet
  int bytes_sent = sendto(m_sock, packet, len, 0, (struct sockaddr*)&m_dest, sizeof (m_dest));
  NS_ABORT_MSG_IF(bytes_sent == -1, "SyncClient::SendPacket(): Could not send synchronization packet!");
  NS_LOG_LOGIC("Sent a packet to the synchronization server");

  // save pointer to this packet
  m_lastPacket = packet;
  m_lastPacketLen = len;
}





} // namespace ns3
