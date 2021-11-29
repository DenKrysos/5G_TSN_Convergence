#include <stdio.h>
#include <unistd.h>

#include "intertask_interface.h"
#include "drb.h"
#include "esm_proc.h"
#include "bstrlib.h"
#include "/usr/include/linux/sctp.h"
#include "EsmMessageContainer.h"
#include "ActivateDedicatedEpsBearerContextRequest.h"
#include "nas_message.h"

#define ENC  // for encoded NAS messages


typedef struct sctp_association_s {
  struct sctp_association_s              *next_assoc;   ///< Next association in the list
  struct sctp_association_s              *previous_assoc;       ///< Previous association in the list
  int                                     sd;   ///< Socket descriptor
  uint32_t                                ppid; ///< Payload protocol Identifier
  uint16_t                                instreams;    ///< Number of input streams negociated for this connection
  uint16_t                                outstreams;   ///< Number of output strams negotiated for this connection
  sctp_assoc_id_t                         assoc_id;     ///< SCTP association id for the connection
  uint32_t                                messages_recv;        ///< Number of messages received on this connection
  uint32_t                                messages_sent;        ///< Number of messages sent on this connection

  struct sockaddr                        *peer_addresses;       ///< A list of peer addresses
  int                                     nb_peer_addresses;
} sctp_association_t;

typedef struct args_s{
	ue_context_data_t *ctx;
	int *sd;
	struct sockaddr_in *addr;
	int exp;
	int jump; 		// number of TTI's to wait before sending a packet
	pthread_barrier_t *barrier;
} args_t;

int exit_drb;

void sockets_setup(void* args){
	args_t *arguments = (args_t*)args;
	ue_context_data_t *ctx = arguments->ctx;

	int counter = ctx->counter_UE;

	// open 3 sockets
	struct sockaddr_in *addr_p=malloc(sizeof(struct sockaddr_in));

	int *sd0 = (int*)malloc(sizeof(int));
	int *sd1 = (int*)malloc(sizeof(int));
	int *sd2 = (int*)malloc(sizeof(int));
	int *sd3 = (int*)malloc(sizeof(int));

	if ((*sd0 = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		printf("[ERROR-DRB] Socket 0 creation failed.\n");
		return;
	  }

	if ((*sd1 = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		printf("[ERROR-DRB] Socket 1 creation failed.\n");
		return;
	  }

	if ((*sd2 = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		printf("[ERROR-DRB] Socket 2 creation failed.\n");
		return;
	  }

	if ((*sd3 = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		printf("[ERROR-DRB] Socket 3 creation failed.\n");
		return;
	  }

	int index;
	for (index=0;index<15;index++){
		sleep(1);
		if (counter!=ctx->counter_UE){
			printf("\n[DEBUG-DRB] Counter changed, returning\n\n");
			close(*sd0);
			close(*sd1);
			close(*sd2);
			close(*sd3);
			return;
		}
	}

	memset(addr_p, 0, sizeof (struct sockaddr_in));
	addr_p->sin_family = AF_INET;
	addr_p->sin_port = htons(PORT0);
	addr_p->sin_addr.s_addr = inet_addr(ADDR_EPC);

	if (bind(*sd0, (struct sockaddr *)addr_p, sizeof (struct sockaddr_in)) < 0) {
		printf("\n[ERROR-DRB] Failed to bind socket 0\n\n");
		close(*sd0);
		close(*sd1);
		close(*sd2);
		close(*sd3);
		return;
	}

	// wait for trigger from UE
	struct sockaddr_in *peer = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	memset(peer,0,sizeof(struct sockaddr_in));
	int *recbuf = malloc(sizeof(int));
	int exp_l = 0;
	socklen_t fromlen = sizeof(*peer);

	addr_p->sin_port = htons(PORT1);

	if (bind(*sd1, (struct sockaddr *)addr_p, sizeof (struct sockaddr_in)) < 0) {
		printf("[ERROR-DRB] Failed to bind socket 1\n");
		close(*sd0);
		close(*sd1);
		close(*sd2);
		close(*sd3);
		return;
	}

	addr_p->sin_port = htons(PORT2);

	if (bind(*sd2, (struct sockaddr *)addr_p, sizeof (struct sockaddr_in)) < 0) {
		printf("[ERROR-DRB] Failed to bind socket 2\n");
		close(*sd0);
		close(*sd1);
		close(*sd2);
		close(*sd3);
		return;
	}

	addr_p->sin_port = htons(PORT3);

	if (bind(*sd3, (struct sockaddr *)addr_p, sizeof (struct sockaddr_in)) < 0) {
		printf("[ERROR-DRB] Failed to bind socket 3\n");
		close(*sd0);
		close(*sd1);
		close(*sd2);
		close(*sd3);
		return;
	}


	pthread_t handles[4];
	args_t *arg1 = (args_t*)malloc(sizeof(args_t));
	args_t *arg2 = (args_t*)malloc(sizeof(args_t));
	args_t *arg3 = (args_t*)malloc(sizeof(args_t));

	while(!exit_drb){

		printf("\n[DRB] Waiting for trigger\n\n");
		memset(peer,0,sizeof(struct sockaddr_in));
		while(peer->sin_port!=htons(6000)){
			recvfrom(*sd0,recbuf,sizeof(int),0,(struct sockaddr*)peer,&fromlen);
			exp_l = *recbuf;
			printf("Received trigger from %s/%d: %d, now starting to flush the sockets...\n",inet_ntoa(peer->sin_addr),ntohs(peer->sin_port),(int)recbuf[0]);
		}

		if (exp_l == 0){
			printf("[DRB-ERROR] experiment number 0\n");
			close(*sd0);
			close(*sd1);
			close(*sd2);
			close(*sd3);
			return;
		}

		pthread_barrier_t *barr = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
		pthread_barrier_init(barr,NULL,3);

		arg1->addr = peer;
		arg1->ctx = NULL;
		arg1->sd = sd1;
		arg1->exp = 11;//exp_l;
		arg1->jump = 0;
		arg1->barrier = barr;

		arg2->addr = peer;
		arg2->ctx = NULL;
		arg2->sd = sd2;
		arg2->exp = 11;//exp_l;
		arg2->jump = 5000;
		arg2->barrier = barr;

		arg3->addr = peer;
		arg3->ctx = NULL;
		arg3->sd = sd3;
		arg3->exp = 11;//exp_l;
		arg3->jump = 15000;
		arg3->barrier = barr;

		pthread_create(&handles[1],NULL,udp_flush_enb,(void*)arg1);
		pthread_create(&handles[2],NULL,udp_flush_enb,(void*)arg2);
		pthread_create(&handles[3],NULL,udp_flush_enb,(void*)arg3);

		pthread_join(handles[1],NULL);
		pthread_join(handles[2],NULL);
		pthread_join(handles[3],NULL);
	}


	close(*sd0);
	close(*sd1);
	close(*sd2);
	close(*sd3);
	return;
}

void udp_flush_enb(void* args){

	args_t *arg = (args_t*)args;
	int *sd_p = arg->sd;
	struct sockaddr_in *peer = arg->addr;
	size_t size_send = 0;
	switch(arg->exp){
	case 1:{
		size_send = 44;
	} break;
	case 2:{
		size_send = 121;
	} break;
	case 3:{
		size_send = 197;
	} break;
	case 4:{
		size_send = 273;
	} break;
	case 5:{
		size_send = 350;
	} break;
	case 6:{
		size_send = 426;
	} break;
	case 7:{
		size_send = 502;
	} break;
	case 8:{
		size_send = 578;
	} break;
	case 9:{
		size_send = 655;
	} break;
	case 10:{
		size_send = 731;
	} break;
	case 11:{
		size_send = 197;
	} break;
	case 12:{
		size_send = 426;
	} break;
	case 13:{
		size_send = 655;
	} break;
	case 14:{
		size_send = 884;
	} break;
	case 15:{
		size_send = 1113;
	} break;
	case 16:{
		size_send = 1341;
	} break;
	}


	// flush udp messages
	printf("[DRB] flush: sending over sd %d to address %s/%d\n",*sd_p,inet_ntoa(peer->sin_addr),ntohs(peer->sin_port));

	srand(time(0));
	int t1,t2;

	char buffer[BUFSIZE];
	int index;
	int jump_local = arg->jump;
	int *count = buffer;
	int *sd_buf = buffer+sizeof(int);
	*sd_buf = *sd_p;



	for (index=0;index<NUM_PACKETS;index++){

		// harmonize sockets when useful
		//pthread_barrier_wait(arg->barrier);

		*count = index+1;
		t1 = rand()%(T_GCL+jump_local);
		t2 = (T_GCL+jump_local) - t1;
		usleep(t1);
		//if ((index%jump_local)==0){
			sendto(*sd_p,buffer,size_send,0,(struct sockaddr *)peer,(socklen_t)sizeof(struct sockaddr));
		//}
		usleep(t2);
	}

	printf("[DRB] Ending to flush udp socket desc. %d\n",*sd_p);
	return;
}


void drb_process_itti_msg(ue_context_data_t *param_ctx){

	MessageDef *received_message_p = NULL;
	itti_receive_msg (TASK_DRB, &received_message_p);

	switch (ITTI_MSG_ID(received_message_p)) {

		case ATTACH_FORWARD_DRB:{
			itti_attach_forward_drb_t *container = &received_message_p->ittiMsg.attach_forward_drb;
			emm_data_context_t *ctx = (emm_data_context_t *)malloc(sizeof(emm_data_context_t));
			memcpy((void*)ctx,(void*)container->emm_ctx,sizeof(emm_data_context_t));

			param_ctx->emm_data = ctx;

			printf("\n[DRB] Attach complete: mme-ue_id: %d\n\n", ctx->ue_id);

			int ret[3];
			int pid = 0;

			// set up new bearer context

			unsigned int *ebi_a = (unsigned int*)calloc(3,sizeof(unsigned int));
			unsigned int *default_ebi = (unsigned int*)malloc(sizeof(unsigned int));
			int *esm_cause = (int*)malloc(sizeof(int));



			esm_proc_tft_t *tft = NULL;
			esm_proc_qos_t *qos;
			qos = (esm_proc_qos_t *)malloc(sizeof(esm_proc_qos_t));

			qos->qci = 8;		// default bearer 9
			qos->gbrDL = 120;	// same as default bearer
			qos->gbrUL = 64;
			qos->mbrDL = 135;
			qos->mbrUL = 72;




			param_ctx->ebi[0] = &ebi_a[0];
			param_ctx->ebi[1] = &ebi_a[1];
			param_ctx->ebi[2] = &ebi_a[2];


			ret[0] = esm_proc_dedicated_eps_bearer_context(ctx,pid,&ebi_a[0],default_ebi,qos,tft,esm_cause);
			printf("\n[DEBUG] created context for ebi %d, return value %d, now let's activate\n\n",ebi_a[0],ret[0]);

			ret[1] = esm_proc_dedicated_eps_bearer_context(ctx,pid,&ebi_a[1],default_ebi,qos,tft,esm_cause);
			printf("\n[DEBUG] created context for ebi %d, return value %d, now let's activate\n\n",ebi_a[1],ret[1]);

			ret[2] = esm_proc_dedicated_eps_bearer_context(ctx,pid,&ebi_a[2],default_ebi,qos,tft,esm_cause);
			printf("\n[DEBUG] created context for ebi %d, return value %d, now let's activate\n\n",ebi_a[2],ret[2]);


							//       type
			char buf_head[] = {0x00, 0x05, 0x00};

			char buf_pdu[] = {0x00, 0x00, 0x03, // num IE's
								0x00, 0x00, 0x00, 0x02, 0x00, 0x02,					// MME UE ID
								0x00, 0x08, 0x00, 0x04, 0x80, 0x06, 0x69, 0x2d,		// ENB UE ID
								0x00, 0x10, 0x00};								// list-bearerSU

			char buf_list[] = {0x00,	// num items in list - 1
					 	 	 	0x00, 0x11, 0x00};					// item-bearerSU


			char buf_item0[] = {0x0c, 0x00, 0x04, 0x04, 0x0f, 0x80,
								 0xac, 0x10, 0x02, 0x8d,		// transport layer address
							     0x00, 0x00, 0x00, 0x03};		// gtp tunnel id

			char buf_nas0[] = {0x27, 0x0f, 0xa9, 0x97, 0x5a, 0x02,
								0x62, 0x00, 0xc5, 0x05,
								0x01, 0x08};

			char buf_tft0[] = {0x21, 0x31, 0x00, 0x03, 0x50, 0x13, 0x89};


			char buf_list1[] = {0x00, 0x00, 0x11, 0x00};

			char buf_item1[] = {0x0e, 0x00, 0x04, 0x04, 0x0f, 0x80,
					 0xac, 0x10, 0x02, 0x8d,		// transport layer address
				     0x00, 0x00, 0x00, 0x04};		// gtp tunnel id

			char buf_nas1[] = {0x27, 0x0f, 0xa9, 0x97, 0x5a, 0x02,
								0x62, 0x00, 0xc5, 0x05,
								0x01, 0x08};

			char buf_tft1[] = {0x21, 0x31, 0x00, 0x03, 0x50, 0x13, 0x89};



			char buf_list2[] = {0x00, 0x00, 0x11, 0x00};

			char buf_item2[] = {0x10, 0x00, 0x04, 0x04, 0x0f, 0x80,
					 0xac, 0x10, 0x02, 0x8d,		// transport layer address
				     0x00, 0x00, 0x00, 0x05};		// gtp tunnel id

			char buf_nas2[] = {0x27, 0x0f, 0xa9, 0x97, 0x5a, 0x02,
								0x62, 0x00, 0xc5, 0x05,
								0x01, 0x08};

			char buf_tft2[] = {0x21, 0x31, 0x00, 0x03, 0x50, 0x13, 0x89};

#ifdef ENC

			printf("[DEBUG] initializing msg 0\n");

			// initialize NAS messages
			char plain_nas0[1000],plain_nas1[1000],plain_nas2[1000];
			//char plain_nas1[1000],plain_nas2[1000];
			nas_message_t msg_plain0,msg_plain1,msg_plain2;


			// esm message container
			memset(&msg_plain0,0,sizeof(msg_plain0));
			memset(&msg_plain1,0,sizeof(msg_plain1));
			memset(&msg_plain2,0,sizeof(msg_plain2));
			//nas_message_security_header_t * sec_header = &msg_plain0.security_protected.header;
			activate_dedicated_eps_bearer_context_request_msg * sec_msg_esm0 = &msg_plain0.plain.esm.activate_dedicated_eps_bearer_context_request;
			activate_dedicated_eps_bearer_context_request_msg * sec_msg_esm1 = &msg_plain1.plain.esm.activate_dedicated_eps_bearer_context_request;
			activate_dedicated_eps_bearer_context_request_msg * sec_msg_esm2 = &msg_plain2.plain.esm.activate_dedicated_eps_bearer_context_request;



			sec_msg_esm0->epsbeareridentity = 0b0110;
			sec_msg_esm0->epsqos.bitRatesExtPresent = 0b0;
			sec_msg_esm0->epsqos.bitRatesPresent = 0b0;
			sec_msg_esm0->epsqos.qci = 4;
			sec_msg_esm0->linkedepsbeareridentity = 5;
			sec_msg_esm0->messagetype = ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST;
			sec_msg_esm0->presencemask = 0;
			sec_msg_esm0->proceduretransactionidentity = 0;
			sec_msg_esm0->protocoldiscriminator = 0b0010;
			sec_msg_esm0->transactionidentifier.field = 0;
			sec_msg_esm0->tft.numberofpacketfilters = 0b0001;
			sec_msg_esm0->tft.ebit = 0b0;
			sec_msg_esm0->tft.tftoperationcode = TRAFFIC_FLOW_TEMPLATE_OPCODE_CREATE;
			sec_msg_esm0->tft.packetfilterlist.createtft[0].direction = TRAFFIC_FLOW_TEMPLATE_BIDIRECTIONAL;
			sec_msg_esm0->tft.packetfilterlist.createtft[0].eval_precedence = 0;
			sec_msg_esm0->tft.packetfilterlist.createtft[0].identifier = 0b0001;
			sec_msg_esm0->tft.packetfilterlist.createtft[0].packetfilter.singleremoteport = 6001;
			sec_msg_esm0->tft.packetfilterlist.createtft[0].packetfilter.flags = TRAFFIC_FLOW_TEMPLATE_SINGLE_REMOTE_PORT_FLAG;

			sec_msg_esm1->epsbeareridentity = 0b0111;
			sec_msg_esm1->epsqos.bitRatesExtPresent = 0b0;
			sec_msg_esm1->epsqos.bitRatesPresent = 0b0;
			sec_msg_esm1->epsqos.qci = 4;
			sec_msg_esm1->linkedepsbeareridentity = 5;
			sec_msg_esm1->messagetype = ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST;
			sec_msg_esm1->presencemask = 0;
			sec_msg_esm1->proceduretransactionidentity = 0;
			sec_msg_esm1->protocoldiscriminator = 0b0010;
			sec_msg_esm1->transactionidentifier.field = 0;
			sec_msg_esm1->tft.numberofpacketfilters = 0b0001;
			sec_msg_esm1->tft.ebit = 0b0;
			sec_msg_esm1->tft.tftoperationcode = TRAFFIC_FLOW_TEMPLATE_OPCODE_CREATE;
			sec_msg_esm1->tft.packetfilterlist.createtft[0].direction = TRAFFIC_FLOW_TEMPLATE_BIDIRECTIONAL;
			sec_msg_esm1->tft.packetfilterlist.createtft[0].eval_precedence = 0;
			sec_msg_esm1->tft.packetfilterlist.createtft[0].identifier = 0b0001;
			sec_msg_esm1->tft.packetfilterlist.createtft[0].packetfilter.singleremoteport = 6002;
			sec_msg_esm1->tft.packetfilterlist.createtft[0].packetfilter.flags = TRAFFIC_FLOW_TEMPLATE_SINGLE_REMOTE_PORT_FLAG;

			sec_msg_esm2->epsbeareridentity = 0b1000;
			sec_msg_esm2->epsqos.bitRatesExtPresent = 0b0;
			sec_msg_esm2->epsqos.bitRatesPresent = 0b0;
			sec_msg_esm2->epsqos.qci = 4;
			sec_msg_esm2->linkedepsbeareridentity = 5;
			sec_msg_esm2->messagetype = ACTIVATE_DEDICATED_EPS_BEARER_CONTEXT_REQUEST;
			sec_msg_esm2->presencemask = 0;
			sec_msg_esm2->proceduretransactionidentity = 0;
			sec_msg_esm2->protocoldiscriminator = 0b0010;
			sec_msg_esm2->transactionidentifier.field = 0;
			sec_msg_esm2->tft.numberofpacketfilters = 0b0001;
			sec_msg_esm2->tft.ebit = 0b0;
			sec_msg_esm2->tft.tftoperationcode = TRAFFIC_FLOW_TEMPLATE_OPCODE_CREATE;
			sec_msg_esm2->tft.packetfilterlist.createtft[0].direction = TRAFFIC_FLOW_TEMPLATE_BIDIRECTIONAL;
			sec_msg_esm2->tft.packetfilterlist.createtft[0].eval_precedence = 0;
			sec_msg_esm2->tft.packetfilterlist.createtft[0].identifier = 0b0001;
			sec_msg_esm2->tft.packetfilterlist.createtft[0].packetfilter.singleremoteport = 6003;
			sec_msg_esm2->tft.packetfilterlist.createtft[0].packetfilter.flags = TRAFFIC_FLOW_TEMPLATE_SINGLE_REMOTE_PORT_FLAG;


			int bytes_encoded[3] = {0,0,0};

			bytes_encoded[0] = nas_message_encode(plain_nas0,&msg_plain0,sizeof(plain_nas0),(void*)&ctx->_security);
			bytes_encoded[1] = nas_message_encode(plain_nas1,&msg_plain1,sizeof(plain_nas1),(void*)&ctx->_security);
			bytes_encoded[2] = nas_message_encode(plain_nas2,&msg_plain2,sizeof(plain_nas2),(void*)&ctx->_security);


			//char plain_nas0[] = {0x62, 0x00, 0xc5, 0x05, 0x01, 0x04, 0x07, 0x21, 0x31, 0x00, 0x03, 0x50, 0x17, 0x71};
			//char plain_nas1[] = {0x72, 0x00, 0xc5, 0x05, 0x01, 0x04, 0x07, 0x21, 0x31, 0x00, 0x03, 0x50, 0x17, 0x72};
			//char plain_nas2[] = {0x82, 0x00, 0xc5, 0x05, 0x01, 0x04, 0x07, 0x21, 0x31, 0x00, 0x03, 0x50, 0x17, 0x73};

			char encrypted_nas0[50],encrypted_nas1[50],encrypted_nas2[50];

/*
			// plain_nas0 wird Inhalt eines NAS transports von EMM

			nas_message_t plain_emm0;
			downlink_nas_transport_msg *msg_emm_plain0 = &plain_emm0.plain.emm.downlink_nas_transport;

			msg_emm_plain0->messagetype = 0x62;
			msg_emm_plain0->securityheadertype = 0b0000;
			msg_emm_plain0->protocoldiscriminator = 0b0111;
			bstring nas_container0 = malloc(sizeof(struct tagbstring));
			char *nas_data0 = malloc(50);
			msg_emm_plain0->nasmessagecontainer = nas_container0;
			nas_container0->data = nas_data0;
			memcpy(nas_data0,plain_nas0,bytes_encoded[0]);
			nas_container0->slen = bytes_encoded[0];
			nas_container0->mlen = bytes_encoded[0]+2;
			char plain_nas0_1[1000];
			bytes_encoded[0] = nas_message_encode(plain_nas0_1,&plain_emm0,sizeof(plain_nas0_1),(void*)&ctx->_security);
*/

			nas_message_security_header_t header0,header1,header2;

			// initialize NAS headers
			header0.security_header_type = 2;
			header0.protocol_discriminator = 7;
			header0.sequence_number = 1;
			memcpy(&header1,&header0,sizeof(header0));
			memcpy(&header2,&header0,sizeof(header0));

			// get emm security context
			emm_security_context_t sec_ctx = ctx->_security;

			printf("[DEBUG] encrypting all messages\n");

			// encrypt the plain NAS messages

			int bytes_encrypted[3] = {0,0,0};

			bytes_encrypted[0] = nas_message_encrypt(plain_nas0,encrypted_nas0,&header0,bytes_encoded[0]+6,(void*)&sec_ctx);
			bytes_encrypted[1] = nas_message_encrypt(plain_nas1,encrypted_nas1,&header1,bytes_encoded[1]+6,(void*)&sec_ctx);
			bytes_encrypted[2] = nas_message_encrypt(plain_nas2,encrypted_nas2,&header2,bytes_encoded[2]+6,(void*)&sec_ctx);
#endif



#ifdef ENC
			uint8_t size_nas2 = bytes_encrypted[2];
#else
			uint8_t size_tft2 = sizeof(buf_tft2);
			uint8_t size_nas2 = size_tft2 + 1 + sizeof(buf_nas2);
#endif
			uint8_t size_item2 = size_nas2 + 1 + sizeof(buf_item2);

#ifdef ENC
			uint8_t size_nas1 = bytes_encrypted[1];
#else
			uint8_t size_tft1 = sizeof(buf_tft1);
			uint8_t size_nas1 = size_tft1 + 1 + sizeof(buf_nas1);
#endif
			uint8_t size_item1 = size_nas1 + 1 + sizeof(buf_item1);

#ifdef ENC
			uint8_t size_nas0 = bytes_encrypted[0];
#else
			uint8_t size_tft0 = sizeof(buf_tft0);
			uint8_t size_nas0 = size_tft0 + 1 + sizeof(buf_nas0);
#endif
			uint8_t size_item0 = size_nas0 + 1 + sizeof(buf_item0);

			uint8_t size_list = size_item0 + 1 + sizeof(buf_list);

			uint8_t size_pdu = size_list + 1 + sizeof(buf_pdu);


			char *payload = calloc(256,sizeof(char));

			int loaded = 0;

			uint8_t id[4];
			uint8_t *cp = (uint8_t *)&param_ctx->enb_id;
			id[0]=0x80;
			id[1]=cp[2];
			id[2]=cp[1];
			id[3]=cp[0];

			printf("[DEBUG] beginning to write payload, bytes_encoded[0] of msg 0: %d\n",bytes_encoded[0]);

			memcpy(payload,buf_head,sizeof(buf_head));
			loaded+=sizeof(buf_head);
			payload[loaded] = size_pdu;
			loaded+=1;
			memcpy(payload+loaded,buf_pdu,sizeof(buf_pdu));
			memcpy(payload+loaded+13,&id,sizeof(id));
			loaded+=sizeof(buf_pdu);
			payload[loaded] = size_list;
			loaded+=1;

			int loaded_list = loaded;

			memcpy(payload+loaded,buf_list,sizeof(buf_list));
			loaded+=sizeof(buf_list);
			payload[loaded] = size_item0;
			loaded+=1;
			memcpy(payload+loaded,buf_item0,sizeof(buf_item0));
			loaded+=sizeof(buf_item0);
#ifdef ENC
			payload[loaded] = bytes_encrypted[0];
			loaded+=1;
			memcpy(payload+loaded,encrypted_nas0,bytes_encrypted[0]);
			loaded+=bytes_encrypted[0];
#else
			payload[loaded] = size_nas0;
			loaded+=1;
			memcpy(payload+loaded,buf_nas0,sizeof(buf_nas0));
			loaded+=sizeof(buf_nas0);
			payload[loaded] = size_tft0;
			loaded+=1;
			memcpy(payload+loaded,buf_tft0,sizeof(buf_tft0));
			loaded+=sizeof(buf_tft0);
#endif
			sctp_assoc_id_t sctp_assoc_id = 3;
			uint16_t stream = 1;

			// 1
			sctp_sendmsg (44, (const void *)payload, loaded, NULL, 0, htonl(18), 0, stream, 0, 0);
			esm_ebr_set_status(ctx,ebi_a[0],ESM_EBR_ACTIVE_PENDING,false);


			loaded = loaded_list;
			memcpy(payload+loaded,buf_list1,sizeof(buf_list1));
			loaded+=sizeof(buf_list1);
			payload[loaded] = size_item1;
			loaded+=1;
			memcpy(payload+loaded,buf_item1,sizeof(buf_item1));
			loaded+=sizeof(buf_item1);
#ifdef ENC
			payload[loaded]=bytes_encrypted[1];
			loaded+=1;
			memcpy(payload+loaded,encrypted_nas1,bytes_encrypted[1]);
			loaded+=bytes_encrypted[1];
#else
			payload[loaded] = size_nas1;
			loaded+=1;
			memcpy(payload+loaded,buf_nas1,sizeof(buf_nas1));
			loaded+=sizeof(buf_nas1);
			payload[loaded] = size_tft1;
			loaded+=1;
			memcpy(payload+loaded,buf_tft1,sizeof(buf_tft1));
			loaded+=sizeof(buf_tft1);
#endif
			// 2
			usleep(300000);
			sctp_sendmsg (44, (const void *)payload, loaded, NULL, 0, htonl(18), 0, stream, 0, 0);
			esm_ebr_set_status(ctx,ebi_a[1],ESM_EBR_ACTIVE_PENDING,false);


			loaded = loaded_list;
			memcpy(payload+loaded,buf_list2,sizeof(buf_list2));
			loaded+=sizeof(buf_list2);
			payload[loaded] = size_item2;
			loaded+=1;
			memcpy(payload+loaded,buf_item2,sizeof(buf_item2));
			loaded+=sizeof(buf_item2);
#ifdef ENC
			payload[loaded]=bytes_encrypted[2];
			loaded+=1;
			memcpy(payload+loaded,encrypted_nas2,bytes_encrypted[2]);
			loaded+=bytes_encrypted[2];
#else
			payload[loaded] = size_nas2;
			loaded+=1;
			memcpy(payload+loaded,buf_nas2,sizeof(buf_nas2));
			loaded+=sizeof(buf_nas2);
			payload[loaded] = size_tft2;
			loaded+=1;
			memcpy(payload+loaded,buf_tft2,sizeof(buf_tft2));
			loaded+=sizeof(buf_tft2);
#endif

			// 3
			usleep(300000);
			sctp_sendmsg (44, (const void *)payload, loaded, NULL, 0, htonl(18), 0, stream, 0, 0);
			esm_ebr_set_status(ctx,ebi_a[2],ESM_EBR_ACTIVE_PENDING,false);

			param_ctx->counter_UE = param_ctx->counter_UE + 1;

			args_t *arguments=malloc(sizeof(args_t));
			arguments->ctx = param_ctx;

			pthread_t handle;
			pthread_create(&handle,NULL,sockets_setup,(void*)arguments);
		}

		break;

		case S1AP_NAS_DL_DATA_REQ:{
			// DRB state can be set to ACTIVE


			int * ebi_local_p = (int*)param_ctx->ebi[0];
			int ebi_local = *ebi_local_p;
			printf("[DEBUG] DRB entering esm_ebr_set_status with ebi=%d, context pointer=%p\n",
					ebi_local,param_ctx->emm_data);
			esm_ebr_set_status(param_ctx->emm_data,ebi_local,ESM_EBR_ACTIVE,false);

			ebi_local_p = (int*)param_ctx->ebi[1];
			ebi_local = *ebi_local_p;
			printf("[DEBUG] DRB entering esm_ebr_set_status with ebi=%d, context pointer=%p\n",
					ebi_local,param_ctx->emm_data);
			esm_ebr_set_status(param_ctx->emm_data,ebi_local,ESM_EBR_ACTIVE,false);

			ebi_local_p = (int*)param_ctx->ebi[2];
			ebi_local = *ebi_local_p;
			printf("[DEBUG] DRB entering esm_ebr_set_status with ebi=%d, context pointer=%p\n",
					ebi_local,param_ctx->emm_data);
			esm_ebr_set_status(param_ctx->emm_data,ebi_local,ESM_EBR_ACTIVE,false);


		}
		break;

		case MME_APP_INITIAL_UE_MESSAGE: {
			itti_mme_app_initial_ue_message_t *cont = &received_message_p->ittiMsg.mme_app_initial_ue_message;
			param_ctx->enb_id = cont->enb_ue_s1ap_id;
			char *cp = &param_ctx->enb_id;
			printf("\n[DRB] Received initial UE message. enb-id: %x %x %x %x, %d\n\n",
					cp[0],cp[1],cp[2],cp[3],param_ctx->enb_id);
		}
		break;

		case TERMINATE_MESSAGE:{
			exit_drb = 1;
		}
		break;

		default:{
		}
		break;
	}

	return;
}


// Start-routine for DRB task, wait for itti messages
void * drb_task(void* args){
	itti_mark_task_ready(TASK_DRB);

	int index;

	printf("[DRB] Task initiated\n");

	ue_context_data_t *ue_ctx = (ue_context_data_t *)malloc(sizeof(ue_context_data_t));

	ue_ctx->ebi[0] = NULL;
	ue_ctx->ebi[1] = NULL;
	ue_ctx->ebi[2] = NULL;

	ue_ctx->emm_data = NULL;
	ue_ctx->enb_id = 0;
	ue_ctx->counter_UE = 0;


	while(!exit_drb){
		drb_process_itti_msg(ue_ctx);
	}

	printf("[DRB] Task terminating\n");
	free(ue_ctx);

	return NULL;
}


// create Itti-task (Thread) for DRB-Module as a part of MME
int drb_init(void){
	int ret = 0;
	exit_drb = 0;

	if (itti_create_task(TASK_DRB, &drb_task, NULL)<0){
		ret = -1;
	}

	return ret;
}
