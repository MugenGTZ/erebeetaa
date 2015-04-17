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
	order 				myOrder;									//id, floor, direction
	netcard				origin;										//netcard origin machine
	netcard				dest;										//netcard serving machine
	unsigned int		cost;										//cost of order
	int					status; 									//0 - done 1 - waiting 2 - created
}extendedOrder;

typedef struct _udpPack{
	extendedOrder 	orderQueue[8];
	netcard			origin;									//id of the machine that created this pack
}udpPack;

volatile udpPack Pack01;
pthread_mutex_t udpPack01Mutex = PTHREAD_MUTEX_INITIALIZER;


bool(*_sendOrder2Elevator)(ulong, int, int);
int(*_getCost2Go2Floor)(int, int);
void(*_clearButtonLamp)(int, bool);

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
					if(tmpPack->orderQueue[i].myOrder.ID == Pack01.orderQueue[i].myOrder.ID){
						Pack01.orderQueue[i].dest = tmpPack->orderQueue[i].dest;
						Pack01.orderQueue[i].cost = tmpPack->orderQueue[i].cost;
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
}

void networkConnectionOrientedCallBack(chan extChan){
	Chanorder rcvOrder;
	ssize_t len;
	nErr error;
	printf("TCP server: Oh, Somebody has called me");
	if(NE_NO_ERROR != (error = chanRecv(extChan, (char*)&rcvOrder, &len, sizeof(Chanorder), 5))){
		displayErr(error); 
		closeChannel(extChan);		
	}
	else{
		pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
		printf("Taking order from outside.\n");	
		if(-1 == _getCost2Go2Floor(rcvOrder.floor, rcvOrder.direction)) rcvOrder.success = false;
		else{
			if(_sendOrder2Elevator(rcvOrder.ID, rcvOrder.floor, rcvOrder.direction))
				rcvOrder.success = true;
			else
				rcvOrder.success = false;
		}
		pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
		
		if(NE_NO_ERROR != (error = chanSend(extChan, (char*)&rcvOrder, sizeof(Chanorder)))) displayErr(error); 
		else printf("Successful sending of confirmation of order to the origin TCP node\n");
			
		closeChannel(extChan);
	}
}

/////////////////////////////////////////////////////////////////////////


void protocolInit(bool(*sendOrder2Elevator)(ulong, int, int), int(*getCost2Go2Floor)(int, int), void(*clearButtonLamp)(int, bool)){
	static int hasInitialize;
	pthread_t dispatcher;
	
	initNetwork();
	chanServerCallBack(networkConnectionOrientedCallBack);
	dataServerCallBack(networkConnectionlessCallBack);

	if(!hasInitialize){
		memset((void*)&Pack01, 0, sizeof(udpPack));			//initialize to 0
		srand(time(0));
		_sendOrder2Elevator = sendOrder2Elevator;
		_getCost2Go2Floor	= getCost2Go2Floor;
		_clearButtonLamp	= clearButtonLamp;
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
	pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
	if(Pack01.orderQueue[idx].status == 0){
		printf("the status is: %d\n",Pack01.orderQueue[idx].status);
		printf("New order (%lu) dir:%d\n", o.ID, o.direction);
		Pack01.orderQueue[idx].myOrder.ID 			= o.ID;
		Pack01.orderQueue[idx].myOrder.floor 		= o.floor;
		Pack01.orderQueue[idx].myOrder.direction 	= o.direction;
		Pack01.orderQueue[idx].origin 				= getMyNetCard();			//my netcard
		Pack01.orderQueue[idx].dest 				= INVALIDNETCARD;			//does not have a destination!
		Pack01.orderQueue[idx].status 				= 2;
	}
	pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
	return 0;
}

void printOrderStatus(){
	for(int i = 0; i < 8;i++) printf("Order[%d]: ID = %lu   status = %d\n", i, Pack01.orderQueue[i].myOrder.ID, Pack01.orderQueue[i].status);	
}

void internalOrderDone(ulong id){
	pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
	for(int i = 0; i < 8;i++){
		volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
		if(tempOrder->ID == id) {
			Pack01.orderQueue[i].status = 0;
			//send clear signal
			//printf("HW clear floor %d, dir %d", Pack01.orderQueue[i].myOrder.floor, Pack01.orderQueue[i].myOrder.direction);
			_clearButtonLamp(Pack01.orderQueue[i].myOrder.floor, Pack01.orderQueue[i].myOrder.direction == 1);
		}
	}
	printf("Called from internalOrderDone!\n");
	printOrderStatus();
	pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
}

void internalOrderPause(ulong id){
	pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
	for(int i = 0; i < 8;i++){
		volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
		if(tempOrder->ID == id) Pack01.orderQueue[i].status = 1;
	}
	printf("Called from internalOrderPause!\n");
	printOrderStatus();
	pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
}

void internalOrderResume(ulong id){
	pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
	for(int i = 0; i < 8;i++){
		volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
		if(tempOrder->ID == id) Pack01.orderQueue[i].status = 2;
	}
	printf("Called from internalOrderResume!\n");
	printOrderStatus();
	pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
}

static bool disableNetwork = false;
void* dispatchOrders(void *){
	while(1){
		//make Pack
		bool skip = true;
		pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
		//find our min cost
		for(int i = 0; i < 8; i++){
			//printf("%d,",Pack01.orderQueue[i].myOrder.ID);
			volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
			Pack01.orderQueue[i].dest = getMyNetCard();
			if(Pack01.orderQueue[i].status == 2){
				skip = false;
				Pack01.orderQueue[i].cost = _getCost2Go2Floor(tempOrder->floor, tempOrder->direction);
				//printf("Cost is: %d\n", Pack01.orderQueue[i].cost);
			}
			else
				Pack01.orderQueue[i].cost = -1;
		}
		pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
		
		if(skip) continue;
		if(disableNetwork == false){
			pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
				sendBroadcast((char*)&Pack01, sizeof(udpPack));
			pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
		}
		usleep(500000);
		
		//find the global min cost
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

		//Broadcast all orders over UDP
		//Find minimum Cost of all matrices
		if(minCost != (unsigned int)-1){
			//If minimum cost is outside make a thread to connect via TCP to remote peer
			pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
			netcard remoteNetcard = Pack01.orderQueue[minCostIdx].dest;
			Chanorder remoteOrder;
			
			remoteOrder.ID = Pack01.orderQueue[minCostIdx].myOrder.ID;
			remoteOrder.floor = Pack01.orderQueue[minCostIdx].myOrder.floor;
			remoteOrder.direction = Pack01.orderQueue[minCostIdx].myOrder.direction;
			remoteOrder.cod = COD_REQUEST;
			
			pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
			
			if(remoteNetcard != getMyNetCard() && remoteNetcard != INVALIDNETCARD){
				printf("Alien[%lu] will do this(%d):%d\n", Pack01.orderQueue[minCostIdx].dest, minCostIdx,minCost);
				chan channel = createChannel(remoteNetcard);															    
				if((int)channel < 0){
					disableNetwork = true;
					printf("Channel setup error. Closing the channel!\n"); 
					closeChannel(channel);
				}
				else{
					nErr error;
				
					if(NE_NO_ERROR != (error = chanSend(channel, (char*)&remoteOrder, sizeof(Chanorder)))){
						disableNetwork = true;
						displayErr(error); 
						closeChannel(channel); 
						//return;
					}
					else{
						printf("Successful sending, waiting for answer...\n");
						internalOrderPause(remoteOrder.ID);
						Chanorder rcvOrder;
						ssize_t len;
						if(NE_NO_ERROR != (error = chanRecv(channel, (char*)&rcvOrder, &len, sizeof(Chanorder), 5))){
							disableNetwork = true;
							displayErr(error); 
							closeChannel(channel);
							internalOrderResume(remoteOrder.ID);		
						}
						else{
							if(rcvOrder.success){
								internalOrderDone(remoteOrder.ID);
								printf("Order has been served by remote peer\n");
							}
							else{
								printf("*****************************************************************************\n");
								printf("Unsuccessful transaction!\n");							
								printf("*****************************************************************************\n");
								internalOrderResume(remoteOrder.ID);
							}
							if(remoteOrder.ID != rcvOrder.ID) printf("Order ID mismatch during TCP session\n");
							closeChannel(channel);
						}
					}
				}
			}
			//Else do the task
			else{
				disableNetwork = false;
				pthread_mutex_lock(&udpPack01Mutex);				//START OF CRITICAL REGION
				printf("this computer should serve this:%d!\n",minCost);	
				volatile order *tempOrder = &(Pack01.orderQueue[minCostIdx].myOrder);
				printf("{%d}",tempOrder->direction);
				if(_sendOrder2Elevator(tempOrder->ID, tempOrder->floor, tempOrder->direction)) Pack01.orderQueue[minCostIdx].status = 1;		//waiting
				pthread_mutex_unlock(&udpPack01Mutex);				//END OF CRITICAL REGION
			}
		}
		usleep(100000);	
	}
	return 0;
}


