/*
 * drb.h
 *
 *  Created on: Oct 26, 2020
 *      Author: root
 */

#ifndef SRC_DRB_STARTER_DRB_H_
#define SRC_DRB_STARTER_DRB_H_


#define PORT0 7007
#define PORT1 6001
#define PORT2 6002
#define PORT3 6003
#define PORT_UE 6000
#define ADDR_EPC "192.168.178.42"
#define ADDR_UE "172.16.0.4"
#define T_GCL  4850//us

#define BUFSIZE 1000
#define NUM_PACKETS 400

typedef struct ue_context_data_s {
	enb_ue_s1ap_id_t enb_id;
	unsigned int *ebi[3];
	emm_data_context_t *emm_data;
	int counter_UE;
} ue_context_data_t;

int drb_init(void);
void udp_flush_enb(void* unused);
void sockets_setup(void* args);
void * drb_task(void* args);
void drb_process_itti_msg(ue_context_data_t *param_ctx);


#endif /* SRC_DRB_STARTER_DRB_H_ */

