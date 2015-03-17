#include "Door.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "HW/elev.h"
#include <stdlib.h>

int Door::state = 0;				//0 - closed; 1 - open/waiting/closing;

void* updateStatus1(void* a){
	Door::updateStatus();
	return 0;
}

void Door::updateStatus(void){
	//int i;
	
	sleep(1);

	while(1){
		if(elev_get_obstruction_signal()) usleep(100000);
		else break;
	}
	
	state = 0;
	elev_set_door_open_lamp(0);	
	
	return;	
}

void Door::openAndCloseDoor(){

	if(state == 1) return;

	pthread_t stateUpdate;

	state = 1;
	elev_set_door_open_lamp(1);
	
	if(pthread_create(&stateUpdate, NULL, updateStatus1, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
	
	pthread_join(stateUpdate, NULL);
	
}

	 


