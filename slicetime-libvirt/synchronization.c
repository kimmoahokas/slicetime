#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#include "synchronization.h"

#define DEBUG 0

void print_addr(struct addrinfo *);

// global variables for slicetime synchronization
int slicetime_client_sock, slicetime_client_id, slicetime_seqNr,
    slicetime_client_period = -1; //-1 denotes not started

struct sockaddr_in slicetime_dest;
    
SliceTime_runfor slicetime_runfor_cb;

void setnonblocking(int sock)
{
	int opts;

	opts = fcntl(sock,F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		exit(EXIT_FAILURE);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(sock,F_SETFL,opts) < 0) {
		perror("fcntl(F_SETFL)");
		exit(EXIT_FAILURE);
	}
	return;
}

void print_addr(struct addrinfo *info)
{
    printf("IP addresses\n");

    struct addrinfo *p;
    
    for(p = info;p != NULL; p = p->ai_next) {
        void *addr;
       const char *ipver;
	char ipstr[INET6_ADDRSTRLEN];

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("  %s: %s\n", ipver, ipstr);
    }
}

/*
 * Create socket, bind it to local port and connect it to given host:port
 * (yes, UDP sockets can be connected. this allows us to use send() and recv()
 * without destination essentially making other parts little simpler. 
 */
int get_connection(const char *host, const char *remote_port, const char *local_port) {
    //define variables
    struct addrinfo hints, *host_info, *tmp;
    int sockfd, yes, status;
    
    yes = 1;	// for setsockopt

    // get addrinfo for local port
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; //ipv4 or ipv6
    hints.ai_socktype = SOCK_DGRAM; //udp
    hints.ai_flags = AI_PASSIVE; //pick interface automatically
    status = getaddrinfo(NULL, local_port, &hints, &host_info);
    if (status != 0) {
        perror("getaddrinfo error:");
        return -1;
    }
    printf("Getting socket...\n");
    print_addr(host_info);
    // loop through struct addrinfo and bind to first possible socket
    for (tmp = host_info; tmp != NULL; tmp = tmp->ai_next) {
        //create socket
        sockfd = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
        if (sockfd < 0) {
            continue;
        }
        // allow fast reuse of socket
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        //bind socket
        if (bind(sockfd, tmp->ai_addr, tmp->ai_addrlen) != 0) {
            close(sockfd);
            continue;
        }
        //bind succesfull
        break;
    }
    if(tmp == NULL) {
        //something went wrong while trying to get listening socket
        perror("Failed to establish listening socket");
        return -1;
    }

    //we don't need addrinfo anymore
    freeaddrinfo(host_info);
    
    slicetime_dest.sin_family = AF_INET;
    slicetime_dest.sin_addr.s_addr = inet_addr(host);
    slicetime_dest.sin_port = htons(atoi(remote_port));
    
    setnonblocking(sockfd);

    printf("Succesfully connected socket!\n");
    return sockfd;
}

void client_sendFinished(COM_Finished fin) {
    uint8_t buffer[32000];
    //first of all, mutual exclusion for safety reasons :)

    struct COM_SyncPacket
		    *spacket=(struct COM_SyncPacket*) malloc(sizeof(COM_SyncPacket)
				    +sizeof(fin));

    memcpy(&(spacket->data), &fin, sizeof(fin));

    slicetime_seqNr++;
    spacket->packetType = PACKETTYPE_FINISHED;
    spacket->seqNr = htonl(slicetime_seqNr);
    //	spacket->length = sizeof(reg);

    int buffer_length = sizeof(*spacket)+sizeof(fin);
    memcpy(&buffer,spacket,buffer_length);

    int bytes_sent = sendto(slicetime_client_sock, buffer, buffer_length, 0, (struct sockaddr*)&slicetime_dest, sizeof(struct sockaddr_in));
    if(bytes_sent < 0)
	perror("Error sending packet:");

    //free memory
    free(spacket);
    //printf("Done sending seqNr=%d. ", slicetime_seqNr);
}

void client_sendRegister(COM_RegisterClient reg) {
	uint8_t buffer[32000];
	//first of all, mutual exclusion for safety reasons :)

	struct COM_SyncPacket
			*spacket=(struct COM_SyncPacket*) malloc(sizeof(COM_SyncPacket)
					+sizeof(reg));

	memcpy(&(spacket->data),&reg,sizeof(reg));

	slicetime_seqNr++;
	spacket->packetType = PACKETTYPE_REGISTER;
	spacket->seqNr = htonl(slicetime_seqNr);
	//	spacket->length = sizeof(reg);

	int buffer_length = sizeof(*spacket)+sizeof(reg);
	memcpy(&buffer,spacket,buffer_length);

	int bytes_sent = sendto(slicetime_client_sock, buffer, buffer_length, 0, (struct sockaddr*)&slicetime_dest, sizeof(struct sockaddr_in));
	if(bytes_sent < 0)
	    perror("Error sending packet:");

	//free memory
	free(spacket);
	//printf("Done sending the registration.\n");
}

void client_sendUnregister(COM_UnregisterClient ureg) {
	uint8_t buffer[32000];
	//first of all, mutual exclusion for safety reasons :)

	struct COM_SyncPacket
	    *spacket = (struct COM_SyncPacket*) malloc(sizeof(COM_SyncPacket)
					+sizeof(ureg));

	memcpy(&(spacket->data), &ureg, sizeof(ureg));

	slicetime_seqNr++;
	spacket->packetType = PACKETTYPE_UNREGISTER;
	spacket->seqNr = htonl(slicetime_seqNr);

	int buffer_length = sizeof(*spacket)+sizeof(ureg);
	memcpy(&buffer,spacket,buffer_length);

	int bytes_sent = sendto(slicetime_client_sock, buffer, buffer_length, 0, (struct sockaddr*)&slicetime_dest, sizeof(struct sockaddr_in));
	if(bytes_sent < 0)
	    perror("Error sending packet:");

	//free memory
	free(spacket);
	//printf("Done unregister!\n");
}

int register_client(const char *host, const char *host_port,
		    const char *client_port, int client_id, SliceTime_runfor cb)
{
    slicetime_client_period = -1; //-1 denotes not started
    slicetime_client_id = client_id;
    slicetime_runfor_cb = cb;
    slicetime_client_sock = get_connection(host, host_port, client_port);
    if (slicetime_client_sock <= 0)
    {
	perror("Couldn't create socket:");
	return -1;
    }
    COM_RegisterClient reg;
    reg.clientID = htons(slicetime_client_id);
    reg.clientType= CLIENT_TYPE_TEST;
    sprintf(reg.client_Description, "Xen-emulator");

    printf("Registering...\n");
    client_sendRegister(reg);
    printf("Done register!\n");
    return slicetime_client_sock;
}  
  

void period_finished(int virtual_time, int real_time)
{
    if (slicetime_client_period % 10 == 0) {
	printf("Finished period %i\n", slicetime_client_period);
    }
    COM_Finished fin;
    fin.clientId = htons(slicetime_client_id);
    fin.periodId = htonl(slicetime_client_period);
    fin.runTime = htonl(virtual_time);
    fin.realTime = htonl(real_time);

    client_sendFinished(fin);
}

int unregister_client(int reason)
{
    printf("Unregistering client..\n");
    COM_UnregisterClient ureg;
    ureg.clientID = htons(slicetime_client_id);
    ureg.reason = reason;
    client_sendUnregister(ureg);

    printf("Closing socket. \n");
    close(slicetime_client_sock);
    printf("stopping client functionality... \n");
    return 0;
}

void handle_socket_read(void) {
    int status;
    uint8_t buffer[30000];

    struct sockaddr from;
    socklen_t addrlen;
    
    status = recvfrom(slicetime_client_sock, &buffer, sizeof(buffer), 0,
                      &from, &addrlen);
    if (status <= 0)
    {
    	perror("Error while receiving data:");
    	return;
    }

    struct COM_SyncPacket* received_packet=(COM_SyncPacket*) &buffer;

    //int rec_seqNr = ntohl(received_packet->seqNr);
    int rec_packetType = received_packet->packetType;

    if (rec_packetType==PACKETTYPE_RUNPERMISSION) {
	COM_RunPermission *result = (COM_RunPermission*) &(received_packet->data);

	uint32_t runTime = ntohl(result->runTime);
	int periodId = ntohl(result->periodId);
	//monitor_printf(monitor, "RunPermission received: runTime=%i,periodId=%i\n",runTime,periodId);

	//in the beginning, synchronize to periodId of server
	if (slicetime_client_period==-1) {
	    //printf("Intial Synchronization: Setting client_period to server period: %i\n",periodId);
	    slicetime_client_period=periodId;
	    //as this is the initial synchronization step, force received to be true
	}
	else if ((periodId>slicetime_client_period)) {
	    slicetime_client_period = periodId;
	}

    //printf("\nReceived periodId=%d. ", periodId);
	slicetime_runfor_cb(runTime);
    } else {
	printf("Wrong Packet Type. Ignoring\n");
    }
}


