#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>
#include <ctype.h> // for isspace()
#include <assert.h>

#include "handshake.h"

#define PORT_NUM 1004

#define BUFFER_SIZE 512
#define EXIT_COMMAND "\n"
#define CREATE_NEW_ROOM_COMMAND "new"
#define UNINITIALIZED_ROOM_NUMBER -1


typedef enum _ConnectionStatus {
	CONNECTED,
	SENT_DISCONNECT_REQUEST,
	RECEIVED_DISCONNECT_CONFIRMATION
} ConnectionStatus;


// Monitor for connection status
typedef struct _ConnectionStatusMonitor {
	ConnectionStatus connection_status;
	pthread_mutex_t connection_status_mutex;
	pthread_cond_t connection_status_cond;
} ConnectionStatusMonitor;

typedef struct _ThreadArgs {
	int clisockfd;
	ConnectionStatusMonitor* csm;
} ThreadArgs;


void csm_init(ConnectionStatusMonitor* monitor);
void csm_destroy(ConnectionStatusMonitor* monitor);
void* thread_main_recv(void* args);
void* thread_main_send(void* args);


/*================================INITIAL CONNECTION================================*/
int set_username(ConnectionRequest *cr);
int init_connection_request(int argc, char *room_arg, ConnectionRequest *cr);
void print_connection_request(ConnectionRequest *cr);
int initiate_server_handshake(int sockfd, unsigned char* handshake_buffer);
void print_connection_confirmation(ConnectionConfirmation *cc);
void set_server_addr(int sockfd, char* hostname, struct sockaddr_in* serv_addr);
ConnectionConfirmation mock_server_connection_confirmation();
/*================================UTILITIES=========================================*/
char* trim_whitespace(char *str);
void error(const char *msg);
void print_server_addr(struct sockaddr_in* serv_addr);
void print_hex(const unsigned char* buffer, size_t len);



void error(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

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

void csm_init(ConnectionStatusMonitor* monitor) {
	monitor->connection_status = CONNECTED;
	pthread_mutex_init(&monitor->connection_status_mutex, NULL);
	pthread_cond_init(&monitor->connection_status_cond, NULL);
}

void csm_destroy(ConnectionStatusMonitor* monitor) {
	pthread_mutex_destroy(&monitor->connection_status_mutex);
	pthread_cond_destroy(&monitor->connection_status_cond);
}

// NOTE: you technically don't need argc, but it makes things more explicit than if I just checked room_arg being NULL
int init_connection_request(int argc, char* room_arg, ConnectionRequest *cr)
{
	memset(cr, 0, sizeof(ConnectionRequest));

	switch (argc) {
		case 2: // display rooms and ask user to choose one
			cr->type = SELECT_ROOM;
			cr->room_number = UNINITIALIZED_ROOM_NUMBER;
			break;
		case 3: // join room with specified room number or new room
			assert(room_arg != NULL);
			if (strncmp(room_arg, CREATE_NEW_ROOM_COMMAND, strlen(CREATE_NEW_ROOM_COMMAND)) == 0) {
				cr->type = CREATE_NEW_ROOM;
				cr->room_number = UNINITIALIZED_ROOM_NUMBER;
			}
			else {
				cr->type = JOIN_ROOM;
				cr->room_number = (int)strtol(room_arg, NULL, 10);
			}
			break;
	}

	set_username(cr);

	return 0;
}

// NOTE: structs are packed already so no padding
size_t serialize_connection_request(ConnectionRequest *cr, unsigned char* buffer) {
	size_t offset = 0;

	// serialize username (string is already array of chars so no need to worry about endianness)
	memcpy(buffer + offset, cr->username, sizeof(cr->username));
	offset += sizeof(cr->username);

	// serialize type (enum int)
	int32_t type_net = htonl((int32_t) cr->type);
	memcpy(buffer + offset, &type_net, sizeof(type_net));
	offset += sizeof(type_net);

	// serialize room number
	int32_t room_number_net = htonl(cr->room_number);
	memcpy(buffer + offset, &room_number_net, sizeof(room_number_net));
	offset += sizeof(room_number_net);

	return offset;
}


void print_connection_request(ConnectionRequest *cr)
{
	printf("Connection request username: %s\n", cr->username);
	printf("Connection request type: %d\n", cr->type);
	printf("Connection request room number: %d\n", cr->room_number);
}

ConnectionConfirmation mock_server_connection_confirmation()
{
	ConnectionConfirmation cc;
	cc.status = CONFIRMATION_SUCCESS;
	cc.rooms_info.num_rooms = 3;

	cc.rooms_info.rooms[0].room_number = 1;
	cc.rooms_info.rooms[0].num_connected_clients = 5;

	cc.rooms_info.rooms[1].room_number = 2;
	cc.rooms_info.rooms[1].num_connected_clients = 10;

	cc.rooms_info.rooms[2].room_number = 3;
	cc.rooms_info.rooms[2].num_connected_clients = 1;

	return cc;
}

void* thread_main_recv(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	ConnectionStatusMonitor* csm = ((ThreadArgs*) args)->csm;
	free(args);
	
	// keep receiving and displaying message from server
	char buffer[BUFFER_SIZE];
	int n;

	memset(buffer, 0, BUFFER_SIZE);

	n = recv(sockfd, buffer, BUFFER_SIZE, 0);
	switch (n) {
		case -1:
			error("ERROR recv() failed");
			break;
		case 0:	
			pthread_mutex_lock(&csm->connection_status_mutex);
			csm->connection_status = RECEIVED_DISCONNECT_CONFIRMATION;
			pthread_cond_signal(&csm->connection_status_cond);
			pthread_mutex_unlock(&csm->connection_status_mutex);
			break;
		default:
			printf("\n%s\n", buffer);

			while (n > 0) {
				memset(buffer, 0, BUFFER_SIZE);
				n = recv(sockfd, buffer, BUFFER_SIZE, 0);
				switch (n) {
					case -1:
						error("ERROR recv() failed");
						break;
					case 0:
						pthread_mutex_lock(&csm->connection_status_mutex);
						csm->connection_status = RECEIVED_DISCONNECT_CONFIRMATION;
						pthread_cond_signal(&csm->connection_status_cond);
						pthread_mutex_unlock(&csm->connection_status_mutex);
						break;
					default:
						printf("\n%s\n", buffer);
				}
			}
	}
	return NULL;
}

void* thread_main_send(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	ConnectionStatusMonitor* csm = ((ThreadArgs*) args)->csm;
	free(args);

	// keep sending messages to the server
	char buffer[BUFFER_SIZE];
	int n;

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		//printf("\nPlease enter the message: ");
		memset(buffer, 0, BUFFER_SIZE);
		fgets(buffer, BUFFER_SIZE - 1, stdin);

		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) {
			error("ERROR writing to socket");
		} else if (n == 0 && strlen(buffer) != 0) {
			error("(probably) ERROR writing to socket");
		}

		// Handle user manual disconnect
		if ((strncmp(buffer, EXIT_COMMAND, strlen(EXIT_COMMAND)) == 0)) {
			pthread_mutex_lock(&csm->connection_status_mutex);
			csm->connection_status = SENT_DISCONNECT_REQUEST;
			pthread_cond_signal(&csm->connection_status_cond);
			pthread_mutex_unlock(&csm->connection_status_mutex);
			break;
		}
	}
	return NULL;
}

int set_username(ConnectionRequest *cr)
{
	printf("Type your username: ");
	char username[MAX_USERNAME_LEN];
	if (fgets(username, MAX_USERNAME_LEN - 1, stdin)) {
		char *trimmed_username = trim_whitespace(username);
		// I have to copy the trimmed username to the struct because trim_whitespace() returns a char*
		// and the ConnectionRequest struct expects a char[MAX_USERNAME_LEN]
		strncpy(cr->username, trimmed_username, strlen(trimmed_username));
	} else {
		return 1;
	}
	return 0;
}


void set_server_addr(int sockfd, char* hostname, struct sockaddr_in* serv_addr)
{
	memset((char*) serv_addr, 0, sizeof(*serv_addr));
	serv_addr->sin_family = AF_INET;
	serv_addr->sin_addr.s_addr = inet_addr(hostname);
	serv_addr->sin_port = htons(PORT_NUM);
}

void print_connection_confirmation(ConnectionConfirmation *cc) {
	printf("Connection confirmation status: %d\n", cc->status);
	printf("Connection confirmation num rooms: %d\n", cc->rooms_info.num_rooms);

	for (int i = 0; i < cc->rooms_info.num_rooms; i++) {
		printf("Room number: %d\tNumber of connected clients: %d\n",
				cc->rooms_info.rooms[i].room_number,
				cc->rooms_info.rooms[i].num_connected_clients);
	}
}

void print_room_selection_prompt(ConnectionConfirmation *cc) {
	printf("Server says following options are available:\n");
	for (int i = 0; i < cc->rooms_info.num_rooms; i++) {
		char people_person[7];
		if (cc->rooms_info.rooms[i].num_connected_clients == 1) {
			strcpy(people_person, "person");
		} else {
			strcpy(people_person, "people");
		}
		printf("Room %d: %d %s\n", cc->rooms_info.rooms[i].room_number, cc->rooms_info.rooms[i].num_connected_clients, people_person);
	}
	printf("Choose the room number or type [new] to create a new room: ");
}

int initiate_server_handshake(int sockfd, unsigned char* handshake_buffer)
{
	int n = send(sockfd, handshake_buffer, sizeof(ConnectionRequest), 0);
	if (n < 0) error("ERROR writing to socket");

	// ConnectionConfirmation cc;
	// n = recv(sockfd, &cc, sizeof(cc), 0);
	// if (n < 0) error("ERROR reading from socket");

	// print_connection_confirmation(&cc);

	return 0;
}

// given command line arguments, populates a serialized connection request buffer
void prepare_connection_request(int argc, char* room_arg, unsigned char* buffer) {
	ConnectionRequest cr;
	init_connection_request(argc, room_arg, &cr);
	if (serialize_connection_request(&cr, buffer) != sizeof(ConnectionRequest)) {
		error("ERROR: Failed to serialize connection request");
	}
}

// prints the serialized connection request buffer as a ConnectionRequest struct
void print_serialized_connection_request(unsigned char* buffer) {
	printf("Printing serialized connection request...\n");
	ConnectionRequest cr;
	memcpy(&cr, buffer, sizeof(ConnectionRequest));
	print_connection_request(&cr);
}


int main(int argc, char *argv[])
{
	/*================================INITIAL CONNECTION================================*/
	char* room_arg = NULL;

	switch (argc) {
		case 2:
			break;
		case 3:
			room_arg = argv[2];
			break;
		default:
			error("ERROR: Invalid number of arguments\n"
			"Usage:\n"
			"./chat_client <hostname> <room_number>\n"
			"./chat_client <hostname> new\n"
			"./chat_client <hostname>");
	}

	// Parse command line arguments and set up initial connection request (serialized buffer)
	unsigned char handshake_buffer[sizeof(ConnectionRequest)];
	prepare_connection_request(argc, room_arg, handshake_buffer);

	print_serialized_connection_request(handshake_buffer);


	/*================================CONNECTION STATUS MONITOR============================*/
	// Initialize connection status monitor
	ConnectionStatusMonitor csm;
	csm_init(&csm);
	
	/*================================SOCKET CONNECTION====================================*/
	// Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	set_server_addr(sockfd, argv[1], &serv_addr);

	// Try connecting to server
	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (status < 0) error("ERROR connecting");


	/*================================SETUP THREADS========================================*/
	// Set up send and receive threads
	pthread_t tid_recv;
	pthread_t tid_send;
	ThreadArgs* args; // reuse for both threads
	
	initiate_server_handshake(sockfd, handshake_buffer);


	
	// args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	// args->clisockfd = sockfd;
	// args->csm = &csm;
	// pthread_create(&tid_recv, NULL, thread_main_recv, (void*) args);
	
	// args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	// args->clisockfd = sockfd;
	// args->csm = &csm;
	// pthread_create(&tid_send, NULL, thread_main_send, (void*) args);


	// // NOTE: I'm pretty sure you can't/shouldn't join a detached thread
	// // pthread_join(tid_send, NULL);
	// pthread_mutex_lock(&csm.connection_status_mutex);
	// while (csm.connection_status != RECEIVED_DISCONNECT_CONFIRMATION) {
	// 	pthread_cond_wait(&csm.connection_status_cond, &csm.connection_status_mutex);
	// }
	// pthread_mutex_unlock(&csm.connection_status_mutex);

	// close(sockfd);

	csm_destroy(&csm);

	return 0;
}

