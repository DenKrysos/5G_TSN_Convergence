# 5G_TSN_Convergence
Codes for thesis of Benedikt Veith

## Versions
* openair-cn:
* OAI eNB:

## Files
### packet_terminal
Takes downlink messages coming from EPC and manipulates GTP headers, so the packets are mapped to different Radio Bearers in the eNB.
The mapping depends on the UDP port of the sender.
The Terminal is configured by receiving messages from the eNB, which contain the GTP TEID of the newly established connection.

### LCP Receiver
Application running behind the UE, which receives the Downlink messages and stores a table of timestamps, ordered by UDP sender port.

### openair-cn
Module added:
* src/drb_starter/: initializes a configuration procedure for several data radio bearers in the eNB, after the UE has atteched successfully.

Modules manipulated:
* src/nas/: sends a message to drb starter at reception of 'attach-complete'
* src/s1ap/: send messages to drb starter at reception of 'mme-app-initial-ue-message' and 'e-rab-setup with successful outcome'
* build/: add drb starter task to CMakeLists.txt

### openairinterface5g
Module added:
* openair3/ICM/: listens to ITTI-messaging in the eNB and sends configurations (GTP TEIDs) to packet terminal

Modules manipulated:
* cmake_targets/CMakeLists.txt: add ICM module to build process
* common/utils/ocp_itti/: send copies of ITTI messages to ICM task
* targets/COMMON/: initialize ICM task along with others
* openair3/UDP/: change Uplink paths, so all user plane traffic is sent to packet terminal instead of SPGW
* openair2/LAYER2/MAC/eNB_scheduler_dlsch.c : manipulate LCID-arrays and RLC headers to enforce dLCP execution and then merge downlink traffic to a single bearer again

