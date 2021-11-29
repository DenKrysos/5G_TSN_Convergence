 #undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <time.h>

#define DEFAULT_BUFLEN 1500
#define DEFAULT_PORT "6000"
#define NUM_PACKETS 350

int main(void) 
{
    WSADATA wsaData;
    int iResult;
    int index;

    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct sockaddr_in sender;

    int iSendResult;
    unsigned char *recvbuf = (unsigned char*)malloc(DEFAULT_BUFLEN);
    int recvbuflen = DEFAULT_BUFLEN;
    int addr_len = sizeof(sender);

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }


    // Create a SOCKET for connecting to server
    ClientSocket = socket(AF_INET,SOCK_DGRAM,0);
    if (ClientSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }


    // Set up the UDP socket
    SOCKADDR_IN my_addr;
    my_addr.sin_port = htons(6000);
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr("172.16.0.2");
    ZeroMemory(&my_addr.sin_zero, sizeof(char));
	
	iResult = bind( ClientSocket, (SOCKADDR*)&my_addr, sizeof(SOCKADDR));
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }
    
    printf("Socket bound to address %s, Port %u\n",inet_ntoa(my_addr.sin_addr),ntohs(my_addr.sin_port));
    
    
    // send start-trigger to EPC
    SOCKADDR_IN peer;
	peer.sin_addr.s_addr = inet_addr("192.168.178.42");
    peer.sin_family = AF_INET;
    peer.sin_port = htons(7007);
    
    int *send_buf = malloc(sizeof(int));
    printf("Ready to send trigger. type an index for experiment:\n"); 
    scanf("%d",send_buf);
    sendto(ClientSocket,(char*)send_buf,sizeof(int),0,(SOCKADDR*)&peer,sizeof(SOCKADDR));    
    
    int perc = (*send_buf)*(10);
    
    int port_send[NUM_PACKETS];
    ZeroMemory(port_send,NUM_PACKETS*sizeof(int));
    LARGE_INTEGER StartingTime, RecvTime[NUM_PACKETS], ElapsedMicroseconds[NUM_PACKETS];
    ZeroMemory(ElapsedMicroseconds,NUM_PACKETS*sizeof(LARGE_INTEGER));
	LARGE_INTEGER Frequency;
	
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartingTime);
    int bytes[NUM_PACKETS];
    
    // receive packets from the 3 ports and store timing measurment
    printf("now receiving...\n");
    index=0;
    for (index=0;index<(NUM_PACKETS);index++){
		bytes[index] = recvfrom(ClientSocket,recvbuf,recvbuflen,0,(SOCKADDR*)&sender,&addr_len);
    	QueryPerformanceCounter(&RecvTime[index]);
    	//printf("Received %d bytes, index %d\n",bytes[index],index);
    	port_send[index]=sender.sin_port;
	}
	 
	if (bytes[NUM_PACKETS-1]<0){
		printf("recvfrom failed with error: %d\n", WSAGetLastError());
	} 
	
	// print values into text file
	FILE *fp;
	char title[50];
	sprintf(title,"2_15_times_P20_%d_aktiv.txt",perc);
	fp = fopen(title,"a");
	
	for(index=0;index<NUM_PACKETS;index++){
		ElapsedMicroseconds[index].QuadPart = RecvTime[index].QuadPart - StartingTime.QuadPart;
		ElapsedMicroseconds[index].QuadPart *= 1000000;
		ElapsedMicroseconds[index].QuadPart /= Frequency.QuadPart;
		port_send[index] = ntohs(port_send[index])-6000;
	}
	
	fprintf(fp,"\n\n");
	fprintf(fp,"times=[");
	for(index=0;index<(NUM_PACKETS-1);index++){
		fprintf(fp,"%u,",ElapsedMicroseconds[index].QuadPart);
	}
	fprintf(fp,"%u];",ElapsedMicroseconds[NUM_PACKETS-1].QuadPart);
	
	fprintf(fp,"\nports=[");
	for(index=0;index<(NUM_PACKETS-1);index++){
		fprintf(fp,"%u,",port_send[index]);
	}
	fprintf(fp,"%u];\n",port_send[NUM_PACKETS-1]);

    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}

