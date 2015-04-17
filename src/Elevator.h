#ifndef _ELEVATOR_H
#define _ELEVATOR_H

#include "Door.h"
#include "HW/elev.h"
#include "HW/channels.h"
#include <pthread.h>
#include <limits.h>

#ifndef ULONGDEFINED
#define ULONGDEFINED 1
typedef long unsigned int ulong;
#endif

//Class Elevator is in charge of:
//1. calculating the cost to get to the certain floor 
//2. Hardware
//		a. moving the elevator (estates)
// 		b. controlling the lamps on the control panel
//3. Keeping track of internal orders (orders inside the elevator)
//4. Serving one external order at a time (from the local server)

class Elevator{
	private:
		//State variables
		int				_elevatorReady;												//Is elevator ready for operation?
		int 			_lastValidFloor;										
		int 			_direction;													//0 - stall, (-1) - DOWN, 1 - UP
		int 			_currentOrderDirection;										//similar to _direction, but keeps track of the _direction even if the elevator stops to serve someone
		bool 			_stopEnabled;										
		bool 			_floorStopSignal[N_FLOORS];									//Indicator of existance of an order at a certain floor	
		ulong			_floorExtOrderID[N_FLOORS];									//The ID of an external order at a certain floor
		ulong			_floorExtOrderDir[N_FLOORS];
		
		//Helper functions										
		bool 			floorWithinRange(int floorNumber);							//Checks if a floor is within our current floor and the last floor (that has been requested) in our current direction 
		int				nextStoppingFloor(int direction);							//Finds the nearest floor (that has been requested) in our current direction
		void			hasArrivedToFloor(int number);
		void			serveOrder();
		void			respondToNewOrders();
		void			directElevatorToNewOrder();
		
		pthread_t 		_elevatorMotionThread;										//Thread which controls the elevator's motion
		pthread_t 		_internalOrdersServerThread;					
		pthread_mutex_t _ordersMutex;											
	public:
						Elevator();
						~Elevator();
		int 			cost2get2floor(int floorNumber, int direction);			
		bool 			go2floor(ulong requestID, int floorNumber, int direction);	//Request elevator to go to the floor (external order)
		
		void 			updateElevatorState();																			
		void 			serveInternalOrders();
		//Helper function
		void			clearButtonLamp(int floor, bool upwards);					//Special case when the local server sends the order to the network and another elevator serves it 
																					//thus we must clear it because the local server keeps track of pending orders
};

#endif
