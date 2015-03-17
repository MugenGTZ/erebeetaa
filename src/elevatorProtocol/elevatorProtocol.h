#ifndef _ELEVATOR_PROTOCOL_H
#define _ELEVATOR_PROTOCOL_H

#include <limits.h>

#ifndef ULONGDEFINED
#define ULONGDEFINED 1
typedef unsigned long ulong;
#endif

//typedef unsigned long long ulong;

typedef struct _order{
	ulong ID;
	int floor;
	int direction;
}order;

void	protocolInit(bool(*sendOrder2Elevator)(ulong, int, int), int(*getCost2Go2Floor)(int, int));
int		sendOrder(order o);
void	internalOrderDone(ulong id);


#endif
