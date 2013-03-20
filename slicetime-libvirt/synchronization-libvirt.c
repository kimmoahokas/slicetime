/* 
 * SliceTime synchronization for libvirt
 * Aalto University School of Science
 * Department of Computer Science and Engineering.
 * Vu Ba Tien Dung
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <libvirt/libvirt.h>

#include "synchronization.h"
#include "synchronization-libvirt.h"

// data structure for the libvirt connection
virConnectPtr conn;
int numDomains;
virDomainPtr *activeDomains;

// data structure for the slicetime communication
int slicetime_initialized = 0;
int slicetime_socket;
fd_set rset;

// data structure for the RUN_PERMISSION
uint32_t slice;
int first_time = 0;
int vm_running = 1;

// data structure for time tracking
int64_t r_total = 0;
struct timespec tp_origin, tp_start, tp_end;

/////////////////////////////////////////////////////
// Libvirt-related functions
/////////////////////////////////////////////////////

void libvirt_connect() {
    int i;
    int *activeDomainIDs;

    // connect to xend
    conn = virConnectOpen("xen:///");
    if (conn == NULL) {
        printf("Failed to open connection to xen");
        exit(0);
    }

    // get the Domain ID list
    numDomains = virConnectNumOfDomains(conn);
    activeDomainIDs = (int*) malloc(sizeof(int) * numDomains);
    activeDomains = (virDomainPtr*) malloc(sizeof(virDomainPtr) * numDomains);
    numDomains = virConnectListDomains(conn, activeDomainIDs, numDomains);

    // associate the Domain ID list to Domain name list
    printf("Active domain IDs:\n");
    for (i = 0; i < numDomains; i++) {
        activeDomains[i] = virDomainLookupByID(conn, activeDomainIDs[i]);
        printf("  [%s - %d]\n", virDomainGetName(activeDomains[i]), activeDomainIDs[i]);
    }

    // Domain ID list is useless
    free(activeDomainIDs);
}

void libvirt_disconnect() {
    free(activeDomains);
    virConnectClose(conn);

    // directly exit from here
    exit(0);
}

void stop_all_vm() {
    int state, reason, count, i;

    // suspend all the VM-es (except for Domain-0)
    for (i = 1; i < numDomains; i++) 
        virDomainSuspend(activeDomains[i]);

    // capture the time after suspend action
    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    
    // this function will looped until all machines are paused.
    while (1) {
        count = 0;
        for (i = 1; i < numDomains; i++) {
            // check if the state is PAUSED
            virDomainGetState(activeDomains[i], &state, &reason, 0);
            if (state == VIR_DOMAIN_PAUSED)
                count++;
        }
        // break when all the machines are paused
        if (count == numDomains - 1) break;
    }
}

void resume_all_vm() {
    int state, reason, count, i;

    // resume all the VM-es (except for the Domain-0)
    for (i = 1; i < numDomains; i++)
        virDomainResume(activeDomains[i]);

    // capture the time after resume action
    clock_gettime(CLOCK_MONOTONIC, &tp_start);

    // this function will looped until all machines are resumed.
    while (1) {
        count = 0;
        for (i = 1; i < numDomains; i++) {
            // check if the state is RUNNING (or BLOCKED in Xen)
            virDomainGetState(activeDomains[i], &state, &reason, 0);
            if (state == VIR_DOMAIN_BLOCKED || state == VIR_DOMAIN_RUNNING)
                count++;
        }
        if (count == numDomains - 1) break;
    }
}

/////////////////////////////////////////////////////
// Slicetime-related functions
/////////////////////////////////////////////////////

void slicetime_init_client(const char *host, const char *host_port, 
                          const char *client_port, int client_id)
{
    // init the basic values
    slicetime_initialized = 1;
    vm_running = 0;
    first_time = 1;

    // suspend all the VM-es upon start up
    stop_all_vm();

    // init client-server communication
    slicetime_socket = register_client(host, host_port, client_port, client_id,
                                 slicetime_run_for);
    if (slicetime_socket < 0)
    {
        printf("Init failed!\n");
        slicetime_initialized = 0;
        return;
    }    

    // record the based real-time
    clock_gettime(CLOCK_MONOTONIC, &tp_origin);

    // finish
    printf("Init slicetime client\n");
}

void slicetime_sock_read_cb(void) {
    // handle the RUN_PERMISSION message
    handle_socket_read();
}

void slicetime_run_for(uint32_t microseconds)
{
    struct itimerval value, ovalue, pvalue;
    int which = ITIMER_REAL;

    // only run when the slicetime communication works
    if (!slicetime_initialized)
    {
        printf("SliceTime not initialized!\n");
        return;
    }

    // save the slicetime values
    slice = microseconds;
    first_time = 0;

    // ignore if the VM-es are still running
    if (!vm_running)
    {
        // resume all the VM-es
        vm_running = 1; // come before resume_all_vm so that the loop in main() works
        resume_all_vm();

        // set the timer interrupt for "slice" microseconds
        signal(SIGALRM, slicetime_stop_timer_cb);

        getitimer( which, &pvalue );
        value.it_interval.tv_sec = 0;       /* Zero seconds */
        value.it_interval.tv_usec = slice;  /* "slice" microseconds */
        value.it_value.tv_sec = 0;          /* Zero seconds */
        value.it_value.tv_usec = slice;     /* "slice" microseconds */
        setitimer( which, &value, &ovalue);
    }
}

static void slicetime_stop_timer_cb(int sig) {
    struct itimerval value;
    int which = ITIMER_REAL;

    // stop the timer interrupts
    signal(SIGALRM, SIG_IGN);  

    getitimer( which, &value );
    value.it_value.tv_sec = 0;
    value.it_value.tv_usec = 0;
    setitimer( which, &value, NULL );

    // suspend all the VM-es
    stop_all_vm();
    vm_running = 0; // come after stop_all_vm to make sure all VM-es are stopped

    // calculate the running time (in nanoseconds)
    int64_t r_duration = (tp_end .tv_sec - tp_origin.tv_sec) - (tp_start.tv_sec - tp_origin.tv_sec);
    r_duration = r_duration * 1000000000LL + (tp_end.tv_nsec - tp_start.tv_nsec); // duration in nanoseconds
    
    // accumulate the running time
    r_total += r_duration;

    // send the FINISH message 
    //   content = time slice (us) + accumulated running time (ns)
    period_finished(slice, r_total); 
}

void slicetime_stop_sync()
{
    // terminate the slicetime communication
    unregister_client(0);
    slicetime_initialized = 0;

    // resume all the VM-es
    vm_running = 1;
    resume_all_vm();

    // finish
    printf("Stopping sync..\n");
}

static void exitProcedure(int sig) {
    // remove the timer and control-c interrupt
    signal(SIGALRM, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    // disconnect from the slicetime server
    slicetime_stop_sync();

    // disconnect from the libvirtd
    libvirt_disconnect();
} 

/*
 * Main function
 * Input: 
 *  1 - slicetime-server IP
 *  2 - slicetime-server port
 *  3 - slicetime-client port (this client)
 *  4 - slicetime-client ID (this client)
 */
int main(int argc, char *argv[]) {

    // retrieve the list of domains
    libvirt_connect();

    // connect to the slicetime server
    slicetime_init_client(argv[1], argv[2], argv[3], atoi(argv[4]));

    // capture the Control-C signal
    signal(SIGINT, exitProcedure);

    // loop forever until the control-c is caught
    for ( ; ; ) {
        FD_ZERO(&rset);
        FD_SET(slicetime_socket, &rset);
        select(FD_SETSIZE, &rset, NULL, NULL, NULL);

        // process the received RUN_PERMISSION
        if (FD_ISSET(slicetime_socket, &rset)) 
            slicetime_sock_read_cb();

        // the next loop will start after all VM-es are suspended
        while (first_time == 0 && vm_running) { sleep(1); }
    }

    return 0;
}
