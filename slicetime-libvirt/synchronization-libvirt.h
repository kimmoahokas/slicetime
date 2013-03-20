#ifndef SYNCHRONIZATION_LIBVIRT_H 
#define SYNCHRONIZATION_LIBVIRT_H  

/* 
 * SliceTime synchronization for libvirt
 * Aalto University School of Science
 * Department of Computer Science and Engineering.
 * Vu Ba Tien Dung
 */

/*
 * Connect/disconnect libvirt to the Xen hypervisor
 */
void libvirt_connect();
void libvirt_disconnect();

/*
 * Suspend/resume all the VM-es associated with the Xen hypervisor 
 * 	(except for the Domain-0)
 */
void stop_all_vm();
void resume_all_vm();

/*
 * Initialize SliceTime client. This should be called in emulator initialization
 * code. It connects emulator to SliceTime synchronization server and return the
 * opened socket.
 */
void slicetime_init_client(const char *host, const char *host_port, 
			  const char *client_port, int client_id);

/*
 * After the client-server communication has been initiated, start listenning
 * for the RUN_PERMISSION message
 */
void slicetime_sock_read_cb(void);

/*
 * Allow the emulator to run for given amount of microseconds
 */
typedef void(*SliceTime_runfor)(uint32_t);
void slicetime_run_for(uint32_t microseconds);

/*
 * After the run-time has ended
 */
static void slicetime_stop_timer_cb(int);

/*
 * Terminate the Slicetime client-server communication
 */
void slicetime_stop_sync();

/*
 * This function is executed when Ctrl-C is caught. Terminate the slicetime, libvirt
 */
static void exitProcedure(int);

#endif
