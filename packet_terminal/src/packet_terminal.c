#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>

#include "packet_terminal.h"

extern int run;
extern struct queue_item_t * anchor;

// access to anchor and items in linked list
extern pthread_mutex_t queue_mutex;

int main(void) {
	int socket_desc_enb, socket_desc_epc;
	struct sockaddr_in addr_enb, addr_epc;

	//create sockets for both directions
	if ((socket_desc_enb = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("[Error] Could not create socket for direction ENB.\n");
	}
	if ((socket_desc_epc = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("[Error] Could not create socket for direction EPC.\n");
	}

	// configure addresses and ports to bind the sockets
    in_addr_t addr_buffer;
    char * printable_addr_enb=IPV4_ADDR_ENB;
    char * printable_addr_epc=IPV4_ADDR_EPC;

    inet_pton(AF_INET,printable_addr_enb,&addr_buffer);

    addr_enb.sin_addr.s_addr = addr_buffer;		// ENB-address
    addr_enb.sin_port = htons( PORT_ENB );
    addr_enb.sin_family = AF_INET;

    addr_epc.sin_addr.s_addr = addr_buffer;		// ENB-address
    addr_epc.sin_port = htons( PORT_EPC );
    addr_epc.sin_family = AF_INET;

	// bind sockets to Ports in both directions
	if( bind(socket_desc_enb,(struct sockaddr *)&addr_enb , sizeof(addr_enb)) < 0){
		printf("[Error] Could not bind socket to Port for direction ENB.\n");
	}
	if( bind(socket_desc_epc,(struct sockaddr *)&addr_epc , sizeof(addr_epc)) < 0){
		printf("[Error] Could not bind socket to Port for direction EPC.\n");
	}

	// Logging of Bind()-Results
	char s[INET_ADDRSTRLEN];
	inet_ntop(AF_INET,&(addr_epc.sin_addr.s_addr),s,INET_ADDRSTRLEN);
	printf("Socket %d (eNB) bound to Port %d\nSocket %d (EPC) bound to Port %d\nlocal Address: %s\n",
			socket_desc_enb,
			(int)ntohs(addr_enb.sin_port),
			socket_desc_epc,
			(int)ntohs(addr_epc.sin_port),
			s);

	// configure addresses of remote ports of eNB and EPC
	struct sockaddr_in addr_enb_peer, addr_epc_peer;

	addr_enb_peer.sin_addr.s_addr = addr_buffer;		// ENB-address
    addr_enb_peer.sin_port = htons( PORT_REMOTE_ENB );	//  |
    addr_enb_peer.sin_family = AF_INET;					//  |
    													//  V
    inet_pton(AF_INET,printable_addr_epc,&addr_buffer); // change address to EPC
    													//  |
    													//  V
	addr_epc_peer.sin_addr.s_addr = addr_buffer;		// EPC-address
    addr_epc_peer.sin_port = htons( PORT_REMOTE_EPC );	//
    addr_epc_peer.sin_family = AF_INET;					//

    printf("[DEBUG] About to allocate memory for pthreads...\n");

	// create threads for udp_handler_up and udp_handler_down to achieve full duplex communication
    pthread_t *udp_up_thread = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_t *udp_down_thread = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_t *print_thread = (pthread_t *)malloc(sizeof(pthread_t));

    // set up argument structs for up- and down-thread
    struct args_t args_up, args_down;
    args_up.socket1 = socket_desc_enb;
    args_up.socket2 = socket_desc_epc;
    args_up.address = addr_epc_peer;
    args_down.socket1 = socket_desc_enb;
    args_down.socket2 = socket_desc_epc;
    args_down.address = addr_enb_peer;

    printf("[DEBUG] About to create pthreads...\n");

    pthread_create(udp_up_thread,NULL,&udp_handler_up,(void *)&args_up);
    pthread_create(udp_down_thread,NULL,&udp_handler_down,(void *)&args_down);
    pthread_create(print_thread,NULL,&handler_print,NULL);

    // wait for a long long time
    int remain_sec;
    remain_sec=sleep(SLEEP_LEN_SEC);
    printf("I have been running for %d seconds. Time to close the sockets...\n",(SLEEP_LEN_SEC-remain_sec));

    // close sockets for both directions
    run=0;  // end thread tasks
	close(socket_desc_enb);
	close(socket_desc_epc);

	// bye
	return EXIT_SUCCESS;
}
