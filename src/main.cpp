#include "Elevator.h"
#include "HW/elev.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "localServer.h"

//Creates an Elevator object, detects the input from the Floor panel box and sends orders to the protocol
int initElevatorProgram(){	
	Elevator elev;
	int newPress[8];
	static int oldPress[8];
	while(1){
		newPress[1] = (elev_get_button_signal(BUTTON_CALL_UP, 0)) ? 1 : 0;
		newPress[3] = (elev_get_button_signal(BUTTON_CALL_UP, 1)) ? 1 : 0;
		newPress[5] = (elev_get_button_signal(BUTTON_CALL_UP, 2)) ? 1 : 0;
		newPress[2] = (elev_get_button_signal(BUTTON_CALL_DOWN, 1)) ? 1 : 0;
		newPress[4] = (elev_get_button_signal(BUTTON_CALL_DOWN, 2)) ? 1 : 0;
		newPress[6] = (elev_get_button_signal(BUTTON_CALL_DOWN, 3)) ? 1 : 0;

		for(int i=1; i < 7; i++) {
			if(newPress[i] && !oldPress[i]){
				order newOrder;
				newOrder.ID 			= (ulong)(rand());
				newOrder.floor			= i/2;
				newOrder.direction		= (i & 0x01) ? 1 : -1;
				sendOrder(newOrder);
				elev_set_button_lamp((newOrder.direction == 1) ? BUTTON_CALL_UP : BUTTON_CALL_DOWN, newOrder.floor ,1);
			}
			oldPress[i] = newPress[i];
		}
	}
	return 0;
}

int spawn (char* program, char** arg_list){
	pid_t child_pid;
	child_pid = fork();
	if (child_pid != 0)
		return child_pid;
	else {
		execvp ("gnome-terminal", arg_list);
		printf ("an error occurred in execvp\n");
		abort ();
	}
}

void* primaryPPFun(void *){
	printf("**Primary Elevator Server!**\n");
	int pipePP;
	while(1){
		pipePP = open("IPC_pipe", O_RDWR);
		if(pipePP<1){ 
			printf("Error opening pipe");
			usleep(10000);
			continue;
		}
		write(pipePP, "I am alive", strlen("I am alive"));
		close(pipePP);
		//printf("Written to pipe\n");
		usleep(200000);
	}
	return 0;
}

int thereIsDataToRead(int fd) {
	struct timeval timeout;
	fd_set readfds;

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	select(fd+1, &readfds, NULL, NULL, &timeout);
	return (FD_ISSET(fd, &readfds));							//true if there is data
}

void* backUpPPFun(void *){
	printf("**Backup Elevator Server!**\n");
	int pipePP;
	char data[10];
	char* arg_list[] =  {(char*)"gnome-terminal",(char*)"-e",(char*)"./Elevator.o -b",NULL};
	
    while(1){
		pipePP = open("IPC_pipe",O_RDONLY | O_NONBLOCK);
		if(pipePP<1){ 
			printf("Error opening pipe");
			break;
		}
		if(!thereIsDataToRead(pipePP)){
			printf("Timeout reached, I am primary now!\n");
			spawn ((char*)"gnome-terminal", arg_list);
			printf ("Backup spawned\n");	
			break;
		}
    	read(pipePP, &data, 10); 
		close(pipePP);

		usleep(200000);
	}
	return 0;
}

int primaryProgram(){
	int pipe;
	char* arg_list[] =  {(char*)"gnome-terminal",(char*)"-e",(char*)"./Elevator.o -b",NULL};
	pthread_t primaryPP;
	
	system("rm IPC_pipe");
	pipe = mkfifo("IPC_pipe",0666);
	if(pipe < 0){
		printf("Unable to create a pipe");
		exit(-1);
	 }
	else printf("Pipe successfully created\n");
	
	spawn ((char*)"gnome-terminal", arg_list);
	printf ("Backup spawned\n");
	
	if(pthread_create(&primaryPP, NULL, primaryPPFun, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
	
	initElevatorProgram();
	return 0;
}

int BackUpProgram(){
	pthread_t backUpPP, primaryPP;
	
	if(pthread_create(&backUpPP, NULL, backUpPPFun, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
	
	pthread_join(backUpPP, NULL);
	
	if(pthread_create(&primaryPP, NULL, primaryPPFun, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		exit (EXIT_FAILURE);
	}
	
	initElevatorProgram();
	
	return 0;
}

int main(int argc, char** argv){
	if(argc > 1){ if(!strcmp(argv[1], "-b")) return BackUpProgram();}
	return primaryProgram();
}
