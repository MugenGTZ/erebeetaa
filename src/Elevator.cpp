#include "Elevator.h"
#include "HW/elev.h"
#include "toolbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "elevatorProtocol/elevatorProtocol.h"

Elevator *myOnlyElevator;

int cost2get2floorFun(int floorNumber, int direction){										
	if(myOnlyElevator) return myOnlyElevator->cost2get2floor(floorNumber, direction);
	else return -1;
}

bool go2floorFun(ulong requestID, int floorNumber, int direction){
	if(myOnlyElevator) return myOnlyElevator->go2floor(requestID, floorNumber, direction);
	else return false;
}

void clearButtonLampFun(int floor, bool upwards){
	if(myOnlyElevator) return myOnlyElevator->clearButtonLamp(floor, upwards);
	else return;
}


void* makeElevatorThread(void* elevator) {
	((Elevator*)elevator)->updateElevatorState();
	printf("Exit!\n");
	return 0;
}


void* makeInternalOrdersThread(void* elevator) {
	((Elevator*)elevator)->serveInternalOrders();
	printf("Exit!\n");
	return 0;
}


/*//////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//This function moves elevator to server orders  and keeps track of its internal state
//Elevator is moving until it reaches a floor (determined by reading the floor sensor)
//It stops if there is an order for that floo
////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////*/
void Elevator::updateElevatorState(){														
	while(1){
		if((_stopEnabled = elev_get_stop_signal())){
	 		elev_set_motor_direction(DIRN_STOP);
			elev_set_stop_lamp(1);
		}
		else{
			elev_set_stop_lamp(0);
			elev_set_motor_direction((elev_motor_direction_t)_direction);
			int curFloor = elev_get_floor_sensor_signal();
			if(curFloor != -1)	hasArrivedToFloor(curFloor);
		}
	}
}

void Elevator::hasArrivedToFloor(int curFloor){
	_lastValidFloor = curFloor;
	elev_set_floor_indicator(_lastValidFloor);
	if(_floorStopSignal[_lastValidFloor])		arrivedToFloorWithOrder();
	else{
		if(_direction == 0)						arrivedToFloorWithNoOrder();							//elevator is stopped?
		else if(nextStoppingFloor(_direction) == -1){													//elevator is moving in direction where no orders exist 	
			printf("Last floor and direction: %d %d\n",_lastValidFloor, _direction);
			printf("Invalid condition, you are moving to no floor\n");				
			elev_set_motor_direction(DIRN_STOP);
			exit(1);
		}
	}
}

void Elevator::arrivedToFloorWithNoOrder(){
	int nextFloorUp = nextStoppingFloor(1);																//orders in UP direction?
	int nextFloorDown = nextStoppingFloor(-1);															//orders in DOWN direction?
	if((nextFloorUp >= 0) || (nextFloorDown>= 0)){
		if(nextFloorUp >= 0 && nextFloorDown>= 0)
			_direction = (nextFloorUp-_lastValidFloor > _lastValidFloor - nextFloorDown) ? 1 : -1;		//go to the nearest floor
		else 
			_direction = (nextFloorUp >= 0) ? 1 : -1;	
	}
	elev_set_motor_direction((elev_motor_direction_t)_direction);
}

void Elevator::arrivedToFloorWithOrder(){
	elev_set_motor_direction(DIRN_STOP);																//stop the motor because we reached the desired floor

	if(_floorExtOrderID[_lastValidFloor] != 0){															//clear the order
		if(_currentOrderDirection == 1) elev_set_button_lamp(BUTTON_CALL_UP, _lastValidFloor ,0);		//turn off the corresponding lamp
		else elev_set_button_lamp(BUTTON_CALL_DOWN, _lastValidFloor ,0);	
		internalOrderDone(_floorExtOrderID[_lastValidFloor]);											//notify the protocol about the completion of the order
		_floorExtOrderID[_lastValidFloor] = 0;															//clear external orders for the floor
	}

	pthread_mutex_lock(&_ordersMutex);				
	_floorStopSignal[_lastValidFloor] = false;															//mark floor as served 
	pthread_mutex_unlock(&_ordersMutex);

	if(nextStoppingFloor(_direction) == -1) 
		_direction = _currentOrderDirection;															//restoring the direction of the moving after serving an internal order
		
	Door::openAndCloseDoor();																			//open door and wait to let passangers in/out	

	if(nextStoppingFloor(_direction) == -1){															//no more requests in current direction?
		_direction = (nextStoppingFloor(-_direction) == -1) ? 0 : -_direction;							//stop or change direction(in case of other requests)
		_currentOrderDirection = _direction;					
	}	
}

/*//////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////*/

void Elevator::serveInternalOrders(){
	while(1){
		for(int i=0; i<N_FLOORS; i++){
			pthread_mutex_lock(&_ordersMutex);
			_floorStopSignal[i] = _floorStopSignal[i] || elev_get_button_signal(BUTTON_COMMAND, i); //taking internal orders
			elev_set_button_lamp(BUTTON_COMMAND, i, _floorStopSignal[i]);
			pthread_mutex_unlock(&_ordersMutex);
		}	
	}
}




int Elevator::nextStoppingFloor(int direction){
	if(direction == 0) return -1;
	int nextFloor = _lastValidFloor;
	while(nextFloor >= 0 && nextFloor < N_FLOORS){
		if(_floorStopSignal[nextFloor]) return nextFloor;
		nextFloor += direction;
	}
	return -1;
}

//determing if requested floor is in the interval: [last_valid_floor, last_floor_in_current_direction]
bool Elevator::floorWithinRange(int floorNumber){						
	int nextFloor = _lastValidFloor + _direction;
	bool passesThroughFloor = false;
	while(nextFloor >= 0 && nextFloor < N_FLOORS){
		if(nextFloor == floorNumber) passesThroughFloor = true;
		if(_floorStopSignal[nextFloor] && passesThroughFloor) return true;
		nextFloor += _direction;
	}
	return false;
}


Elevator::Elevator(){
	_elevatorReady = -1;
	myOnlyElevator = this;
	protocolInit(go2floorFun, cost2get2floorFun, clearButtonLampFun);
	 if (!elev_init()){
        printf("Unable to initialize elevator hardware!\n");
        exit(EXIT_FAILURE);
    }

	int curFloor = elev_get_floor_sensor_signal();
	_lastValidFloor = curFloor;
	_direction = 0;
	_currentOrderDirection = 0;
	_stopEnabled = false;
	for(int i=0; i<N_FLOORS;i++) _floorStopSignal[i] = false;
	for(int i=0; i<N_FLOORS;i++) _floorExtOrderID[i] = 0;
	
//initial positioning to a valid floor	
	if(_lastValidFloor == -1){
		elev_set_motor_direction(DIRN_DOWN);
		int nTries = 0;
		while((_lastValidFloor = elev_get_floor_sensor_signal()) == -1){
			//you have 10 seconds to get the elevator unstuck!
			if(10000 == ++nTries){
				printf("Starting: Motor Panel only mode\n");
				elev_set_motor_direction(DIRN_STOP);
				return;
			}
			//for 2 seconds the elevator will try to unstuck itself by moving downwards
			if(nTries > 2000){
				elev_set_motor_direction(DIRN_STOP);
			}
			//every second there will be this message
			if(0 == (nTries & 0x03FF)) printf("the elevator is stucked! Please check!\n");
			usleep(1000);
		}
		elev_set_motor_direction(DIRN_STOP);
	}
	printf("Valid floor reached during init phase. LVF = %d\n", _lastValidFloor);
	
	int tmpLVF;
	if(0 == _lastValidFloor){
		tmpLVF = 1;
		elev_set_motor_direction(DIRN_UP);
		int nTries = 0;
		while((_lastValidFloor = elev_get_floor_sensor_signal()) != 1){
			if(5000 == ++nTries){
				printf("Motor is stucked!\nStarting: Motor Panel only mode\n");
				elev_set_motor_direction(DIRN_STOP);
				return;
			}
			usleep(1000);	
		}
	}
	else if(N_FLOORS - 1 == _lastValidFloor){
		tmpLVF = N_FLOORS - 2;
		elev_set_motor_direction(DIRN_DOWN);	
		int nTries = 0;
		while((_lastValidFloor = elev_get_floor_sensor_signal()) != N_FLOORS - 2){
			if(5000 == ++nTries){
				printf("Motor is stucked!\nStarting: Motor Panel only mode\n");
				elev_set_motor_direction(DIRN_STOP);
				return;
			}
			usleep(1000);	
		}
	}
	
	_lastValidFloor = tmpLVF;

	pthread_mutex_init(&_ordersMutex, NULL);

	if(pthread_create(&_elevatorMotionThread, NULL, makeElevatorThread, (void*)this)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}

	if(pthread_create(&_internalOrdersServerThread, NULL, makeInternalOrdersThread, (void*)this)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
	_elevatorReady = 1;
	printf("Starting: The Elevator has started\n");
}

Elevator::~Elevator(){
}

//This function returns the cost for the Elevator to get to a given floor
//If the floor is in the interval of the elevator and has the same direction as the order, the cost = Difference in floors
//else cost = -1;
//If the elevator is standing still then the cost = Difference in floors + total floors(lower the priority).
int Elevator::cost2get2floor(int floorNumber, int direction){

	if(_stopEnabled) return -1;
	if(_elevatorReady == -1) return -1;
	
	if(_direction == 0) return (floorNumber == _lastValidFloor) ? 0 : ABS(floorNumber - _lastValidFloor) + N_FLOORS;
	else{
		if(_direction != direction) return -1;
		if((_currentOrderDirection != 0) && (_currentOrderDirection != direction)) return -1;
		if(!floorWithinRange(floorNumber)) return -1;
		return ABS(floorNumber - _lastValidFloor);
	}
}

//This fuction is called from the protocol for external commands
bool Elevator::go2floor(ulong requestID, int floorNumber, int direction){
	printf("Serving order: %lu\n",requestID);
	
	if(_floorExtOrderID[floorNumber]) 					return false;
	if(cost2get2floor(floorNumber, direction) == -1) 	return false;									//If moving & floor not in interval, dont take order
	pthread_mutex_lock(&_ordersMutex);
	_floorStopSignal[floorNumber] = true;																//set before _direction because we are locking!
	_direction = (floorNumber == _lastValidFloor) ? 0 : ((floorNumber > _lastValidFloor)? 1 : -1);		//case when elevator stands still is covered
	pthread_mutex_unlock(&_ordersMutex);
	_currentOrderDirection = direction;

	_floorExtOrderID[floorNumber] = requestID;
	return true;	
}

void Elevator::clearButtonLamp(int floor, bool upwards){
	printf("HW clear floor %d, dir %d", floor, upwards);
	if(upwards) elev_set_button_lamp(BUTTON_CALL_UP, floor, 0);
	else	 	elev_set_button_lamp(BUTTON_CALL_DOWN, floor, 0);
}

