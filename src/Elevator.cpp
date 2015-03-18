#include "Elevator.h"
#include "HW/elev.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "elevatorProtocol/elevatorProtocol.h"

//going against the rules
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


#define ABS(a) ({ __typeof__ (a) _a = (a); _a > 0 ? _a : -_a; })
/*
int main(){
	Elevator elev;
	int floorNum = 1, direction = -1, cost2go;
	while(1){
		printf("Enter floor number and direction: ");
		scanf("%d %d",&floorNum, &direction);
		cost2go = elev.cost2get2floor(floorNum, direction);
		printf("Cost: %d\n", cost2go);
		if(cost2go != -1) elev.go2floor(1, floorNum, direction);
		//usleep(5);
	}
	return 0;
}
*/
void* makeElevatorThread(void* elevator) {
	((Elevator*)elevator)->updateElevator();
	printf("Exit!\n");
	return 0;
}


void* makeInternalOrdersThread(void* elevator) {
	((Elevator*)elevator)->serveInternalOrders();
	printf("Exit!\n");
	return 0;
}


int cc = 0;
void Elevator::updateElevator(){
	while(1){
		//usleep(1000);
		if((_stopEnabled = elev_get_stop_signal())){
	 		elev_set_motor_direction(DIRN_STOP);
			elev_set_stop_lamp(1);
		}
		else{
			elev_set_stop_lamp(0);
			elev_set_motor_direction((elev_motor_direction_t)_direction);
			int curFloor = elev_get_floor_sensor_signal();
			if(curFloor != -1){
				_lastValidFloor = curFloor;
				elev_set_floor_indicator(_lastValidFloor);
				if(_floorStopSignal[_lastValidFloor]){						
					elev_set_motor_direction(DIRN_STOP);										//stop the motor because we reached the desired floor

					if(_floorExtOrderID[_lastValidFloor] != 0){
						if(_currentOrderDirection == 1) elev_set_button_lamp(BUTTON_CALL_UP, _lastValidFloor ,0);
						else elev_set_button_lamp(BUTTON_CALL_DOWN, _lastValidFloor ,0);	
						internalOrderDone(_floorExtOrderID[_lastValidFloor]);	
						_floorExtOrderID[_lastValidFloor] = 0;
					}

					pthread_mutex_lock(&ordersMutex);				
					_floorStopSignal[_lastValidFloor] = false;
					pthread_mutex_unlock(&ordersMutex);

					if(nextStoppingFloor(_direction) == -1) 
						_direction = _currentOrderDirection;
					Door::openAndCloseDoor();													//open door and wait to let passangers in/out	

					if(nextStoppingFloor(_direction) == -1){									//no more requests in current direction?
						_direction = (nextStoppingFloor(-_direction) == -1) ? 0 : -_direction;	//stop or change direction(in case of other requests)
						_currentOrderDirection = _direction;					
					}		
				}
				else{
					if(_direction == 0){														//elevator is stopped?
						int nextFloorUp = nextStoppingFloor(1);									//orders in UP direction?
						int nextFloorDown = nextStoppingFloor(-1);								//orders in DOWN direction?
						if((nextFloorUp >= 0) || (nextFloorDown>= 0)){
							if(nextFloorUp >= 0 && nextFloorDown>= 0)
								_direction = (nextFloorUp-_lastValidFloor > _lastValidFloor - nextFloorDown) ? 1 : -1;	//go to the nearest floor
							else 
								_direction = (nextFloorUp >= 0) ? 1 : -1;	
						}
						elev_set_motor_direction((elev_motor_direction_t)_direction);
					}
					else if(nextStoppingFloor(_direction) == -1){								//elevator is moving in direction where no orders exist 	
						printf("Last floor and direction: %d %d\n",_lastValidFloor, _direction);
						printf("Invalid condition, you are moving to no floor\n");				
						elev_set_motor_direction(DIRN_STOP);
						exit(1);
					}
				}
			}
		}
	}
}

void Elevator::serveInternalOrders(){
	while(1){
		for(int i=0; i<N_FLOORS; i++){
			pthread_mutex_lock(&ordersMutex);
			_floorStopSignal[i] = _floorStopSignal[i] || elev_get_button_signal(BUTTON_COMMAND, i); //taking internal orders
			elev_set_button_lamp(BUTTON_COMMAND, i, _floorStopSignal[i]);
			pthread_mutex_unlock(&ordersMutex);
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
		while((_lastValidFloor = elev_get_floor_sensor_signal()) == -1);
		elev_set_motor_direction(DIRN_STOP);
	}
	
	pthread_mutex_init(&ordersMutex, NULL);

	if(pthread_create(&elevatorMoving, NULL, makeElevatorThread, (void*)this)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}

	if(pthread_create(&internalOrdersServing, NULL, makeInternalOrdersThread, (void*)this)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
}

Elevator::~Elevator(){
}

//This function returns the cost for the Elevator to get to a given floor
//If the floor is in the interval of the elevator and has the same direction as the order, the cost = Difference in floors
//else cost = -1;
//If the elevator is standing still then the cost = Difference in floors + total floors(lower the priority).
int Elevator::cost2get2floor(int floorNumber, int direction){
//	int curFloor = elev_get_floor_sensor_signal();
	if(_stopEnabled) return -1;
	printf("LVF: %d\n", _currentOrderDirection);
	if(_direction == 0) return (floorNumber == _lastValidFloor) ? 0 : ABS(floorNumber - _lastValidFloor) + N_FLOORS;
	else{
		if(_direction != direction) return -1;
		if((_currentOrderDirection != 0) && (_currentOrderDirection != direction)) return -1;
		if(!floorWithinRange(floorNumber)) return -1;
		return ABS(floorNumber - _lastValidFloor);
	}
}

//This fuction is called from the Local server for external commands
bool Elevator::go2floor(ulong requestID, int floorNumber, int direction){
	if(cost2get2floor(floorNumber, direction) == -1) return false;										//If moving & floor not in interval, dont take order
	pthread_mutex_lock(&ordersMutex);
	_floorStopSignal[floorNumber] = true;																//set before _direction because we are locking!
	_direction = (floorNumber == _lastValidFloor) ? 0 : ((floorNumber > _lastValidFloor)? 1 : -1);		//case when elevator stands still is covered
	pthread_mutex_unlock(&ordersMutex);
	_currentOrderDirection = direction;

	_floorExtOrderID[floorNumber] = requestID;
	return true;	
}

void Elevator::clearButtonLamp(int floor, bool upwards){
	printf("HW clear floor %d, dir %d", floor, upwards);
	if(upwards) elev_set_button_lamp(BUTTON_CALL_UP, floor, 0);
	else	 	elev_set_button_lamp(BUTTON_CALL_DOWN, floor, 0);
}

