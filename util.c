#include "util.h"


/* NOTE: The username is modified in place and the char * just gets moved
 * to the first non-whitespace character. So beware original buffer will look like:
 * [ , \n, \t, , , , u, s, e, r, n, a, m, e, \0, \0, \0, \0]
 * 
 * Assumed properly null terminated string.
*/
char* trim_whitespace(char *str_start) {
    assert(str_start != NULL);

    // move pointer to start of string to the first non-whitespace character
    while (isspace((unsigned char)*str_start)) {
		str_start = str_start + 1;
	}

	// if it's all whitespace just return the pointer that points to the first non-whitespace character
    if (*str_start == '\0') {
		return str_start;
	}

    // Find the end of the string and move it backwards to the first non-whitespace character you encounter
    char *str_end = str_start + strlen(str_start) - 1;
    while (str_end > str_start && isspace((unsigned char)*str_end)) {
		str_end = str_end - 1;
	}

    // Add a null-terminator right after the last non-whitespace character
    *(str_end + 1) = '\0';

    return str_start;
}

// checks if a string is a number
int is_number(char* str) {
	for (size_t i = 0; i < strlen(str); i++) {
		if (!isdigit(str[i])) {
			return 0;
		}
	}
	return 1;
}



void error(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}


void print_hex(const unsigned char* buffer, size_t len) {
	for (size_t i = 0; i < len; i++) {
		printf("%02X ", buffer[i]);
	}
	printf("\n");
}


void print_server_addr(struct sockaddr_in* serv_addr) {
	printf("sin_family: %d\n", serv_addr->sin_family);
	printf("sin_port: %d\n", ntohs(serv_addr->sin_port));
	printf("sin_addr: %s\n", inet_ntoa(serv_addr->sin_addr));
	printf("sin_zero: %s\n", serv_addr->sin_zero);
}

// Free memory allocated for the buffer and reset the size and data pointer
void cleanup_buffer(Buffer* buffer) {
	free(buffer->data);
	buffer->data = NULL;
	buffer->size = 0;
}

// Initialize the buffer with the given size
void init_buffer(Buffer* buffer, size_t size) {
	buffer->size = size;
	buffer->data = (unsigned char*) malloc(size);
	memset(buffer->data, 0, size);
}
