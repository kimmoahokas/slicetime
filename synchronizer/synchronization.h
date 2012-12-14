#ifndef SYNCHRONIZATION_H_
#define SYNCHRONIZATION_H_
#include <cstdlib>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
/**
 *
 * This file contains important structs that are used for the communication between the xen-synchronizer (in server
 * mode) and clients using the barrier-sync algorithm.
 *
 * The basic communication actions are:
 *
 * 	 a) a client registers himself to be synchronized (PacketType=0);
 *   b) a client unregisters himfself to be synchronized (PacketType=1)
 *   c) a server permits the clients to run for a time period X (PacketType=2)
 *   d) a client reports to a server that it has finished execution of period X (PacketType=3)
 *
 */


//following are the structs for communication between clients and servers

/*
 * If a client registers at the server, the client registers himself
 * with a unique client id
 */

#define CLIENT_TYPE_LOCAL_XDOMAIN 0
#define CLIENT_TYPE_REMOTE_XDOMAIN 1
#define CLIENT_TYPE_REMOTE_SIMULATION 2
#define CLIENT_TYPE_TEST 133
#define CLIENT_TYPE_OTHER 254
#define CLIENT_TYPE_UNKNOWN 255

#define CLIENT_DESCR_LENGTH 100 //length of client description in byte

struct COM_RegisterClient {

	u_int16_t clientID; //the id of the client that wishes to register. must be unique!
	u_int8_t clientType; // Type of client: 0 = locally managed Xen Client, 1 = remotely managed  Xen client, 2 = remote simulation client, 254 = other, 255 = unknown
	char client_Description[CLIENT_DESCR_LENGTH]; //arbitrary client description.

};

/*
 * unregisters a client from the synchronization server
 *
 */

#define UNREGISTER_REASON_REGULAR 0
#define UNREGISTER_REASON_OUT_OF_SYNC 1
#define UNREGISTER_REASON_OTHER 2

struct COM_UnregisterClient {

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

/*
 * NOTE 06/04/2009 - We globally switch to MICROSECONDS for the accuracy level!
 *
 * This has been consistently changed in the kernel sync module and the ns-3 implementation
 * as well as in the synchronizer
 * 
 */
struct COM_RunPermission {

	u_int32_t periodId; // used to identify the periods
	u_int32_t runTime;  //the runtime the clients are allowed to continue their execution in MICROSECONDS!!! 


};


/*
 *  tell the server that a client has finished the execution of a provided period
 *
 *  IMPLEMENTATION NOTE: If transmitted over UDP, this information must be rebroadcasted periodically
 *  in order to avoid deadlocking (if a finished packet gets lost)
 */

struct COM_Finished {
	 u_int32_t periodId; //the id of the period whose execution has been finished;
	 u_int32_t runTime; //the virtual runtime that was assigned in this period (microseconds)
	 u_int32_t realTime; //the time actually needed for executing this period (microseconds)=> useful for statistics!
	 u_int16_t clientId;

};

//definition of packet type ids (should correspond to introductionary comment above!)

#define PACKETTYPE_REGISTER 0 //used to register a client at a server - note: we could use tcp here als well
#define PACKETTYPE_UNREGISTER 1 //used to unregister a client - note: we could use tcp for this as well.
#define PACKETTYPE_RUNPERMISSION 2 //used to announce that the clients can continue with their execution. UDP-Broadcast
#define PACKETTYPE_FINISHED 3 //used by clients to announce they finished execution of a period. UDP-Unicast


/*
 *  this packet specifies the structure of the UDP packets
 *
 */

struct COM_SyncPacket {

	u_int32_t seqNr; //a seqNr to distinguish the packets
	u_int8_t  packetType; //contains packet type information
	//u_int16_t length; //length of data
	u_int8_t data[]; //should contain one of the COM_* Structs â

};



#endif /*SYNCHRONIZATION_H_*/
