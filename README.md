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
