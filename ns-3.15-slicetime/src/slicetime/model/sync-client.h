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

#ifndef SYNC_CLIENT_H
#define SYNC_CLIENT_H

#include "ns3/assert.h"
#include "ns3/object.h"
#include "ns3/ipv4-address.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <string>


namespace ns3 {

  #define MAX2(a, b) (((a) > (b)) ? (a) : (b))
  #define MAX4(a, b, c, d) MAX2(MAX2(a,b),MAX2(c,d))

  namespace SyncCom {
    //following are the structs for communication between clients and servers

    /*
     * If a client registers at the server, the client registers himself
     * with a unique client id
     */

    static const int CLIENT_TYPE_LOCAL_XDOMAIN = 0;
    static const int CLIENT_TYPE_REMOTE_XDOMAIN = 1;
    static const int CLIENT_TYPE_REMOTE_SIMULATION = 2;
    static const int CLIENT_TYPE_TEST = 133;
    static const int CLIENT_TYPE_OTHER = 254;
    static const int CLIENT_TYPE_UNKNOWN = 255;

    static const int CLIENT_DESCR_LENGTH = 100; //length of client description in byte

    struct RegisterClient {

      u_int16_t clientID; //the id of the client that wishes to register. must be unique!
      u_int8_t clientType; // Type of client: 0 = locally managed Xen Client, 1 = remotely managed  Xen client, 2 = remote simulation client, 254 = other, 255 = unknown
      char client_Description[CLIENT_DESCR_LENGTH]; //arbitrary client description.

    };

    /*
     * unregisters a client from the synchronization server
     *
     */

    static const int UNREGISTER_REASON_REGULAR = 0;
    static const int UNREGISTER_REASON_OUT_OF_SYNC = 1;
    static const int UNREGISTER_REASON_OTHER = 2;

    struct UnregisterClient {

    u_int16_t clientID; //the id of the client to unregisterâ
    u_int8_t reason; //the reason why a client unregisters, shoudl correspond to constantââ
  };

  /*
     * tell all clients that they're allowed to execute a certain amount of virtual time
   *
   * IMPLEMENTATION NOTE: - Distribute this to all remote systems with a UDP broadcast!
   *  					- Afterwards, use subsequent hypercalls to notify local guest domains!
   *
   * NOTE: We probably need periodic retransmissions in order to compensate for lost udp-packets
   * otherwise, the execution will hang in a deadlock! (server waits for clients to finish execution, one client waits
   * for server to announce next period)â
   */

  struct RunPermission {

    u_int32_t periodId; // used to identify the periods
    u_int32_t runTime;  //the runtime (in microseconds) the clients are allowed to continue their execution  
  };


  /*
   *  tell the server that a client has finished the execution of a provided period
   *
   *  IMPLEMENTATION NOTE: If transmitted over UDP, this information must be rebroadcasted periodically
   *  in order to avoid deadlocking (if a finished packet gets lost)
   */

  struct Finished {
     u_int32_t periodId; //the id of the period whose execution has been finished;
     u_int32_t runTime; //the virtual runtime that was assigned in this period (microseconds)
     u_int32_t realTime; //the time actually needed for executing this period (microseconds)=> useful for statistics!
     u_int16_t clientId; // (clientId is the last field here, since it better like that in the xen kernel module)
                         //    (this was changed consitently on 2009-06-04)
  };

  //definition of packet type ids (should correspond to introductionary comment above!)

  static const int PACKETTYPE_REGISTER = 0; //used to register a client at a server - note: we could use tcp here als well
  static const int PACKETTYPE_UNREGISTER = 1; //used to unregister a client - note: we could use tcp for this as well.
  static const int PACKETTYPE_RUNPERMISSION = 2; //used to announce that the clients can continue with their execution. UDP-Broadcast
  static const int PACKETTYPE_FINISHED = 3; //used by clients to announce they finished execution of a period. UDP-Unicast


  /*
   *  this packet specifies the structure of the UDP packets
   *
   */

  struct SyncPacket {

    u_int32_t seqNr; //a seqNr to distinguish the packets
    u_int8_t  packetType; //contains packet type information
    //u_int16_t length; //length of data
    u_int8_t data[]; //should contain one of the Structs â

  };
} // namespace SyncCom


/**
 * \brief Helper class for SyncSimulatorImpl which manages the connection to the synchronization server
 * 
 * This class implements a simple protocol which can be used to synchronize ns-3 with other simulation 
 * toolkits or virtual machines.
 *
 * The clients register at a synchronization server. This server broadcasts a runpermission packet which allows all clients
 * to run for a specific amount of time. When the clients finished this timeslice, they send a finished packet 
 * to the server and after the server received such a packet from all registered clients, it sends the next runpermission.
 * If a client wants to leave, it sends an unregister packet. For efficiency reasons all of that communcation takes place 
 * via udp, which means on the other side that packets might get lost. To accommodate that, the client resends the finish packet 
 * after some configurable time if it receives not run permission.
 * The detailed format of this protocol can be found in src/simulator/sync-client.h
 *
 * The configuration is done with the following attributes:
 * - \em ClientPort and \em ClientAddress sets the port and address to receive packets on. 
     This has to be the same (broadcast) address and port for all synchronization clients. 
     In order to allow multiple instances to listen on the same broadcast address and port 
     on the same machine, SO_REUSEADDR is used for the socket.
 * - \em ServerPort and \em ServerAddress define how to reach the synchronization server
 * - \em ClientId is the id with which the client registers at the server
 * - \em ClientDescription is a name for the client (for debugging and logging in the server)
 * - \em RecvTimeout is the number of seconds after which the client shall resend a finish packet if no 
 *   runpermission is received. If \em RecvTimeout is 0, packets are not resent at all.
 * 
 * \see SyncSimulatirImpl
 */

class SyncClient : public Object
{
  public:
   static TypeId GetTypeId (void);

   SyncClient ();
   virtual ~SyncClient ();

   /**
    * Sets up the socket and sends a register packet to the synchronization server.
    */
   void ConnectAndSendRegister();

   /**
    * Sends an unregister packet to the synchronization server and shuts down the socket.
    */
   void SendUnregAndDisconnect();

   /**
    * Sends a finished packet to the synchronization server.
    *
    * \param runTime the time that was assigned in the last timeslice (in microseconds)
    * \param realTime the time which was actually needed for the simulation of the timeslice (in microseconds)
    */
   void SendFinished(uint32_t runTime, uint32_t realTime);

   /**
    * Waits for the server to send a run permission packet and if that does not happen after 
    * \em RecvTimeout seconds, the last sent packet is sent again.
    *
    * \returns the run time (in microseconds) of the next timeslice
    */
   uint32_t WaitForRunPermission();

 private:
   // sets the sequence number of a packet structure and sends it to the server
   void SendPacket (struct SyncCom::SyncPacket *packet, size_t len);

   // configuration attributes
   uint16_t m_clientPort;
   uint16_t m_serverPort;
   Ipv4Address m_clientAddress;
   Ipv4Address m_serverAddress;
   uint16_t m_clientId;
   std::string m_clientDescription;
   uint16_t m_recvTimeout;

   // socket to send and receive on
   int m_sock;
   
   // counter for the sequence number of packets
   uint32_t m_seqNr;
   
   // counter of the period ids the server sends
   uint32_t m_periodId;

   // length, complete packet and data part for all four messages
   // (is malloced only once in the constructor)
   int m_regPacket_len;
   struct SyncCom::SyncPacket *m_regPacket;
   struct SyncCom::RegisterClient *m_regPacket_data;
   int m_unregPacket_len;
   struct SyncCom::SyncPacket *m_unregPacket;
   struct SyncCom::UnregisterClient *m_unregPacket_data;
   int m_finishedPacket_len;
   struct SyncCom::SyncPacket *m_finishedPacket;
   struct SyncCom::Finished *m_finishedPacket_data;
   int m_runpermPacket_recv_len;
   int m_runpermPacket_real_len;
   struct SyncCom::SyncPacket *m_runpermPacket;
   struct SyncCom::RunPermission *m_runpermPacket_data;
   struct sockaddr_in m_dest;

   // pointer to the last sent packet and its length
   struct SyncCom::SyncPacket *m_lastPacket;
   int m_lastPacketLen;
};

} // namespace ns3

#endif /* SYNC_CLIENT_H */
