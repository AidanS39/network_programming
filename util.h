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

typedef struct _Buffer {
	unsigned char* data;
	size_t size;
} Buffer;


char* trim_whitespace(char *str);
int is_number(char* str);

void error(const char *msg);
void print_server_addr(struct sockaddr_in* serv_addr);
void print_hex(const unsigned char* buffer, size_t len);

void init_buffer(Buffer* buffer, size_t size);
void cleanup_buffer(Buffer* buffer);

#endif
