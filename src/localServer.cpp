#include "localServer.h"
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
#include <semaphore.h>
#include <map>
#include "../../Networking/src/network.h"

#define PORT 20000													//port for UDP broadcast
#define PORT_lst 30000												//UDP listening port

typedef struct _extendedOrder{
	order 				myOrder;									//ID, floor, direction
	netcard				origin;										//netcard of origin machine
	netcard				dest;										//netcard of serving machine
	unsigned int		cost;										//cost of order
	int					status; 									//order status: 0 - done 1 - waiting 2 - created
}extendedOrder;

typedef struct _udpPack{
	extendedOrder 	orderQueue[8];
	netcard			origin;											//ID of the machine that created this pack
}udpPack;

volatile udpPack Pack01;
pthread_mutex_t udpPack01Mutex = PTHREAD_MUTEX_INITIALIZER;


bool(*_sendOrder2Elevator)(ulong, int, int);
int(*_getCost2Go2Floor)(int, int);
void(*_clearButtonLamp)(int, bool);

void* dispatchOrders(void *);
/////////////////////////////////////////////////////////////////////////////////////////////////////
//Callback functions: Connection oriented and Connectionless (We register these functions later on)
//Connectionless:
//Receives order cost from remote peer (Connectionless protocol).
//Compares received cost to our own and stores the destination netcard if its cost is less than ours 
/////////////////////////////////////////////////////////////////////////////////////////////////////
void networkConnectionlessCallBack(netcard num, char *data, int32_t len){
	if(len != sizeof(udpPack)) {
		printf("Size missmatch!\n");
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
				else printf("This should be served by this computer\n");
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

//If a remote peer knows that we can serve an order he will send it to us (with the connection oriented protocol); a thread is spawned per TCP client 
//Thread proccesses the order by calling the following procedure. We inform the peer if we can serve it or not (we might be unable to serve it if we received another order before its)
//Thread is blocked until it is signalled that order is done. If the remote peer does not get the acknowledgement in certain period, assumes order not done
//We close the channel after we are done

std::map<ulong, sem_t*> semaphoreMap;						//semaphores are referenced per order ID basis
void networkConnectionOrientedCallBack(chan extChan){
	Chanorder rcvOrder;
	ssize_t len;
	nErr error;
	printf("TCP server: Oh, Somebody has called me");
	if(NE_NO_ERROR != (error = chanRecv(extChan, (char*)&rcvOrder, &len, sizeof(Chanorder), 10))){
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
		
		if(rcvOrder.success){
			sem_t signalSem;
			sem_init(&signalSem, 0, 0);
			semaphoreMap[rcvOrder.ID] = &signalSem;
			printf("Waiting on semaphore with ID = %lu\n", rcvOrder.ID);
			sem_wait(&signalSem);							//calling thread is blocked until confirmation the order is done
			printf("Done waiting on semaphore with ID = %lu\n", rcvOrder.ID);
			sem_destroy(&signalSem);
		}
		
		if(NE_NO_ERROR != (error = chanSend(extChan, (char*)&rcvOrder, sizeof(Chanorder)))) displayErr(error); 
		else printf("Successful sending of confirmation of order to the origin TCP node\n");
			
		closeChannel(extChan);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////

//Set Callback functions
//If we have not initialize then initialize variables and set elevator callback functions.
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

//Called from main function when an input is detected (the local server gets the order)
//Updates the orderQueue struct with required information
int sendOrder(order o){
	int idx = o.floor*2+((o.direction==1)?1:0);
	pthread_mutex_lock(&udpPack01Mutex);										//START OF CRITICAL REGION
	if(Pack01.orderQueue[idx].status == 0){
		printf("The status is: %d\n",Pack01.orderQueue[idx].status);
		printf("New order (ID = %lu) with direction: %d\n", o.ID, o.direction);
		Pack01.orderQueue[idx].myOrder.ID 			= o.ID;
		Pack01.orderQueue[idx].myOrder.floor 		= o.floor;
		Pack01.orderQueue[idx].myOrder.direction 	= o.direction;
		Pack01.orderQueue[idx].origin 				= getMyNetCard();			//my netcard
		Pack01.orderQueue[idx].dest 				= INVALIDNETCARD;			//does not have a destination
		Pack01.orderQueue[idx].status 				= 2;
	}
	pthread_mutex_unlock(&udpPack01Mutex);										//END OF CRITICAL REGION
	return 0;
}

void printOrderStatus(){
	for(int i = 0; i < 8;i++) printf("Order[%d]: ID = %lu   status = %d\n", i, Pack01.orderQueue[i].myOrder.ID, Pack01.orderQueue[i].status);	
}

//Clears orders from this server (changes order's status to 0)
//Signals the corresponding order's semaphore
//Turns off the external floor lamp
void internalOrderDone(ulong id){
	pthread_mutex_lock(&udpPack01Mutex);											//START OF CRITICAL REGION
	if(semaphoreMap.find(id) != semaphoreMap.end()){
		printf("Signalling the semaphore with ID = %lu\n", id);
		sem_post(semaphoreMap[id]);
		semaphoreMap.erase(id);
	}										
	for(int i = 0; i < 8;i++){
		volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
		if(tempOrder->ID == id) {
			Pack01.orderQueue[i].status = 0;
			_clearButtonLamp(Pack01.orderQueue[i].myOrder.floor, Pack01.orderQueue[i].myOrder.direction == 1);
		}
	}
	
	printOrderStatus();
	pthread_mutex_unlock(&udpPack01Mutex);										//END OF CRITICAL REGION
}

//Pauses order when waiting for remote peer to report if the order will be served (changes order's status to 1)
void internalOrderPause(ulong id){
	pthread_mutex_lock(&udpPack01Mutex);										//START OF CRITICAL REGION
	for(int i = 0; i < 8;i++){
		volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
		if(tempOrder->ID == id) Pack01.orderQueue[i].status = 1;
	}
	
	printOrderStatus();
	pthread_mutex_unlock(&udpPack01Mutex);										//END OF CRITICAL REGION
}

//Resumes the order when remote peer fails to serve the order (changes order's status to 2)
void internalOrderResume(ulong id){
	pthread_mutex_lock(&udpPack01Mutex);										//START OF CRITICAL REGION
	for(int i = 0; i < 8;i++){
		volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
		if(tempOrder->ID == id) Pack01.orderQueue[i].status = 2;
	}
	
	printOrderStatus();
	pthread_mutex_unlock(&udpPack01Mutex);										//END OF CRITICAL REGION
}

typedef struct _threadParam{									
	volatile int*	status;
	sem_t* 	semaphore;
}threadParam;

//When we decide to serve the order locally, a thread is spawned which waits certain amount of time on the semaphore 
//which should be signalled from internalOrderDone; In case of timeout, order is again in status "pending" and can be sent outside
void* internalOrderDoneFun(void *param){
	timespec timeoutTime;
	threadParam * funcParam = (threadParam*) param;	
	
	if (clock_gettime(CLOCK_REALTIME, &timeoutTime) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
	timeoutTime.tv_sec += 10;
	
	sem_t* orderSem = funcParam->semaphore;
	printf("Entering timedwait! \n");

	if(sem_timedwait(orderSem, &timeoutTime) == -1){
		printf("Leaving timedwait! \n");
		*(funcParam->status) = 2;											//order is "pending" now
		sem_wait(orderSem);
	}
	
	sem_destroy(orderSem);
	free(funcParam);
	return NULL;
}

static bool _disableNetwork = false;

//Find the cost of pending orders for this machine; if none, waits until it recieves orders
//Send a request (via connectionless protocol) in order to get costs from other peers 
//Wait for 0.5s to receive costs from all peers (in connectionless callback)
//Find the minimum cost of all orders
//If an order can be served continue, else go from beginning
//If the minimum cost is from a remote peer, make a thread to connect via connection oriented protocol to that peer
//Else serve the order locally
//In case of any errors during the communication with the remote peer (via connection oriented protocol) =>
//In the next iteration, we try to serve the order locally before attempting to search for remote peer again (using _disableNetwork var)
 
void* dispatchOrders(void *){
	while(1){
		
		bool skip = true;														
		pthread_mutex_lock(&udpPack01Mutex);									//START OF CRITICAL REGION
		
		for(int i = 0; i < 8; i++){		
			volatile order *tempOrder = &(Pack01.orderQueue[i].myOrder);
			Pack01.orderQueue[i].dest = getMyNetCard();
			if(Pack01.orderQueue[i].status == 2){
				skip = false;
				Pack01.orderQueue[i].cost = _getCost2Go2Floor(tempOrder->floor, tempOrder->direction);
			}
			else
				Pack01.orderQueue[i].cost = -1;
		}
		pthread_mutex_unlock(&udpPack01Mutex);									//END OF CRITICAL REGION
		
		if(skip) continue;														//if no pending orders, skip the rest of the code
		
		if(_disableNetwork == false){
			pthread_mutex_lock(&udpPack01Mutex);								//START OF CRITICAL REGION
				sendBroadcast((char*)&Pack01, sizeof(udpPack));					
			pthread_mutex_unlock(&udpPack01Mutex);								//END OF CRITICAL REGION
		}
		usleep(500000);
		
		//Find the minimum cost of all orders
		unsigned int minCost = -1;
		int minCostIdx = -1;
		pthread_mutex_lock(&udpPack01Mutex);									//START OF CRITICAL REGION
		for(int i = 0; i < 8; i++){
			unsigned int cost = Pack01.orderQueue[i].cost;
	
			if(Pack01.orderQueue[i].status == 2){
				if(cost < minCost){
					minCost = cost;
					minCostIdx = i;
				}
			}
		}
		pthread_mutex_unlock(&udpPack01Mutex);									//END OF CRITICAL REGION

		if(minCost != (unsigned int)-1){
			
			pthread_mutex_lock(&udpPack01Mutex);								//START OF CRITICAL REGION
			netcard remoteNetcard = Pack01.orderQueue[minCostIdx].dest;
			Chanorder remoteOrder;
			
			remoteOrder.ID = Pack01.orderQueue[minCostIdx].myOrder.ID;
			remoteOrder.floor = Pack01.orderQueue[minCostIdx].myOrder.floor;
			remoteOrder.direction = Pack01.orderQueue[minCostIdx].myOrder.direction;
			remoteOrder.cod = COD_REQUEST;
			
			pthread_mutex_unlock(&udpPack01Mutex);								//END OF CRITICAL REGION
			
			if(remoteNetcard != getMyNetCard() && remoteNetcard != INVALIDNETCARD){
				printf("Remote peer [netcard = %lu] should do the order!\n", Pack01.orderQueue[minCostIdx].dest);
				chan channel = createChannel(remoteNetcard);															    
				if((int)channel < 0){
					_disableNetwork = true;
					printf("Channel setup error. Closing the channel!\n"); 
					closeChannel(channel);
				}
				else{
					nErr error;
				
					if(NE_NO_ERROR != (error = chanSend(channel, (char*)&remoteOrder, sizeof(Chanorder)))){
						_disableNetwork = true;
						displayErr(error); 
						closeChannel(channel); 
					}
					else{
						printf("Successful sending of request. Waiting for answer...\n");
						internalOrderPause(remoteOrder.ID);
						Chanorder rcvOrder;
						ssize_t len;
						if(NE_NO_ERROR != (error = chanRecv(channel, (char*)&rcvOrder, &len, sizeof(Chanorder), 10))){
							_disableNetwork = true;
							displayErr(error); 
							closeChannel(channel);
							internalOrderResume(remoteOrder.ID);		
						}
						else{
							if(rcvOrder.success){
								internalOrderDone(remoteOrder.ID);
								printf("Order has been served by remote peer.\n");
							}
							else{
								printf("*****************************************************************************\n");
								printf("						Unsuccessful transaction!\n");							
								printf("*****************************************************************************\n");
								internalOrderResume(remoteOrder.ID);
							}
							if(remoteOrder.ID != rcvOrder.ID) printf("Order ID mismatch during TCP session.\n");
							closeChannel(channel);
						}
					}
				}
			}
			//Serve the order locally
			else{
				bool hasServedOrder = false;
				_disableNetwork = false;
				pthread_mutex_lock(&udpPack01Mutex);															//START OF CRITICAL REGION
				printf("This computer should serve this order.\n");	
				volatile order *tempOrder = &(Pack01.orderQueue[minCostIdx].myOrder);
				
				if(_sendOrder2Elevator(tempOrder->ID, tempOrder->floor, tempOrder->direction)){
					hasServedOrder = true;
					semaphoreMap[(ulong)(tempOrder->ID)] = (sem_t*)malloc(sizeof(sem_t));						//creating a semaphore for each order
					sem_init(semaphoreMap[(ulong)(tempOrder->ID)], 0, 0);										//initialize the semaphore
					Pack01.orderQueue[minCostIdx].status = 1;		
				}
				pthread_mutex_unlock(&udpPack01Mutex);															//END OF CRITICAL REGION
				
				threadParam * param = (threadParam*)malloc(sizeof(threadParam));															
				param->semaphore = semaphoreMap[(ulong)(tempOrder->ID)];
				param->status = &(Pack01.orderQueue[minCostIdx].status);
				
				if(hasServedOrder){																				//spawn a thread which waits for order to be served locally
					pthread_t internalOrderDoneWaitThread;
					if(pthread_create(&internalOrderDoneWaitThread, NULL, internalOrderDoneFun, (void*) param)){
						fprintf(stderr, "Error creating thread\n");
						exit (EXIT_FAILURE);
					}
				}
				
			}
		}
		else _disableNetwork = false;
		usleep(100000);	
	}
	return 0;
}


