#ifndef SOCKET_SETUP_H
#define SOCKET_SETUP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define PORT_NUM 1004

void set_server_addr(int sockfd, char* hostname, struct sockaddr_in* serv_addr);

#endif
