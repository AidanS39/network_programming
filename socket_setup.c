#include "socket_setup.h"

void set_server_addr(int sockfd, char* hostname, struct sockaddr_in* serv_addr)
{
	memset((char*) serv_addr, 0, sizeof(*serv_addr));
	serv_addr->sin_family = AF_INET;
	serv_addr->sin_addr.s_addr = inet_addr(hostname);
	serv_addr->sin_port = htons(PORT_NUM);
}