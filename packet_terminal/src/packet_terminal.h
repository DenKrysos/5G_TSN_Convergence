#ifndef PACKET_TERMINAL_H_
#define PACKET_TERMINAL_H_

#include<netinet/udp.h>

#define BUFFSIZE 2048

#define IPV4_ADDR_ENB "192.168.178.41"
#define IPV4_ADDR_LOOPBACK "127.0.0.1"
#define PORT_REMOTE_ENB 2151
#define PORT_ENB 2150
#define IPV4_ADDR_EPC "192.168.178.42"
#define PORT_REMOTE_EPC 2152
#define PORT_EPC 2152
#define PORT_ICM 6000
#define MAX_DRB 3

#define PORT1 6001
#define PORT2 6002
#define PORT3 6003

#define SLEEP_LEN_SEC 3600

int run=1;

typedef enum direction_type_t {
	UPSTREAM,
	DOWNSTREAM
} direction_type;

struct queue_item_t * anchor = NULL;

// access to anchor and items in linked list
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

struct args_t {
	int socket1;
	int socket2;
	struct sockaddr_in address;
};

struct queue_item_t{
	char data[BUFFSIZE];
	size_t numb_data;
	direction_type direction;
	struct queue_item_t * next;
};

void* udp_handler_down(void * args);
void* udp_handler_up(void * args);
int add_queue_item(char * data_, direction_type direc_, int numb_, struct queue_item_t ** ptr_to_anchor);
void * handler_print(void * in);
int print_last_item();

void* udp_handler_up(void * args){

	printf("[DEBUG] Started udp_handler_up\n");

	char buffer[BUFFSIZE];
	int numb_read;
	socklen_t address_len = sizeof(struct sockaddr_storage);

	// retrieve information from argument struct
	struct args_t *arguments = (struct args_t *)args;
	int sock_enb = arguments->socket1;
	int sock_epc = arguments->socket2;
	struct sockaddr_in epc_peer_address = arguments->address;

	while(run){
		// wait for incoming packet from eNB
		numb_read = recvfrom(sock_enb,buffer,BUFFSIZE,0,NULL,NULL);

		// do some processing
		add_queue_item(buffer,UPSTREAM,numb_read,&anchor);

		// send packet to EPC
		sendto(sock_epc,buffer,numb_read,0,(struct sockaddr*)&epc_peer_address,address_len);

	}// LOOP
	return NULL;
}

void* udp_handler_down(void* args){

	printf("[DEBUG] Started udp_handler_down\n");

	char buffer[BUFFSIZE];
	int numb_read;
	socklen_t address_len = sizeof(struct sockaddr_storage);
	uint32_t teid_array[MAX_DRB];
	int index_drb = 0;

	// retrieve information from argument struct
	struct args_t *arguments = (struct args_t *)args;
	int sock_enb = arguments->socket1;
	int sock_epc = arguments->socket2;
	struct sockaddr_in enb_peer_address = arguments->address;

	socklen_t len = INET_ADDRSTRLEN;
	struct sockaddr_in addr;
	char *printable_addr = IPV4_ADDR_ENB;
	uint32_t addr_local_int;
	inet_pton(AF_INET,printable_addr,&addr_local_int);
	uint32_t teid = 0;
	int counter[MAX_DRB]={0,0,0};

	while(run){
		// wait for incoming packet from EPC
		numb_read = recvfrom(sock_epc,buffer,BUFFSIZE,0,(struct sockaddr *)&addr,&len);

		// do some processing
		//printf("[CONTROL] received %d bytes\n\taddr_local_int: %u\n\ts_addr: %u\n\tsin_port (host short): %u\n",
		//		numb_read,
		//		addr_local_int,
		//		addr.sin_addr.s_addr,
		//		(unsigned int)ntohs(addr.sin_port));

		if (addr_local_int==addr.sin_addr.s_addr && addr.sin_port==htons(PORT_ICM)){

			memcpy(&teid,buffer,sizeof(uint32_t));
			printf("[CONTROL] Received gtp teid %u\n",teid);

			if (teid==0){
				printf("[CONTROL] now terminating...\n");
				run = 0;
			} else {
				// non-zero-teid
					teid_array[index_drb] = htonl(teid);
					printf("[CONTROL] Added teid %u. index_drb = %d\n",ntohl(teid_array[index_drb]),index_drb);
					index_drb = (index_drb+1) % MAX_DRB;
					if (!index_drb){
						memset(counter,0,sizeof(counter));
					}
			}

		} else {
			// usual S1-U Downstream
			add_queue_item(buffer,DOWNSTREAM,numb_read,&anchor);

			// TODO: check IP header, is this a UDP packet?
			struct udphdr *udph = (struct udphdr*)(buffer+8+20); // size of gtp header and ip header

			//printf("new packet from port %d\n",ntohs(udph->source));


			// exchange TEID of Downlink packets to use dLCP
			if (!index_drb){
				switch(ntohs(udph->source)){
				case PORT1:{
					memcpy(buffer+4,&teid_array[0],4);
					counter[0]++;
					printf("*0*, nr. %d\n",counter[0]);
				} break;
				case PORT2:{
					memcpy(buffer+4,&teid_array[1],4);
					counter[1]++;
					printf("*1*, nr. %d\n",counter[1]);
				} break;
				case PORT3:{
					memcpy(buffer+4,&teid_array[2],4);
					counter[2]++;
					printf("*2*, nr. %d\n",counter[2]);
				} break;
				default: break;
				}
			}

			// send packet to eNB
			sendto(sock_enb,buffer,numb_read,0,(struct sockaddr*)&enb_peer_address,address_len);


		}

	}// LOOP
	return NULL;
}

// add item at beginning of the queue
int add_queue_item(char * data_, direction_type direc_, int numb_, struct queue_item_t ** ptr_to_anchor){

	struct queue_item_t * new = malloc(sizeof(struct queue_item_t));

	pthread_mutex_lock(&queue_mutex);

	strcpy((char *)&(new->data),data_);
	new->direction=direc_;
	new->numb_data=(size_t)numb_;
	new->next=*ptr_to_anchor;
	*ptr_to_anchor=new;

	pthread_mutex_unlock(&queue_mutex);

	return 0;
}

// print last item of queue and delete it
int print_last_item(){
	struct queue_item_t * current = anchor;

	if (current->next==NULL){ // only one item in queue
		if (current->direction==DOWNSTREAM){
			//printf("\t\t[DOWN] Incoming packet of length %d\n",(int)current->numb_data);
		} else {
			//printf("[UP] Incoming packet of length %d\n",(int)current->numb_data);
		}

		anchor=NULL;
		free(current);

		return 0;
	}

	while ((current->next->next)!=NULL){
		current=current->next;
	}		// current points to the second last item

	if (current->next->direction==DOWNSTREAM){
		//printf("\t\t[DOWN] Incoming packet of length %d\n",(int)current->next->numb_data);
	} else {
		//printf("[UP] Incoming packet of length %d\n",(int)current->next->numb_data);
	}

	free(current->next);		// remove last item of queue
	current->next=NULL;
	return 0;
}

void * handler_print(void * in){
	printf("[DEBUG] Started handler_print\n");
	while(run){
		pthread_mutex_lock(&queue_mutex);
		if (anchor!=NULL){
			print_last_item();
		}
		pthread_mutex_unlock(&queue_mutex);
	}
	return NULL;
}

#endif /* PACKET_TERMINAL_H_ */
