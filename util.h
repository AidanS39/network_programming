#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <assert.h>
#include <ctype.h> // for isspace()
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>



char* trim_whitespace(char *str);
void error(const char *msg);
void print_server_addr(struct sockaddr_in* serv_addr);
void print_hex(const unsigned char* buffer, size_t len);

#endif
