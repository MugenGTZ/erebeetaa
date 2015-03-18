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

volatile udpPack Pack01;
pthread_mutex_t udpPack01Mutex = PTHREAD_MUTEX_INITIALIZER;


bool(*_sendOrder2Elevator)(ulong, int, int);
int(*_getCost2Go2Floor)(int, int);

void* dispatchOrders(void *);
/////////////////////////////////////////////////////////////////////////
void networkConnectionlessCallBack(netcard num, char *data, int32_t len){
	if(len != sizeof(udpPack)) {
		printf("size (%d,%d)missmatch!\n",len,(int)sizeof(udpPack));
		return;
	}
	udpPack *tmpPack = (udpPack*)data;

	if( getMyNetCard() == tmpPack->origin){
		printf("finding cost (my order)\n");
		//greedy algorithm	
		pthread_mutex_lock(&udpPack01Mutex);		//START OF CRITICAL REGION
		for(int i = 0; i < 8; i++){
			if(tmpPack->orderQueue[i].status == 2){
				printf("Alien Cost(%d) = %d and our cost is: %d\n", i, tmpPack->orderQueue[i].cost, Pack01.orderQueue[i].cost);
				if(tmpPack->orderQueue[i].cost < Pack01.orderQueue[i].cost){
						printf("--(%lu,%lu)\n",tmpPack->orderQueue[i].myOrder.ID, Pack01.orderQueue[i].myOrder.ID);
					if(tmpPack->orderQueue[i].myOrder.ID == Pack01.orderQueue[i].myOrder.ID){
						printf("-Allien\n");
						//printf("Alien Cost(%d) = %d\n", i, tmpPack->orderQueue[i].cost);
						Pack01.orderQueue[i].dest = tmpPack->orderQueue[i].dest;
						Pack01.orderQueue[i].cost = tmpPack->orderQueue[i].cost;
						//printf("Pack01 from callback: %d ", Pack01.orderQueue[i].cost);
						//tmpPack->orderQueue[i].cost = Pack01.orderQueue[i].cost;
					}
				}
				else printf("-ME\n");
			}
		}		
		pthread_mutex_unlock(&udpPack01Mutex);		//END OF CRITICAL REGION
	}
	else{
		printf("finding cost (not my order)\n");
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
			sendData(tmpPack->origin, (char*)tmpPack, sizeof(udpPack));
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
	printf("New order (%lu)\n", o.ID);
	int idx = o.floor*2+((o.direction==1)?1:0);
	pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
	Pack01.orderQueue[idx].myOrder.ID 			= o.ID;
	Pack01.orderQueue[idx].myOrder.floor 		= o.floor;
	Pack01.orderQueue[idx].myOrder.direction 	= o.direction;
	Pack01.orderQueue[idx].origin 				= getMyNetCard();
	Pack01.orderQueue[idx].dest 				= (netcard)-1;
	Pack01.orderQueue[idx].status 				= 2;
	pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
	return 0;
}


void internalOrderDone(ulong id){
	pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
	for(int i = 0; i < 8;i++){
		volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
		if(tempOrder->ID == id) Pack01.orderQueue[i].status = 0;
	}
	pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
}


void* dispatchOrders(void *){
	while(1){
		//make Pack
		bool skip = true;
		pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
		for(int i = 0; i < 8; i++){
			//printf("%d,",Pack01.orderQueue[i].myOrder.ID);
			volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
			Pack01.orderQueue[i].dest = getMyNetCard();
			if(Pack01.orderQueue[i].status == 2){
				skip = false;
				Pack01.orderQueue[i].cost = _getCost2Go2Floor(tempOrder->floor, tempOrder->direction);
			}
			else
				Pack01.orderQueue[i].cost = -1;
		}
		pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
		
		if(skip) continue;
		pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
		sendBroadcast((char*)&Pack01, sizeof(udpPack));
		pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
		sleep(2);
		
		//find the min cost
		unsigned int minCost = -1;
		int minCostIdx = -1;
		pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
		for(int i = 0; i < 8; i++){
			unsigned int cost = Pack01.orderQueue[i].cost;
			//printf("Pack01 contains: %d ", cost);
			if(Pack01.orderQueue[i].status == 2){
				if(cost < minCost){
					minCost = cost;
					minCostIdx = i;
				}
			}
		}
		pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
		
		//sendData(NETCARDBROADCAST, (char*)"Hello Filip!", 13);
		//Broadcast all orders over UDP
		//Find minimum Cost of all matrices
		
		if(minCost != (unsigned int)-1){
			//If minimum cost is outside make a thread to connect via TCP to remote peer
			pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
			if(Pack01.orderQueue[minCostIdx].dest != getMyNetCard()){
				printf("Alien will do this(%d):%d\n",minCostIdx,minCost);
			}
			//Else do the task
			else{
				printf("this computer should serve this:%d!\n",minCost);	
				volatile order *tempOrder = &(Pack01.orderQueue[minCostIdx].myOrder);
				_sendOrder2Elevator(tempOrder->ID, tempOrder->floor, tempOrder->direction);
				Pack01.orderQueue[minCostIdx].status = 1;		//waiting
			}
			pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
		}
		usleep(100000);	
	}
	return 0;
}


