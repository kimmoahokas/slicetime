#include "configuration.h"
#include "synchronization.h"
#include "synchronizer.h"
#include <iostream>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEBUG 0
Configuration cf(CONFIGFILE_NAME);
std::string mode;
int sock;

bool srv_clientRegistered[MAX_CLIENTS]; //stores if a client with a certain id is registered
int srv_clientType[MAX_CLIENTS]; //stores the clients' types
int srv_recClientCounter; //a counter for the number of currently registered clients
int srv_period[MAX_CLIENTS]; //stores the period of the clients. needed to determine if all clients have finishted
int srv_currentPeriod = 1;
int srv_maxPeriod = 0;

int seqNr = 0;
std::string srv_brdcast_address;

std::string client_sync_server;
int server_port;
int client_port;
int srv_min_clients;


u_int32_t srv_barrierunTime;

struct COM_RunPermission srv_runpermission;

std::string srv_ClientDescription[MAX_CLIENTS];
int client_period = -1; //-1 denots not started
/*------------------------------------------------------------------------------------------
 * main server behaviour
 *------------------------------------------------------------------------------------------
 *
 */

/**
 * This function handles registration packets delivered to a server
 *
 * If a client registers, first a couple of checks are performed in order to prevent
 * double registration or registration at a synchronizer that is in fact running in client mode
 *
 * Afterwards, the data is simply stored in adequate data structures.
 */
void srv_sendRunPermission() {
	char buf[10];
	if (DEBUG) {
	    printf("Sending RunPermission: periodId: %i\n",
		   ntohl(srv_runpermission.periodId));
	}
	else if (seqNr % 1000 == 0) {
	    printf ("Sendin permission for sequence %i\n", seqNr);
	}
	//first of all, mutual exclusion for safety reasons :)

	//fgets(buf, 10, stdin);
	int psize = sizeof(COM_SyncPacket) + sizeof(srv_runpermission);

	struct COM_SyncPacket *packet=(struct COM_SyncPacket*) malloc(psize);
	seqNr++;
	packet->seqNr=htonl(seqNr);
	packet->packetType=PACKETTYPE_RUNPERMISSION;
	//packet->length=htons(sizeof(COM_RunPermission));
	memcpy(&(packet->data),&srv_runpermission,sizeof(COM_RunPermission));

	struct sockaddr_in destination;
    destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = inet_addr(srv_brdcast_address.c_str());
	
    // send run permission to multiple ports because broadcast is not supported
    // on localhost. ports are in range from "client_port" to "client_port +
    // number of clients - 1"
	for (int i=0;i<srv_recClientCounter;i++) {
	    if (DEBUG)
		printf("Sending packet to port: %i\n", client_port + i);
	    destination.sin_port = htons(client_port + i);
	    int bytes_sent = sendto(sock, packet, psize, 0,(struct sockaddr*) &destination, sizeof(struct sockaddr_in) );
	    if(bytes_sent < 0) printf("Error sending packet: %s\n", strerror(errno) );
	}
	
	//free packet memory
	free(packet);

}

void handle_pkt_register(COM_RegisterClient* reg) {

	//safety check: return at once if we're not a server!
	printf("Register received: id: %i, type: %i, description: %s\n",
	ntohs(reg->clientID), reg->clientType, reg->client_Description);

	if (mode!="server")
		return;
	int clientId=ntohs(reg->clientID);

	if (srv_clientRegistered[clientId]==true) {
		printf("This Client ID has already been registered. Ignoring\n");
	} else {

		if (clientId>MAX_CLIENTS-1) {
			printf("clientID %i exceeds MAX_ID. Ignoring. \n", clientId);
			return;
		}

		srv_clientRegistered[clientId]=true;
		srv_clientType[clientId]=ntohs(reg->clientType);
		srv_ClientDescription[clientId]=std::string(reg->client_Description);
		//new Clients start at current Period, of course!
		srv_period[clientId]=srv_currentPeriod;
		srv_recClientCounter++;
		printf("Client registered.\nTotal Number of registered clients: %i\n",
				srv_recClientCounter);


		//invoke sending of a run permission in order to have new clients get started...
    // but only after the minimum number of clients is registered
		if (srv_recClientCounter >= srv_min_clients)
      srv_sendRunPermission();

	}

}

/*
 * if a client registers, the flag indicating his registration is
 *
 */

void handle_pkt_unregisterClient(COM_UnregisterClient* ureg) {

	/*
	 * safety checks
	 *
	 */
	if (mode!="server")
		return;

	int clientId=ntohs(ureg->clientID);

	if (clientId>MAX_CLIENTS-1)
		return;

	if (srv_clientRegistered[clientId]==false)
		return;

	/*
	 * main unregister
	 */

	srv_clientRegistered[clientId]=false;
	srv_clientType[clientId]=99;
	srv_ClientDescription[clientId]="";
	srv_recClientCounter--;
	printf("Client unregistered: id=%i\n", srv_recClientCounter);
	printf("Reason: %i\n", ureg->reason);
	printf("Total Number of registered clients: %i\n", srv_recClientCounter);

}

/* this sends the current runpermission
 *   invoked either if all clients finished their curred barrier
 *   or through a pthread for periodic retransmission
 */

/*
 * If a finish paket is received, first check if the client has already notified the
 * server of this
 */

void handle_pkt_finished(COM_Finished* fin) {

	int clientId=ntohs(fin->clientId);
	int clientPeriodId=ntohl(fin->periodId);
	int runtime=ntohl(fin->runTime);
	int realtime=ntohl(fin->realTime);
	if (DEBUG)
	    printf("Received Finished MSG: client=%i,periodId=%u,runTime=%u,realTime=%u\n",
		   clientId, clientPeriodId, runtime, realtime);

	/*
	 * check for valid data, otherwise return.
	 * also remove duplicates
	 */
	if (clientPeriodId>srv_currentPeriod) {
		printf("Invalid period (client period higher than server period!) Returning.\n");
		return;
	}

	if (clientId>MAX_CLIENTS-1) {
		printf("Invalid clientID. Returning.\n");
		return;
	}

	if (srv_clientRegistered[clientId]==false) {
		printf("This clientID is not registered! Returning.\n");
		return;
	}

	if (clientPeriodId<srv_period[clientId]) {
		printf("Late / finished period (higher period is has been previously reported) Returning.\n");
		return;
	}

	/*
	 * Main behaviour:
	 * 2) store the actual periodId in buffer
	 * 3) check if all registered clients received the current barrier
	 * 4) if so, send next announcement.
	 */

	//step 1
	srv_period[clientId]=clientPeriodId;
	//step 2
	bool finished=true;
	for (int i=0; i<MAX_CLIENTS; i++) {
		if (srv_clientRegistered[i]==true) {
			finished=finished&&(srv_period[i]==srv_currentPeriod);
			if (finished==false)
				break;
		}
	}

	if (finished==false) {
	    if (DEBUG)
		printf("Not all clients have reported to be finished yet");
	    return;

	} else {
	    if (DEBUG)
		printf("All clients have finished the current period\n");

	    //increase current period;
	    srv_currentPeriod++;
	    if (DEBUG)
		printf("Next PeriodId: %i, Runtime=%i\n", srv_currentPeriod,
			    srv_barrierunTime);

	    //increase runpermission struct for transmission

	    srv_runpermission.periodId=htonl(srv_currentPeriod);
	    srv_runpermission.runTime=htonl(srv_barrierunTime);
	    srv_sendRunPermission();

	}

	return;

}

int mainServer() {
	/*
	 *  initialize data structures
	 */

	srv_recClientCounter=0;
	for (int k=0; k<MAX_CLIENTS; k++) {

		srv_clientRegistered[k]=0;
		srv_clientType[k]=CLIENT_TYPE_UNKNOWN;
		srv_period[k]=0;
		srv_ClientDescription[k]="";
	}
	srv_barrierunTime = cf.getAsInt("SERVER", "srv_barrier_interval");
	srv_brdcast_address = cf.getAsString("SERVER", "srv_brdcast_address");
	
	srv_maxPeriod = cf.getAsInt("SERVER", "max_period");
	
	srv_runpermission.periodId=htonl(srv_currentPeriod);
	srv_runpermission.runTime=htonl(srv_barrierunTime);

	//std::string srv_bind_ip = cf.getAsString("SERVER","srv_bind_ip");
	//printf("Binding Server to this ip: %s\n",srv_bind_ip.c_str());

	//bind stuff
	struct sockaddr_in sa;
	sock = socket(PF_INET,SOCK_DGRAM,IPPROTO_IP);

	printf("Binding socket to port: %i\n", server_port);

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(server_port);

	//enable broadcasting on socket
	int val_on=1;
	int sock_brdcast=setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val_on, sizeof(val_on));
	if (sock_brdcast==0) {
		printf("Successfully enabled Broadcasting over socket\n");
	} else {
		printf("Datagramm Broadcasting could not be enabled! Error code: %i\n",sock_brdcast);
	}

	int bound = bind(sock, (struct sockaddr *)&sa, sizeof(struct sockaddr));
	if (bound < 0)
		fprintf(stderr, "binding failed: bind(): %s\n",strerror(errno));

	printf("Server port: %i, client port: %i, runtime: %i microseconds (%.3f seconds)\n",
	       server_port, client_port, srv_barrierunTime, srv_barrierunTime/1000000.0); 
	uint8_t buffer[32000];

	//initialize pthread for periodic retransmission!

	while (true) {
		int bytes_received = recv (sock, buffer, 32000, 0);
		if (DEBUG)
		    printf ("Data received, length: %i\n", bytes_received);
		COM_SyncPacket packet;
		memcpy(&packet, &buffer, bytes_received);
		if (DEBUG) {
		    printf("Packet type: %i\n", packet.packetType);
		    printf("Packet SeqNr: %i\n", ntohl(packet.seqNr));
		}
		//Packet handling
		//Determine Packet Type
		//Call adequate packet handler by creating pointer to packet payload
		//Note: We simply deal with packets sent to a server.
		//Everything else is simply disregarded

		if (packet.packetType==PACKETTYPE_REGISTER) {
			handle_pkt_register((COM_RegisterClient*) &packet.data);

		} else if (packet.packetType==PACKETTYPE_UNREGISTER) {
			handle_pkt_unregisterClient((COM_UnregisterClient*) &packet.data);

		} else if (packet.packetType==PACKETTYPE_FINISHED) {
			handle_pkt_finished((COM_Finished*) &packet.data);

		}
		else{
			printf("\n\n Unknown packet received \n\n");
		}
		if ((srv_maxPeriod > 0) && (srv_currentPeriod >= srv_maxPeriod)) break;

	}

	printf("Server stopped after period %i\n", srv_currentPeriod);
			
	return 0;
}

COM_RunPermission waitForRunPermission(int socket) {

	bool received = false;
	COM_RunPermission result;
	u_int8_t buffer[30000];

	struct sockaddr from;
	socklen_t addrlen;

	do {

		int bytes_received = recvfrom(socket, &buffer, sizeof(buffer),0,&from,&addrlen);

		struct COM_SyncPacket* received_packet=(COM_SyncPacket*) &buffer;

		int rec_seqNr = ntohl(received_packet->seqNr);
		int rec_packetType = received_packet->packetType;

		printf("Packet Received: type=%i,seqNr=%i\n",rec_packetType,rec_seqNr);

		if (rec_packetType==PACKETTYPE_RUNPERMISSION) {

			memcpy(&result,&(received_packet->data),sizeof(COM_RunPermission));

			int runTime = ntohl(result.runTime);
			int periodId = ntohl(result.periodId);

			//in the beginning, synchronize to periodId of server

			if (client_period==-1) {
				printf("Intial Synchronization: Setting client_period to server period: %i\n",periodId);
				client_period=periodId;
				//as this is the initial synchronization step, force received to be true
				received=true;
				return result;

			}


			printf("RunPermission received: runTime=%i,periodId=%i",runTime,periodId);

			if ((periodId>client_period)) {
				client_period = periodId;
				received = true;
				return result;

			}

			return result;

		} else {

			printf("Wrong Packet Type. Ignoring\n");

		}

	}while (received==false);

	return result;

}

void client_sendFinished(int sock, COM_Finished fin) {

	uint8_t buffer[32000];
	//first of all, mutual exclusion for safety reasons :)

	struct COM_SyncPacket
			*spacket=(struct COM_SyncPacket*) malloc(sizeof(COM_SyncPacket)
					+sizeof(fin));

	memcpy(&(spacket->data), &fin, sizeof(fin));

	seqNr++;
	spacket->packetType = PACKETTYPE_FINISHED;
	spacket->seqNr = htonl(seqNr);
	//	spacket->length = sizeof(reg);

	int buffer_length = sizeof(*spacket)+sizeof(fin);
	memcpy(&buffer,spacket,buffer_length);

	struct sockaddr_in destination;

	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = inet_addr(client_sync_server.c_str());
	destination.sin_port = htons(server_port);

	//printf("Sending packet\n");

	int bytes_sent = sendto(sock, buffer, buffer_length, 0,(struct sockaddr*) &destination, sizeof(struct sockaddr_in) );
	if(bytes_sent < 0)
	printf("Error sending packet: %s\n", strerror(errno) );

	//free memory
	free(spacket);
	printf("Done.");
}

void client_sendRegister(int sock, COM_RegisterClient reg) {

	uint8_t buffer[32000];
	//first of all, mutual exclusion for safety reasons :)

	struct COM_SyncPacket
			*spacket=(struct COM_SyncPacket*) malloc(sizeof(COM_SyncPacket)
					+sizeof(reg));

	/*
	 * test 1: register
	 */

	memcpy(&(spacket->data),&reg,sizeof(reg));

	seqNr++;
	spacket->packetType = PACKETTYPE_REGISTER;
	spacket->seqNr = htonl(seqNr);
	//	spacket->length = sizeof(reg);

	int buffer_length = sizeof(*spacket)+sizeof(reg);
	memcpy(&buffer,spacket,buffer_length);

	struct sockaddr_in destination;

	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = inet_addr(client_sync_server.c_str());
	destination.sin_port = htons(server_port);

	//printf("Sending packet\n");

	int bytes_sent = sendto(sock, buffer, buffer_length, 0,(struct sockaddr*) &destination, sizeof(struct sockaddr_in) );
	if(bytes_sent < 0)
	printf("Error sending packet: %s\n", strerror(errno) );

	//free memory
	free(spacket);
	printf("Done.");

}

void client_sendUnregister(int sock, COM_UnregisterClient ureg) {
	uint8_t buffer[32000];
	//first of all, mutual exclusion for safety reasons :)

	struct COM_SyncPacket
			*spacket=(struct COM_SyncPacket*) malloc(sizeof(COM_SyncPacket)
					+sizeof(ureg));

	/*
	 * test 1: register
	 */

	memcpy(&(spacket->data),&ureg,sizeof(ureg));

	seqNr++;
	spacket->packetType = PACKETTYPE_UNREGISTER;
	spacket->seqNr = htonl(seqNr);
	//	spacket->length = sizeof(reg);

	int buffer_length = sizeof(*spacket)+sizeof(ureg);
	memcpy(&buffer,spacket,buffer_length);

	struct sockaddr_in destination;

	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = inet_addr(client_sync_server.c_str());
	destination.sin_port = htons(server_port);

	//printf("Sending packet\n");

	int bytes_sent = sendto(sock, buffer, buffer_length, 0,(struct sockaddr*) &destination, sizeof(struct sockaddr_in) );
	if(bytes_sent < 0)
	printf("Error sending packet: %s\n", strerror(errno) );

	//free memory
	free(spacket);
	printf("Done.\n");
}

//client
int mainClient() {
	int sock;

	int client_id = cf.getAsInt("CLIENT","client_id");


	client_sync_server = cf.getAsString("CLIENT", "client_sync_server");
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (-1 == sock)//if socket failed to initialize, exit
	{
		printf("Error Creating Socket");
		return 0;
	}


	printf("Binding socket to port: %i\n", client_port);

	struct sockaddr_in sa;

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;

	sa.sin_port = htons(client_port);
	int bound = bind(sock, (struct sockaddr *)&sa, sizeof(struct sockaddr));
	if (bound < 0)
		fprintf(stderr, "binding failed: bind(): %s\n",strerror(errno));

	COM_RegisterClient reg;
	reg.clientID = htons(client_id);
	reg.clientType= CLIENT_TYPE_TEST;
	sprintf(reg.client_Description, "Hello World!");

	printf("Registering...\n");
	client_sendRegister(sock, reg);
	printf("Done!\n");
	//Receive 100 Periods
	for (int i=0; i<5000; i++) {

		//WAIT FOR PERMISSION
		printf("Waiting for Runtime Permission...\n");

		COM_RunPermission r = waitForRunPermission(sock);

		printf("Sleeping...\n");

		//SLEEP
		struct timespec ts;
		ts.tv_sec = 0; /*was tv_sec und tv_nsec (Sek. und Nanosek.) enthält*/
		ts.tv_nsec = 1000*ntohl(r.runTime);
		nanosleep(&ts, NULL);

		printf("Sending FINISH..\n");

		//SEND FINISHED
		COM_Finished fin;
		fin.clientId = htons(client_id);
		fin.periodId = htonl(client_period);
		fin.runTime = r.runTime;
		fin.realTime = 1234;

		client_sendFinished(sock, fin);

	}

	/*
	 * Unregistering Client
	 *
	 */

	COM_UnregisterClient ureg;
	ureg.clientID = htons(client_id);
	ureg.reason = UNREGISTER_REASON_REGULAR;
	client_sendUnregister(sock,ureg);


	printf("Closing socket. \n");
	close(sock);
	printf("stopping client functionality... \n");
	return 0;

}

int main(int argc, char **argv) {

	//read configuration

	printf("Reading Configuration file: %s\n", CONFIGFILE_NAME);

	mode = cf.getAsString("GENERAL", "mode");
	server_port = cf.getAsInt("SERVER", "server_port");
	client_port = cf.getAsInt("CLIENT", "client_port");
	srv_min_clients = cf.getAsInt("SERVER", "min_clients");

	//check commandLine
	if (argc>1) {
		if (std::string(argv[1])=="server") {
			printf("Overriding config setting: Running in server mode\n");
			mode="server";
		} else if (std::string(argv[1])=="client") {
			printf("Overriding config setting: Running in client mode\n");
			mode="client";
		}
	}
	int exit_code=255;
	if (mode=="server") {
		printf("System running in server mode\n");
		exit_code=mainServer();

	} else if (mode=="client") {

		printf("System running in client mode\n");
		exit_code=mainClient();

	} else {
		printf("Pleace specify a prope	r mode. Exiting!\n");
		exit_code = 1;
	}

	//pthread_t helloThread;
	//int r =  pthread_create(&helloThread, NULL, PrintHello,NULL);
	//while (true);
	//printf("Going home...\n");

	return (exit_code);

}
