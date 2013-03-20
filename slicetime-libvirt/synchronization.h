#ifndef SYNCHRONIZATION_H_
#define SYNCHRONIZATION_H_
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "synchronization-libvirt.h"

/**
 *
 * This file contains important structs that are used for the communication
 * between synchronizer server and qemu client using the barrier-sync algorithm.
 * Adapted from original SliceTime synchronizer to support qemu.
 *
 * The basic communication actions are:
 *
 *   a) a client registers himself to be synchronized (PacketType=0);
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

 typedef struct COM_RegisterClient {
	uint16_t clientID; //the id of the client that wishes to register. must be unique!
	uint8_t clientType; // Type of client: 0 = locally managed Xen Client, 1 = remotely managed  Xen client, 2 = remote simulation client, 254 = other, 255 = unknown
	char client_Description[CLIENT_DESCR_LENGTH]; //arbitrary client description.
} COM_RegisterClient;

/*
 * unregisters a client from the synchronization server
 *
 */

#define UNREGISTER_REASON_REGULAR 0
#define UNREGISTER_REASON_OUT_OF_SYNC 1
#define UNREGISTER_REASON_OTHER 2

typedef struct COM_UnregisterClient {
	uint16_t clientID; //the id of the client to unregister�
	uint8_t reason; //the reason why a client unregisters, shoudl correspond to constant��
} COM_UnregisterClient;

/*
 * tell all clients that they're allowed to execute a certain amount of virtual time
 *
 * IMPLEMENTATION NOTE: - Distribute this to all remote systems with a UDP broadcast!
 *  					- Afterwards, use subsequent hypercalls to notify local guest domains!
 *
 * NOTE: We probably need periodic retransmissions in order to compensate for lost udp-packets
 * otherwise, the execution will hang in a deadlock! (server waits for clients to finish execution, one client waits
 * for server to announce next period)�
 */

/*
 * NOTE 06/04/2009 - We globally switch to MICROSECONDS for the accuracy level!
 *
 * This has been consistently changed in the kernel sync module and the ns-3 implementation
 * as well as in the synchronizer
 * 
 */
typedef struct COM_RunPermission {
	uint32_t periodId; // used to identify the periods
	uint32_t runTime;  //the runtime the clients are allowed to continue their execution in MICROSECONDS!!! 
} COM_RunPermission;


/*
 *  tell the server that a client has finished the execution of a provided period
 *
 *  IMPLEMENTATION NOTE: If transmitted over UDP, this information must be rebroadcasted periodically
 *  in order to avoid deadlocking (if a finished packet gets lost)
 */

typedef struct COM_Finished {
	 uint32_t periodId; //the id of the period whose execution has been finished;
	 uint32_t runTime; //the virtual runtime that was assigned in this period (microseconds)
	 uint32_t realTime; //the time actually needed for executing this period (microseconds)=> useful for statistics!
	 uint16_t clientId;
} COM_Finished;

//definition of packet type ids (should correspond to introductionary comment above!)

#define PACKETTYPE_REGISTER 0 //used to register a client at a server - note: we could use tcp here als well
#define PACKETTYPE_UNREGISTER 1 //used to unregister a client - note: we could use tcp for this as well.
#define PACKETTYPE_RUNPERMISSION 2 //used to announce that the clients can continue with their execution. UDP-Broadcast
#define PACKETTYPE_FINISHED 3 //used by clients to announce they finished execution of a period. UDP-Unicast


/*
 *  this packet specifies the structure of the UDP packets
 *
 */

typedef struct COM_SyncPacket {
	uint32_t seqNr; //a seqNr to distinguish the packets
	uint8_t  packetType; //contains packet type information
	//u_int16_t length; //length of data
	uint8_t data[]; //should contain one of the COM_* Structs �
} COM_SyncPacket;

int register_client(const char *host, const char *host_port,
		    const char *client_port, int client_id, SliceTime_runfor cb);

void period_finished(int virtual_time, int real_time);

int unregister_client(int reason);

void handle_socket_read(void);

/*
 * this prototypes are required by latest qemu compilation
 * 
 */
 void setnonblocking(int);
 int get_connection(const char *, const char *, const char *); 
 void client_sendFinished(COM_Finished);
 void client_sendRegister(COM_RegisterClient);
 void client_sendUnregister(COM_UnregisterClient);

#endif /*SYNCHRONIZATION_H_*/

