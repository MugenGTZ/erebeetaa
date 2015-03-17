#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "../elevatorProtocol/UDP.h"
#include "../elevatorProtocol/UDP.h"
#include "../elevatorProtocol/elevatorProtocol.h"

#define BUFSIZE 1024
#define PORT_LISTEN_COST 20000
#define PORT_SEND_COST   30000
#define PORT_LISTEN_REQ  40000
#define PORT_SEND_REQ    50000


typedef struct _extendedOrder{
	order 		myOrder;									//id, floor, direction
	ulong		idOrigin;									//id of origin machine
	ulong		idDest;										//id of serving machine
	int			cost;										//cost of order
	int			status; 									//0 - done 1 - not_accepted 2 - created 3 - accepted 
}extendedOrder;

typedef struct _udpPack{
	char			md5Sum[100];
	extendedOrder 	orderQueue[8];
	ulong			idOrigin;								//id of the machine that created this pack
	int				reqOrderIdx;
}udpPack;

int cost = 5;
ulong peerID = 1545415;

void* bcastListen(void *);
void* reqListen  (void *);

int main(){

	pthread_t bcastListener;
	pthread_t reqListener;

	if(pthread_create(&bcastListener, NULL, bcastListen, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}

	if(pthread_create(&reqListener, NULL, reqListen, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
	
	pthread_join(bcastListener, NULL);
	pthread_join(reqListener, NULL);

return 0;
}
		

void* bcastListen(void *){
		
	struct sockaddr_in myaddr; 											//our address  
	struct sockaddr_in remaddr; 										//remote sender address  
	struct  sockaddr_in rcv_addr;										//remote receiver address
	socklen_t addrlen1 = sizeof(remaddr); 								//length of addresses 
	socklen_t addrlen2 = sizeof(rcv_addr);
	int recvlen; 														// bytes received 
	int rcvSckCost, sendSckCost;			 							// our sockets 
	unsigned char rcv_buf[BUFSIZE],send_buf[BUFSIZE] = "hola"; 			// receive and transmit buffer
	udpPack rcvPack;

	rcvSckCost = createUDPSocket();
	bindToSocket(rcvSckCost, PORT_LISTEN_COST);

	sendSckCost = createUDPSocket();
	

	while(1){
		// wait for UDP message
		printf("Listening to port %d\n", PORT_LISTEN_COST);
		if ((recvlen = recvfrom(rcvSckCost, &rcvPack, sizeof(udpPack), 0, (struct sockaddr *)&remaddr, &addrlen1)) == -1){
			perror("read");
			exit(EXIT_FAILURE);
		}

		printf("UDP ms received! \n");
		
		//update cost
		for(int i=0; i<8; i++){
			rcvPack.orderQueue[i].cost = cost;
			rcvPack.orderQueue[i].idDest = peerID;
		}
		rcvPack.idOrigin = peerID;

		//send your cost
		setReceiver(&rcv_addr, addrlen2, inet_ntoa(remaddr.sin_addr), PORT_SEND_COST);
		if(sendto(sendSckCost, &rcvPack, sizeof(udpPack), 0, (struct sockaddr *)&rcv_addr, addrlen2) > 0)
			printf("Successful sending\n");

		sleep(1);
	}
	
return 0;
}
		
void* reqListen  (void *){
	struct sockaddr_in myaddr; 											//our address  
	struct sockaddr_in remaddr; 										//remote sender address  
	struct  sockaddr_in rcv_addr;										//remote receiver address
	socklen_t addrlen1 = sizeof(remaddr); 								//length of addresses 
	socklen_t addrlen2 = sizeof(rcv_addr);
	int recvlen; 														// bytes received 
	int rcvSckCReq, sendSckReq;			 		
	udpPack rcvPack;

	rcvSckCReq = createUDPSocket();
	bindToSocket(rcvSckCReq, PORT_LISTEN_REQ);

	sendSckReq = createUDPSocket();

	while(1){
		
		// wait for UDP message
		printf("Listening to port %d\n", PORT_LISTEN_REQ);
		if ((recvlen = recvfrom(rcvSckCReq, &rcvPack, sizeof(udpPack), 0, (struct sockaddr *)&remaddr, &addrlen1)) == -1){
			perror("read");
			exit(EXIT_FAILURE);
		}

		printf("Connectin established!\n");

		bool acceptOrder;
		if((rand() % 10) >= 1){
			acceptOrder = true;
			rcvPack.orderQueue[rcvPack.reqOrderIdx].status = 3;			//accepted order
		}
		else{
			acceptOrder = false;
			rcvPack.orderQueue[rcvPack.reqOrderIdx].status = 1;			//not accepted order
		}
		
		setReceiver(&rcv_addr, addrlen2, inet_ntoa(remaddr.sin_addr), PORT_SEND_REQ);
		if(sendto(sendSckReq, &rcvPack, sizeof(udpPack), 0, (struct sockaddr *)&rcv_addr, addrlen2) > 0)
			printf("Successful sending\n");

	}
return 0;
}


 
