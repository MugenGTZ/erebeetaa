#ifndef _UDP_H
#define _UDP_H

int  createUDPSocket		();
int  createUDPTimeoutSocket	(long int seconds);
void bindToSocket			(int sck, unsigned short portNum);
void setBroadcastSocket		(int sck, struct sockaddr_in *broadcastAddr,const char* subnetBcast, socklen_t addrlen, unsigned short portNum);
void setReceiver			(struct sockaddr_in *remoteAddrStruct, socklen_t addrlen, const char* remoteIP, unsigned short portNum);

#endif
