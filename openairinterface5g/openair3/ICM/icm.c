#include <stdio.h>
#include "intertask_interface.h"
#include "icm.h"
#include "udp_eNB_task.h"
#include "socket.h"

#define PORT_ICM 6000
#define PORT_PT 2152
#define IPV4_ADDR_LOCAL "127.0.0.1"
#define IPV4_ADDR_ENB "192.168.178.41"

int run;

void * icm_task(void* notUsed);
void * icm_process_itti_msg(int sd);

void * icm_task(void* notUsed){

	itti_mark_task_ready(TASK_ICM);

	run = 1;

	// socket for communication with packet_terminal
	int sd;
	struct sockaddr_in addr_local;
	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("[Error] Could not create socket for terminal.\n");
	}

    in_addr_t addr_buffer;
    char * printable_addr_local=IPV4_ADDR_ENB;

    inet_pton(AF_INET,printable_addr_local,&addr_buffer);
	addr_local.sin_family = AF_INET;
	addr_local.sin_port = htons(PORT_ICM);
	addr_local.sin_addr.s_addr = addr_buffer;

	if( bind(sd,(struct sockaddr *)&addr_local , sizeof(addr_local)) < 0){
		printf("[Error] Could not bind socket to Port for direction PT\n");
	}


	while(run) {
		(void) icm_process_itti_msg(sd);
	}

	close(sd);

	return NULL;
}

void * icm_process_itti_msg(int sd){
	MessageDef         *received_message_p    = NULL;

	itti_receive_msg(TASK_ICM, &received_message_p);
	if (received_message_p != NULL) {
		switch (ITTI_MSG_ID(received_message_p)) {
/*		case UDP_DATA_REQ: {
    		udp_data_req_t *udp_data_req_p = &received_message_p->ittiMsg.udp_data_req;
    		unsigned char buffer_local[1600];
    		memcpy(buffer_local,udp_data_req_p->buffer,udp_data_req_p->buffer_length);
    		buffer_local[(udp_data_req_p->buffer_length)+1]='\0';
    		int index;
    		int len=(udp_data_req_p->buffer_length);
    		for (index=0;index<len;index++){
    			if (buffer_local[index]<32 || buffer_local[index]>127) {
    				buffer_local[index]='+';
    			}
    		}
    		printf("[ICM][UDP_DATA_REQ] buffer length: %u, buffer:\n\t%s\n",
    				udp_data_req_p->buffer_length,
    				buffer_local);
		} break;

		case UDP_DATA_IND: {
    		udp_data_ind_t *udp_data_ind_p = &received_message_p->ittiMsg.udp_data_ind;
    		unsigned char buffer_local[1600];
    		memcpy(buffer_local,udp_data_ind_p->buffer,udp_data_ind_p->buffer_length);
    		int index;
    		int len=(udp_data_ind_p->buffer_length);
     		printf("[ICM][UDP_DATA_IND] buffer length: %u, buffer:\n\t",
    				udp_data_ind_p->buffer_length);

    		for (index=0;index<len;index++){
    			printf("%x ",buffer_local[index]);
    		}
    		printf("\n");

		} break;

		case SCTP_DATA_REQ: {
    		sctp_data_req_t *sctp_data_req_p = &received_message_p->ittiMsg.sctp_data_req;
    		unsigned char buffer_local[3000];
    		memcpy(buffer_local,sctp_data_req_p->buffer,sctp_data_req_p->buffer_length);
    		buffer_local[(sctp_data_req_p->buffer_length)+1]='\0';
    		int index;
    		int len=(sctp_data_req_p->buffer_length);
    		for (index=0;index<len;index++){
    			if (buffer_local[index]<32 || buffer_local[index]>127) {
    				buffer_local[index]='+';
    			}
    		}
    		printf("[ICM][SCTP_DATA_REQ] buffer length: %u, buffer:\n\t%s\n",
    				sctp_data_req_p->buffer_length,
    				buffer_local);
		} break;

		case SCTP_DATA_IND: {
    		sctp_data_ind_t *sctp_data_ind_p = &received_message_p->ittiMsg.sctp_data_ind;
    		unsigned char buffer_local[3000];
    		memcpy(buffer_local,sctp_data_ind_p->buffer,sctp_data_ind_p->buffer_length);
    		buffer_local[(sctp_data_ind_p->buffer_length)+1]='\0';
    		int index;
    		int len=(sctp_data_ind_p->buffer_length);
    		for (index=0;index<len;index++){
    			if (buffer_local[index]<32 || buffer_local[index]>127) {
    				buffer_local[index]='+';
    			}
    		}
    		printf("[ICM][SCTP_DATA_IND] buffer length: %u, buffer:\n\t%s\n",
    				sctp_data_ind_p->buffer_length,
    				buffer_local);
		} break;

		case S1AP_INITIAL_CONTEXT_SETUP_RESP: {
			printf("[ICM] I sense the time has come to act\n");

			s1ap_initial_context_setup_resp_t *s1ap_initial_context_setup_resp_p = &received_message_p->ittiMsg.s1ap_initial_context_setup_resp;

			printf("[ICM] information I have:\n"
					"\teNB_ue_s1ap_id: %d\n"
					"\tnb_of_e_rabs: %d\n"
					"\te_rabs[0].e_rab_id: %u\n"
					"\te_rabs[0].gtp_teid: %u\n"
					"\te_rabs[0].eNB_addr: ",
					s1ap_initial_context_setup_resp_p->eNB_ue_s1ap_id,
					s1ap_initial_context_setup_resp_p->nb_of_e_rabs,
					s1ap_initial_context_setup_resp_p->e_rabs[0].e_rab_id,
					s1ap_initial_context_setup_resp_p->e_rabs[0].gtp_teid);

			int i;
			for (i = 0; i < s1ap_initial_context_setup_resp_p->e_rabs[0].eNB_addr.length;  i++){
				printf("%d", s1ap_initial_context_setup_resp_p->e_rabs[0].eNB_addr.buffer[i]);
			}
			printf("\n");


			// send itti-message S1AP_E_RAB_SETUP_REQ to TASK_RRC_ENB using data of this UE
			S1AP_MME_UE_S1AP_ID_t         mme_ue_s1ap_id;
			S1AP_ENB_UE_S1AP_ID_t         enb_ue_s1ap_id;
			s1ap_eNB_mme_data_t          *mme_desc_p       = NULL;
			s1ap_eNB_ue_context_t        *ue_desc_p        = NULL;
			MessageDef                   *message_p        = NULL;
			S1AP_E_RABSetupRequest_t     *container;
			S1AP_E_RABSetupRequestIEs_t  *ie;
			message_p = itti_alloc_new_message(TASK_S1AP, S1AP_E_RAB_SETUP_REQ);

		}
*/
		case S1AP_E_RAB_SETUP_RESP:{
			// extract gtp teid from message and send add_drb_gtp-message over udp to packet_terminal

			s1ap_e_rab_setup_resp_t *cont = &received_message_p->ittiMsg.s1ap_e_rab_setup_resp;
			e_rab_setup_t * rab = &cont->e_rabs[0];
			uint32_t teid = rab->gtp_teid;

			char buffer[4];
			memcpy(buffer,&teid,sizeof(teid));

			// address to send to (packet_terminal)
			struct sockaddr_in addr_enb_peer;
			inet_aton(IPV4_ADDR_ENB,&addr_enb_peer.sin_addr);
			addr_enb_peer.sin_family = AF_INET;
			addr_enb_peer.sin_port = htons(PORT_PT);

			socklen_t address_len = sizeof(struct sockaddr_storage);
			int ret;

			if ((ret = sendto(sd,buffer,sizeof(buffer),0,(struct sockaddr*)&addr_enb_peer,address_len))<0){
				printf("[ERROR-ICM] failed sendig teid %u to packet_terminal on sd %d\n",teid,sd);
			} else {
				printf("[DEBUG-ICM] Sent teid %u to packet_terminal on sd %d\n",teid,sd);
			}

		}



		default:{
/*
			printf("[ICM] Message: %s from %s\n",
					itti_get_message_name(received_message_p->ittiMsgHeader.messageId),
					itti_get_task_name(received_message_p->ittiMsgHeader.originTaskId));
*/
		} break;

	    case TERMINATE_MESSAGE: {

	    	// we send zero-teid to packet_terminal to end it
			uint32_t teid = 0;

			char buffer[4];
			memcpy(buffer,&teid,sizeof(teid));

			// address to send to (packet_terminal)
			struct sockaddr_in addr_enb_peer;
			inet_aton(IPV4_ADDR_ENB,&addr_enb_peer.sin_addr);
			addr_enb_peer.sin_family = AF_INET;
			addr_enb_peer.sin_port = htons(PORT_PT);

			socklen_t address_len = sizeof(struct sockaddr_storage);
			int ret;
			if ((ret = sendto(sd,buffer,sizeof(buffer),0,(struct sockaddr *)&addr_enb_peer,address_len))<0){
				printf("[ERROR-ICM] failed sendig ZERO-teid to packet_terminal on sd %d\n",sd);
			}

	        run = 0;
	    }
	    break;

		}// switch received message
		}// if msg NOT NULL

	return NULL;
}
