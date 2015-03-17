#include "TCP.h" 


void ObtainServerAddr(sockaddr_info *serveraddr, int port){
	//build the server's Internet address
	bzero((char *) serveraddr, sizeof(sockaddr_info)); 		//did they do something stupid? sizeof(int)?

	// this is an Internet address
	serveraddr[0].sin_family = AF_INET;

	// let the system figure out our IP address
	serveraddr[0].sin_addr.s_addr = htonl(INADDR_ANY);

	// this is the port we will listen on
	serveraddr[0].sin_port = htons((unsigned short)port);
}

void createServerSocket(int *socked_fd, int port_num){
	sockaddr_info serveraddr; /* server's addr */

	//create socket
	socked_fd[0] = socket(AF_INET, SOCK_STREAM, 0);
	if (socked_fd[0] < 0) error("ERROR opening socket");
	
	//kill socket when exit
	int optval = 1;
	setsockopt(socked_fd[0], SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
	
	//obtaining address for server
	ObtainServerAddr(&serveraddr, port_num);
	
	//bind
	if (bind(socked_fd[0], (struct sockaddr *) &serveraddr, sizeof(sockaddr_info)) < 0) error("ERROR on binding");
}

