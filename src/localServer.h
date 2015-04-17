#ifndef _LOCAL_SERVER_H
#define _LOCAL_SERVER_H

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

enum CHANORDERTYPE{COD_REQUEST, COD_RETURN};

typedef struct _Chanorder{
	CHANORDERTYPE 	cod;
	ulong 			ID;
	int 			floor;
	int 			direction;
	bool			success;
}Chanorder;

void	protocolInit(bool(*sendOrder2Elevator)(ulong, int, int), int(*getCost2Go2Floor)(int, int), void(*clearButtonLamp)(int, bool));
int		sendOrder(order o);
void	internalOrderDone(ulong id);


#endif
