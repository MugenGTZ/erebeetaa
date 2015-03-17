#include "../Elevator.h"
#include "../HW/elev.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../elevatorProtocol/elevatorProtocol.h"


int main(){	
	Elevator elev;
//	int floorNum = 1, direction = -1, cost2go;
//	int data[N_FLOORS][2];										//column 1 = UP; column 2 = DOWN;

	while(1){

		if(elev_get_button_signal(BUTTON_CALL_UP, 0)){
			order newOrder;
			newOrder.ID 			= (ulong)(rand());
			newOrder.floor			= 0;
			newOrder.direction		= 1;
			sendOrder(newOrder);
 			//elev.go2floor(1, 0, 1);
			elev_set_button_lamp(BUTTON_CALL_UP, 0 ,1);
		}
		//else elev_set_button_lamp(BUTTON_CALL_UP, 0 ,0);
	
		for(int i=1; i<N_FLOORS - 1; i++){
			if(elev_get_button_signal(BUTTON_CALL_UP, i) ){
				order newOrder;
				newOrder.ID 			= (ulong)(rand());
				newOrder.floor			= i;
				newOrder.direction		= 1;
				sendOrder(newOrder);
				//elev.go2floor(1, i, 1);
				elev_set_button_lamp(BUTTON_CALL_UP, i ,1);	
			}
			//else elev_set_button_lamp(BUTTON_CALL_UP, i, 0);
	
			if(elev_get_button_signal(BUTTON_CALL_DOWN, i) ){
				order newOrder;
				newOrder.ID 			= (ulong)(rand());
				newOrder.floor			= i;
				newOrder.direction		= -1;
				sendOrder(newOrder);
 				//elev.go2floor(1, i, -1);
				elev_set_button_lamp(BUTTON_CALL_DOWN, i ,1);
			}	
		//	else elev_set_button_lamp(BUTTON_CALL_DOWN, i ,0);	
		}
		
		if(elev_get_button_signal(BUTTON_CALL_DOWN, N_FLOORS - 1)){
			order newOrder;
 			newOrder.ID 			= (ulong)(rand());
			newOrder.floor			= N_FLOORS - 1;
			newOrder.direction		= -1;
			sendOrder(newOrder);
			//elev.go2floor(1, N_FLOORS - 1, -1);			
			elev_set_button_lamp(BUTTON_CALL_DOWN, N_FLOORS - 1 ,1);
		}
		//else elev_set_button_lamp(BUTTON_CALL_DOWN, N_FLOORS - 1 ,0);
		
		/*
		printf("Enter floor number and direction: ");
		scanf("%d %d",&floorNum, &direction);
		cost2go = elev.cost2get2floor(floorNum, direction);
		printf("Cost: %d\n", cost2go);
		if(cost2go != -1) elev.go2floor(1, floorNum, direction);
		//usleep(5);*/
	}
	
	return 0;
}
