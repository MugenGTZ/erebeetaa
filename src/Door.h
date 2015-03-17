#ifndef _DOOR_H
#define _DOOR_H

class Door{		
	private:
		//static struct timespec begin, end;
		static int state;				//0 - closed; 1 - open/waiting/closing;		
	public:
		static void updateStatus(void);
		static void openAndCloseDoor();
		//static int isDoorClosed();
};				

#endif
