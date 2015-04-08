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

class Elevator{
	private:
		int				_elevatorReady;
		int 			_lastValidFloor;
		int 			_direction;							//0 - stall, (-1) - DOWN, 1 - UP
		int 			_currentOrderDirection;
		bool 			_stopEnabled;
		bool 			_floorStopSignal[N_FLOORS];
		ulong			_floorExtOrderID[N_FLOORS];
		//LocalServer _server;
		void 			floorReached();
		bool 			floorWithinRange(int floorNumber);
		int				nextStoppingFloor(int direction);
		pthread_attr_t 	threadAttribute;
		pthread_t 		elevatorMoving;
		pthread_t 		internalOrdersServing;
		pthread_mutex_t ordersMutex;
	public:
						Elevator();
						~Elevator();
		int 			cost2get2floor(int floorNumber, int direction);
		bool 			go2floor(ulong requestID, int floorNumber, int direction);
		//static void* startThread(void* pVoid);
		void 			updateElevator();
		void 			serveInternalOrders();
		void			clearButtonLamp(int floor, bool upwards);
};

#endif
