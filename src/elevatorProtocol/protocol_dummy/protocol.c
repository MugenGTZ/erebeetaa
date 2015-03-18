#include "../elevatorProtocol.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "../UDP.h"
#include "../../../../Networking/src/network.h"

#define PORT 20000											//port for broadcast
#define PORT_lst 30000

typedef struct _extendedOrder{
	order 		myOrder;									//id, floor, direction
	netcard		origin;										//netcard origin machine
	netcard		dest;										//netcard serving machine
	int			cost;										//cost of order
	int			status; 									//0 - done 1 - waiting 2 - created
}extendedOrder;

typedef struct _udpPack{
	extendedOrder 	orderQueue[8];
	netcard			origin;									//id of the machine that created this pack
}udpPack;

udpPack Pack01;
extendedOrder 	*orderQueue = Pack01.orderQueue;


bool(*_sendOrder2Elevator)(ulong, int, int);
int(*_getCost2Go2Floor)(int, int);

void* dispatchOrders(void *);
/////////////////////////////////////////////////////////////////////////
void networkConnectionlessCallBack(netcard num, char *data, int32_t len){
	if(len != sizeof(udpPack)) {printf("size missmatch!\n");return;}
	udpPack *tmpPack = (udpPack*)data;
	
	
	if( getMyNetCard() == tmpPack->origin){
		//greedy algorithm
		for(int i = 0; i < 8; i++){
			if(tmpPack->orderQueue[i].status == 2){
				if(tmpPack->orderQueue[i].cost < Pack01.orderQueue[i].cost){
					if(tmpPack->orderQueue[i].myOrder.ID == Pack01.orderQueue[i].myOrder.ID){
						Pack01.orderQueue[i].dest = tmpPack->orderQueue[i].dest;
						Pack01.orderQueue[i].cost = tmpPack->orderQueue[i].cost;
					}
				}
			}
		}
	}
	else{
		bool mustSend = false;
		for(int i = 0; i < 8; i++){
			order tempOrder = tmpPack->orderQueue[i].myOrder;
			if(tmpPack->orderQueue[i].status == 2){
				int cost = _getCost2Go2Floor(tempOrder.floor, tempOrder.direction);
				tmpPack->orderQueue[i].dest = getMyNetCard();
				tmpPack->orderQueue[i].cost = cost;
				if(cost != -1) mustSend = true;
			}
		}
		if(mustSend){
			printf("Sending cost to %lu\n",tmpPack->origin);
			sendData(tmpPack->origin, (char*)&Pack01, sizeof(udpPack));
		}
	}
	
	
	/*char _data[256];
	ssize_t _len;
	nErr error;

	printf("Received pack from:%llu with:%s\n",(long long unsigned int)num, data);
	chan Channel = createChannel(num);
	if(Channel == (chan)-1) {printf("ERROR TCP\n");return;}	
	displayErr(chanSend(Channel, (char*)"Can you hear me?", strlen("Can you hear me?") + 1));

	if(NE_NO_ERROR != (error = chanRecv(Channel, _data, &_len, 256, 5))){ displayErr(error); closeChannel(Channel); return; }
	_data[_len] = 0;	
	printf("Received from %llu: %s\n", (unsigned long long)num, _data);

	closeChannel(Channel);*/
}

void networkConnectionOrientedCallBack(chan extChan){
	/*char data[256];
	ssize_t len;
	nErr error;

	if(NE_NO_ERROR != (error = chanRecv(extChan, data, &len, 256, 5))){ displayErr(error); closeChannel(extChan); return; }
	data[len] = 0;	
	printf("Received: %s\n", data);

	displayErr(chanSend(extChan, (char*)"Ok, got you!", strlen("Ok, got you!")+1));
	closeChannel(extChan);*/
}

/////////////////////////////////////////////////////////////////////////


void protocolInit(bool(*sendOrder2Elevator)(ulong, int, int), int(*getCost2Go2Floor)(int, int)){
	static int hasInitialize;
	pthread_t dispatcher;
	//pthread_t UDP_listener;

	initNetwork();
	chanServerCallBack(networkConnectionOrientedCallBack);
	dataServerCallBack(networkConnectionlessCallBack);

	if(!hasInitialize){
		srand(time(0));
		_sendOrder2Elevator = sendOrder2Elevator;
		_getCost2Go2Floor	= getCost2Go2Floor;
		Pack01.origin		= getMyNetCard();
		hasInitialize++;
	}

	if(pthread_create(&dispatcher, NULL, dispatchOrders, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
	
}


int sendOrder(order o){
	int idx = o.floor*2+((o.direction==1)?1:0);
	orderQueue[idx].myOrder 	= o;
	orderQueue[idx].origin 		= getMyNetCard();
	orderQueue[idx].dest 		= (netcard)-1;
	orderQueue[idx].status 		= 2;
	//_sendOrder2Elevator(o.ID, o.floor, o.direction);
	return 0;
}


void internalOrderDone(ulong id){
	for(int i = 0; i < 8;i++){
		order tempOrder = orderQueue[i].myOrder;
		if(tempOrder.ID == id) orderQueue[i].status = 0;
	}
}


void* dispatchOrders(void *){
	while(1){
		//make Pack
		for(int i = 0; i < 8; i++){
			order tempOrder = orderQueue[i].myOrder;
			orderQueue[i].dest = getMyNetCard();
			if(orderQueue[i].status == 2)
				orderQueue[i].cost = _getCost2Go2Floor(tempOrder.floor, tempOrder.direction);
			else
				orderQueue[i].cost = -1;
		}
		
		sendBroadcast((char*)&Pack01, sizeof(udpPack));
		sleep(2);
		
		//find the min cost
		unsigned int minCost = -1;
		int minCostIdx = -1;
		for(int i = 0; i < 8; i++){
			if(orderQueue[i].status == 2){
				unsigned int cost = orderQueue[i].cost;
				if(cost < minCost){
					minCost = cost;
					minCostIdx = i;
				}
			}
		}
		//sendData(NETCARDBROADCAST, (char*)"Hello Filip!", 13);
		//Broadcast all orders over UDP
		//Find minimum Cost of all matrices
		
		if(minCost != (unsigned int)-1){
			//If minimum cost is outside make a thread to connect via TCP to remote peer
			if(orderQueue[minCostIdx].dest != getMyNetCard()){
				printf("Alien will do this\n");
			}
			//Else do the task
			else{
				printf("this computer should serve this!\n");	
				order tempOrder = orderQueue[minCostIdx].myOrder;
				_sendOrder2Elevator(tempOrder.ID, tempOrder.floor, tempOrder.direction);
				orderQueue[minCostIdx].status = 1;		//waiting
			}
		}
		usleep(100000);	
	}
	return 0;
}


