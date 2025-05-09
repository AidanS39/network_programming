#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>

#include "handshake.h"
#include "util.h"

#define PORT_NUM 1004
#define MAX_USERNAME_LEN 32
#define MAX_FILENAME_LEN 64
#define BUFFER_SIZE 256
#define BACKLOG 5
#define JOINED 1
#define LEFT 0
#define SERVER_SHUTDOWN 1
#define SERVER_RUNNING 0

// TODO: implement MAX_CLIENTS

// Question do we want to add ROOM list here and create function to list available rooms, etc
typedef struct _ServerState {
	int num_rooms;
	pthread_mutex_t server_state_mutex;
} ServerState;

ServerState server_state;


typedef struct _HandshakeResult {
	ConfirmationStatus status;
	char username[MAX_USERNAME_LEN];
	int room_number;
} HandshakeResult;

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

typedef struct _USR {
	int clisockfd;						// socket file descriptor
	char username[MAX_USERNAME_LEN];	// client username
	int color_code;						// user color
	struct _USR* next;					// for linked list queue
} USR;

typedef struct _ROOM {
	int room_number;
	int num_connected_clients;
	USR* usr_head;
	USR* usr_tail;
	struct _ROOM* next;
} ROOM;

ROOM* room_head = NULL;
ROOM* room_tail = NULL;

typedef struct _FileTransferThreadArgs {
	char recv_user[MAX_USERNAME_LEN];
	char file_name[MAX_FILENAME_LEN];
	ROOM* room;
	USR* send_user;
} FileTransferThreadArgs;

ROOM* create_room();
void remove_room(int room_number);
ROOM* find_room(int room_number);
void add_client(ROOM* room, int newclisockfd, char* username);
void remove_client(ROOM* room, int sockfd);
USR* find_client(ROOM* room, int sockfd);
void print_client_list(ROOM* room);
void print_room_list();
int get_color_code(ROOM* room, USR* client);
void broadcast(ROOM* room, int fromfd, char* username, int color_code, char* message);
void announce_status(ROOM* room, int fromfd, char* username, int status);
void* thread_main(void* args);
void print_rooms_with_clients();

HandshakeResult execute_handshake(int clisockfd);

int init_connection_confirmation(ConnectionConfirmation* cc, ConnectionRequest* cr, int clisockfd);
void handle_join_room_request(ConnectionConfirmation* cc, ConnectionRequest* cr, int clisockfd);
void handle_create_new_room_request(ConnectionConfirmation* cc, ConnectionRequest* cr, int clisockfd);
void handle_select_room_request(ConnectionConfirmation* cc);
void handle_invalid_request(ConnectionConfirmation* cc);

void mock_server_state();


int cc_set_available_rooms(ConnectionConfirmation* cc);


void init_server_state();

void clean_up();

ThreadArgs* init_thread_args(int newsockfd);

void init_server_state() {
	pthread_mutex_init(&server_state.server_state_mutex, NULL);
	server_state.num_rooms = 0;
}


void clean_up() {
	// close all client connections and free rooms and the clients in the rooms
	ROOM* cur_room = room_head;
	while (cur_room != NULL) {
		printf("Closing room %d\n", cur_room->room_number);
		USR* cur_usr = cur_room->usr_head;
		while (cur_usr != NULL) {
			printf("Disconnecting client %s\n", cur_usr->username);
			USR* next_usr = cur_usr->next;
			remove_client(cur_room, cur_usr->clisockfd);
			cur_usr = next_usr;
		}

		ROOM* next_room = cur_room->next;
		remove_room(cur_room->room_number);
		cur_room = next_room;
	}
}


ROOM* create_room()
{
	if (room_head == NULL) { // No rooms exist yet
		room_head = (ROOM*) malloc(sizeof(ROOM));
		room_head->room_number = 1;
		room_head->num_connected_clients = 0;
		room_head->usr_head = NULL;
		room_head->usr_tail = NULL;
		room_head->next = NULL;
		room_tail = room_head;
	} else { // At least one room exists
		room_tail->next = (ROOM*) malloc(sizeof(ROOM));
		room_tail->next->room_number = room_tail->room_number + 1;
		room_tail->next->num_connected_clients = 0;
		room_tail->next->usr_head = NULL;
		room_tail->next->usr_tail = NULL;
		room_tail->next->next = NULL;
		room_tail = room_tail->next;
	}

	pthread_mutex_lock(&server_state.server_state_mutex);
	server_state.num_rooms++;
	pthread_mutex_unlock(&server_state.server_state_mutex);

	return room_tail;
}

void remove_room(int room_number) {
	ROOM* cur = room_head;
	ROOM* prev;
	
	/* find room in room list, track previous */
	while (cur != NULL) {
		if (cur->room_number == room_number) {
			break;
		}
		else {
			prev = cur;
			cur = cur->next;
		}
	}

	// TODO: proper error handling
	assert(cur != NULL);

	/* remove room from room list */
	// if room is head of list
	if (cur == room_head) {
		if (room_head == room_tail) {
			room_head = NULL;
			room_tail = NULL;
		}
		else {
			room_head = cur->next;
			cur->next = NULL;
		}
	}
	// if room is tail of list
	else if (cur == room_tail) {
		prev->next = NULL;
		room_tail = prev;
	}
	// if room is neither head or tail of list
	else {
		prev->next = cur->next;
		cur->next = NULL;
	}

	free(cur);

	pthread_mutex_lock(&server_state.server_state_mutex);
	server_state.num_rooms--;
	pthread_mutex_unlock(&server_state.server_state_mutex);
}

ROOM* find_room(int room_number) {

	ROOM* cur_room = room_head;
	
	while(cur_room != NULL) {
		// if room found, return cur_room
		if (cur_room->room_number == room_number) {
			return cur_room;
		}
		else {
			cur_room = cur_room->next;
		}
	}
	// if cur_room not found, return NULL;
	return NULL;
}

void add_client(ROOM* room, int newclisockfd, char* username)
{
	/* add client to room */
	// if room is empty, add client to head of user list
	if (room->usr_head == NULL) {
		room->usr_head = (USR*) malloc(sizeof(USR));
		room->usr_head->clisockfd = newclisockfd;
		strncpy(room->usr_head->username, username, MAX_USERNAME_LEN);
		room->usr_head->next = NULL;
		room->usr_tail = room->usr_head;
	} 
	// if room is not empty, add client to tail of list
	else {
		room->usr_tail->next = (USR*) malloc(sizeof(USR));
		room->usr_tail->next->clisockfd = newclisockfd;
		strncpy(room->usr_tail->next->username, username, MAX_USERNAME_LEN);
		room->usr_tail->next->next = NULL;
		room->usr_tail = room->usr_tail->next;
	}
	room->num_connected_clients++;
}

void remove_client(ROOM* room, int sockfd) {
	
	USR *cur = room->usr_head;
	USR *prev;
	
	/* find client in client list, track previous */
	while (cur != NULL) {
		if (cur->clisockfd == sockfd) {
			break;
		}
		else {
			prev = cur;
			cur = cur->next;
		}
	}

	// TODO: proper error handling
	assert(cur != NULL);

	/* remove client from client list */
	// if client is head of list
	if (cur == room->usr_head) {
		if (room->usr_head == room->usr_tail) {
			room->usr_head = NULL;
			room->usr_tail = NULL;
		}
		else {
			room->usr_head = cur->next;
			cur->next = NULL;
		}
	}
	// if client is tail of list
	else if (cur == room->usr_tail) {
		prev->next = NULL;
		room->usr_tail = prev;
	}
	// if client is neither head or tail of list
	else {
		prev->next = cur->next;
		cur->next = NULL;
	}

	free(cur);
	room->num_connected_clients--;
}

USR* find_client(ROOM* room, int sockfd) {
	
	USR* cur_client = room->usr_head;
	
	while (cur_client != NULL) {
		// if client found, return cur_client
		if (cur_client->clisockfd == sockfd) {
			return cur_client;
		}
		cur_client = cur_client->next;
	}
	// if client not found, return NULL
	return NULL;
}

void print_client_list(ROOM* room) {
	
	USR *cur = room->usr_head;

	printf("CONNECTED CLIENTS IN ROOM %d:\n", room->room_number);
	while (cur != NULL) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		if (getpeername(cur->clisockfd, (struct sockaddr*)&addr, &len) < 0) {
			error("ERROR Unknown client!");
		}

		printf("%s (%s)\n", cur->username, inet_ntoa(addr.sin_addr));
		cur = cur->next;
	}
}

void print_room_list() {
	ROOM* cur_room = room_head;
	while (cur_room != NULL) {
		printf("Room %d:\n", cur_room->room_number);
		cur_room = cur_room->next;
	}
}



// TODO: potential refactor
// CREATE ROOM
// MAX_CLIENTS 
// colors = [0, 0, 0, 0, 0]
// colors = [91, 92, 93, 94, 95]
// colors = [93, 91, 92, 94, 95]
// client->color = colors[client->client_id]
// client_id increments and decrements on join and leave
int get_color_code(ROOM* room, USR* client) {

	int random_color_code;
	USR* cur;

	int color_found = 0;
	
	// find a unique color code
	while (color_found == 0) {
		// create a random color code
		srand(time(NULL));
		random_color_code = (rand() % 6) + 91;
		
		int taken = 0;
		
		// determine if color code is already taken
		if (room->usr_head == NULL) {
			color_found = 1;
		}
		else {
			cur = room->usr_head;
			while (cur != NULL) {
				// if taken, pick another color
				if ((cur->clisockfd != client->clisockfd) && (cur->color_code == random_color_code)) {
					taken = 1;
					break;
				}
				cur = cur->next;
			}
		}
		// if not taken, keep picked color	
		if (taken == 0) {
			color_found = 1;
		}
	}
	
	client->color_code = random_color_code;

	return random_color_code;
}
void broadcast(ROOM* room, int fromfd, char* username, int color_code, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0){
		printf("broadcast error\n");
		error("ERROR Unknown sender!");
	}

	// traverse through all connected clients in room
	USR* cur = room->usr_head;
	
	char buffer[512];

	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			memset(buffer, 0, 512);
			// prepare message
			sprintf(buffer, "\033[%dm[%s (%s)]:%s\033[0m", color_code, username, inet_ntoa(cliaddr.sin_addr), message);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() on broadcast failed");
		}

		cur = cur->next;
	}
}

// TODO: make status an enum
// TODO: consider removing client from room before announcing. Save username and ip address and return it from the remove client function
void announce_status(ROOM* room, int fromfd, char* username, int status)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) {
		printf("announce error\n");
		error("ERROR Unknown sender!");
	}

	char buffer[512];
	
	char* status_string;
	if (status) {
		status_string = "joined";
	}
	else {
		status_string = "left";
	}

	// traverse through all connected clients
	USR* cur = room->usr_head;
	
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd || status) {
			
			// prepare status announcement
			memset(buffer, 0, 512);
			sprintf(buffer, "%s (%s) has %s chat room %d!\n", username, 
			inet_ntoa(cliaddr.sin_addr), status_string, room->room_number);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() on announce_status failed");
		}

		cur = cur->next;
	}
}



int cc_set_available_rooms(ConnectionConfirmation* cc) {
	ROOM* cur_room = room_head;
	int i = 0;
	while (cur_room != NULL) {
		cc->available_rooms.rooms[i].room_number = cur_room->room_number;
		cc->available_rooms.rooms[i].num_connected_clients = cur_room->num_connected_clients;
		i++;
		cur_room = cur_room->next;
	}
	cc->available_rooms.num_rooms = i;
	return 0;
}



void handle_join_room_request(ConnectionConfirmation* cc, ConnectionRequest* cr, int clisockfd) {
	// check if room number is valid
	ROOM* requested_room = find_room(cr->room_number);

	if (requested_room != NULL) { // room exists
		// status
		cc->status = CONFIRMATION_SUCCESS;
		// connected room
		cc->connected_room.room_number = cr->room_number;
		cc->connected_room.num_connected_clients = requested_room->num_connected_clients;

		add_client(requested_room, clisockfd, cr->username);
	}
	else { // room does not exist
		// status
		cc->status = CONFIRMATION_FAILURE;
		// connected room
		cc->connected_room.room_number = UNINITIALIZED_ROOM_NUMBER;
		cc->connected_room.num_connected_clients = UNINITIALIZED_NUM_CONNECTED_CLIENTS;
	}
}

void handle_create_new_room_request(ConnectionConfirmation* cc, ConnectionRequest* cr, int clisockfd) {
	// status
	cc->status = CONFIRMATION_SUCCESS_NEW;
	// create and connect room
	ROOM* new_room = create_room();
	add_client(new_room, clisockfd, cr->username);
	// connected room
	cc->connected_room.room_number = new_room->room_number;
	cc->connected_room.num_connected_clients = new_room->num_connected_clients;
}

void handle_select_room_request(ConnectionConfirmation* cc) {
	// status
	cc->status = CONFIRMATION_PENDING;
	// connected room
	cc->connected_room.room_number = UNINITIALIZED_ROOM_NUMBER;
	cc->connected_room.num_connected_clients = UNINITIALIZED_NUM_CONNECTED_CLIENTS;
	// available rooms
	cc_set_available_rooms(cc);
}

void handle_invalid_request(ConnectionConfirmation* cc) {
	cc->status = CONFIRMATION_FAILURE;
	cc->connected_room.room_number = UNINITIALIZED_ROOM_NUMBER;
	cc->connected_room.num_connected_clients = UNINITIALIZED_NUM_CONNECTED_CLIENTS;
}

// Populates a ConnectionConfirmation struct with the appropriate values based on the ConnectionRequest
int init_connection_confirmation(ConnectionConfirmation* cc, ConnectionRequest* cr, int clisockfd) {
	memset(cc, 0, sizeof(ConnectionConfirmation));
	switch(cr->type) {
		case JOIN_ROOM: // client passed in room that they want to join
			handle_join_room_request(cc, cr, clisockfd);
			break;
		case CREATE_NEW_ROOM: // client wants to create a new room
			handle_create_new_room_request(cc, cr, clisockfd);
			break;
		case SELECT_ROOM: // client wants to select a room to join
			if (server_state.num_rooms == 0) { // no available rooms so create one
				handle_create_new_room_request(cc, cr, clisockfd);
			} else { // there are available rooms so select one
				handle_select_room_request(cc);
			}
			break;
		default:
			// including when the client cancels the handshake
			handle_invalid_request(cc);
			break;
	}
	return 0;
}

void mock_server_state() {
	server_state.num_rooms = 3;
	ROOM* room_1 = create_room();
	ROOM* room_2 = create_room();
	ROOM* room_3 = create_room();

	add_client(room_1, 1, "user_1");
	add_client(room_1, 2, "user_2");
	add_client(room_1, 3, "user_3");

	add_client(room_2, 4, "user_4");
	add_client(room_2, 5, "user_5");
	add_client(room_2, 6, "user_6");

	add_client(room_3, 7, "user_7");
	add_client(room_3, 8, "user_8");
	add_client(room_3, 9, "user_9");
}

void print_rooms_with_clients() {
	ROOM* cur_room = room_head;
	while (cur_room != NULL) {
		printf("Room %d: %d clients\n", cur_room->room_number, cur_room->num_connected_clients);
		USR* cur_client = cur_room->usr_head;
		while (cur_client != NULL) {
			printf("  %s\n", cur_client->username);
			cur_client = cur_client->next;
		}
		cur_room = cur_room->next;
	}
}

USR* find_client_by_username(ROOM* room, char* username) {
	USR* cur = room->usr_head;

	while (cur != NULL) {
		if (strncmp(cur->username, username, MAX_USERNAME_LEN) == 0) {
			break;
		}
		cur = cur->next;
	}
	// if client does not exist, NULL is returned
	return cur;
}

int is_filetransfer(char* buffer) {
	// create copy of message in buffer (needed for strtok_r)
	char message[BUFFER_SIZE];
	strncpy(message, buffer, strlen(buffer));
	
	// determine if first token in message is "SEND"
	char* saveptr;
	char* send_token = strtok_r(message, " ", &saveptr);
	// if there are no more tokens, return false (0)
	if (send_token == NULL) {
		return 0;
	}
	// if yes, return true (1)
	else if (strncmp(send_token, "SEND", strlen(send_token)) == 0) {
		return 1;
	}
	// if no, return false (0)
	else {
		return 0;
	}
}

FileTransferThreadArgs* init_FTthread_args(char* recv_user, char* file_name, ROOM* room, USR* send_user) {

	// prepare ThreadArgs structure to pass client socket
	FileTransferThreadArgs* args = (FileTransferThreadArgs*)
	malloc(sizeof(FileTransferThreadArgs));
	if (args == NULL) error("ERROR creating FT thread arguments");
	
	// set recv username
	strncpy(args->recv_user, recv_user, MAX_USERNAME_LEN);
	
	// set file name
	strncpy(args->file_name, file_name, MAX_FILENAME_LEN);
	
	// set room
	args->room = room;
	
	// set sending client
	args->send_user = send_user;

	return args;
}

void print_thread_args(char* recv_user, char* file_name) {
	printf("Receiving user: %s\n", recv_user);
	printf("File name: %s\n", file_name);
}

void* thread_file_transfer(void* args) {
	// get thread arguments
	char recv_user[MAX_USERNAME_LEN];
	strncpy(recv_user, ((FileTransferThreadArgs*) args)->recv_user, MAX_USERNAME_LEN);
	char file_name[MAX_FILENAME_LEN];
	strncpy(file_name, ((FileTransferThreadArgs*) args)->file_name, MAX_FILENAME_LEN);
	ROOM* room = ((FileTransferThreadArgs*) args)->room;
	USR* send_user = ((FileTransferThreadArgs*) args)->send_user;
	// free argument memory
	free(args);
	
	// TEST print statement
	print_thread_args(recv_user, file_name);
	
	// find client by username
	USR* recv_client = find_client_by_username(room, recv_user);
	if (recv_client == NULL) {
		printf("receiving client not found\n");
		pthread_exit(NULL);
	}
	printf("receiving client found!\n");

	int recvsockfd = recv_client->clisockfd;
	// TODO: all this in this order (ask me why)
	
	// receive file from sending client (somehow...?)
		
	// notify receiving user of file transfer, ask for permission "Y/N"
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);
	sprintf(buffer, "SEND %s %s", send_user->username, file_name);
	send(recvsockfd, buffer, BUFFER_SIZE, 0);
	
	// receive response from receiving user
	memset(buffer, 0, BUFFER_SIZE);
	recv(recvsockfd, buffer, BUFFER_SIZE, 0);

	// if recv user denies
	if (buffer[0] != 'Y') {
		// TODO: maybe return status code of DENIED or something like that?
	}
	// if recv user agrees
	else {
		// send file to receiving client (somehow...?)
	}

	pthread_exit(NULL);
}

int transfer_file(char* buffer, ROOM* room, USR* send_user) {
	// create copy of message in buffer (needed for strtok_r)
	char message[BUFFER_SIZE];
	strncpy(message, buffer, strlen(buffer));
	
	char* saveptr;
	char* send_token = strtok_r(message, " ", &saveptr);
	if (send_token == NULL) {
		return -1;
	}
	char* recv_user = strtok_r(NULL, " ", &saveptr);
	if (recv_user == NULL) {
		return -1;
	}
	char* file_name = strtok_r(NULL, " ", &saveptr);
	if (file_name == NULL) {
		return -1;
	}
	
	// initialize file transfer thread arguments
	FileTransferThreadArgs* args = init_FTthread_args(recv_user, file_name, room, send_user);

	// create file transferring thread
	pthread_t file_tid;
	if (pthread_create(&file_tid, NULL, thread_file_transfer, (void*) args) != 0) {
		error("ERROR creating a file transfer thread");
	}
	
	// wait for file transferring thread to finish
	pthread_join(file_tid, NULL);

	return 0;
}

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());



	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	HandshakeResult handshake_result = execute_handshake(clisockfd);
	ConfirmationStatus status = handshake_result.status;
	char username[MAX_USERNAME_LEN];
	strncpy(username, handshake_result.username, MAX_USERNAME_LEN);
	int room_number = handshake_result.room_number;

	
	if (status == CONFIRMATION_PENDING) {
		// keep sending confirmations until the handshake succeeds or fails
		ConnectionRequest cr;
		ConnectionConfirmation cc;
		cc.status = status;
		
		Buffer cr_buffer;
		init_buffer(&cr_buffer, sizeof(ConnectionRequest));
		Buffer cc_buffer;
		init_buffer(&cc_buffer, sizeof(ConnectionConfirmation));
		
		while (cc.status == CONFIRMATION_PENDING) {
			// reset the buffers and the structs
			memset(cc_buffer.data, 0, cc_buffer.size);
			memset(cr_buffer.data, 0, cr_buffer.size);
			memset(&cc, 0, sizeof(ConnectionConfirmation));
			memset(&cr, 0, sizeof(ConnectionRequest));

			// receive the new request from the client
			recv(clisockfd, cr_buffer.data, cr_buffer.size, 0);
			deserialize_connection_request(&cr, &cr_buffer);
			print_connection_request_struct(&cr);

			// generate a new confirmation
			init_connection_confirmation(&cc, &cr, clisockfd);
			serialize_connection_confirmation(&cc_buffer, &cc);
			// send the confirmation to the client
			send(clisockfd, cc_buffer.data, cc_buffer.size, 0);
		}
		room_number = cc.connected_room.room_number;

		cleanup_buffer(&cr_buffer);
		cleanup_buffer(&cc_buffer);
	}
	if (status == CONFIRMATION_FAILURE) {
		// printf("Server says confirmation failure\n");
		close(clisockfd);
		pthread_exit(NULL);
	}
	// printf("Server says confirmation success\n");

	// get room node
	ROOM* room = find_room(room_number);
	// get client node
	USR* client = find_client(room, clisockfd);
	// get color code
	int color_code = get_color_code(room, client);
	
	// get peername (ip & other info) of client socket
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if (getpeername(clisockfd, (struct sockaddr*)&addr, &len) < 0) {
		printf("thread main top error\n");
		error("ERROR Unknown sender!");
	}

	// print log in server that user has connected
	printf("Connected: %s (%s)\n", username, inet_ntoa(addr.sin_addr));
	
	// print the updated list of clients
	print_client_list(room);
	
	// announce to room that client joined
	announce_status(room, clisockfd, username, JOINED);
	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nrcv;

	memset(buffer, 0, 256);

	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed first recv");
	
	while (nrcv > 0 && buffer[0] != '\n') {
		if (is_filetransfer(buffer)) {
			// transfer file
			if (transfer_file(buffer, room, client) == -1) {
				// send invalid format message to sending user
				printf("File transfer failed.\n");
			}
		} else {
			// we send the message to everyone except the sender
			broadcast(room, clisockfd, username, color_code, buffer);
		}
		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed in while loop");
	}

	remove_client(room, clisockfd);

	// send message to all users that user has left
	announce_status(room, clisockfd, username, LEFT);
	
	// print log in server that user has disconnected
	printf("Disconnected: %s (%s)\n", username, inet_ntoa(addr.sin_addr));

	print_client_list(room);
	
	close(clisockfd);
	
	room = NULL;

	client = NULL;

	return NULL;
}

ThreadArgs* init_thread_args(int newsockfd) {
	ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	if (args == NULL) error("ERROR creating thread arguments");
	args->clisockfd = newsockfd;
	return args;
}



HandshakeResult execute_handshake(int clisockfd) {
	/*================================HANDSHAKE================================*/
	// retrieve room_number and username from client
	Buffer cr_buffer;
	init_buffer(&cr_buffer, sizeof(ConnectionRequest));
	ConnectionRequest cr;

	Buffer cc_buffer;
	init_buffer(&cc_buffer, sizeof(ConnectionConfirmation));
	ConnectionConfirmation cc;

	int handshake_complete = 0;
	while (handshake_complete == 0) {
		// client sends connection request server processes it into a ConnectionRequest struct
		int nrcv = recv(clisockfd, cr_buffer.data, cr_buffer.size, 0);
		if (nrcv < 0) error("ERROR recv() failed");
		deserialize_connection_request(&cr, &cr_buffer);
		// print_connection_request_struct(&cr);

		// Process the connection request and generate a connection confirmation in response
		// generate connection confirmation from connection request
		init_connection_confirmation(&cc, &cr, clisockfd);
		// serialize connection confirmation
		serialize_connection_confirmation(&cc_buffer, &cc);

		if (cc.status != CONFIRMATION_PENDING) {
			handshake_complete = 1;
		}

		// send the confirmation to the client
		send(clisockfd, cc_buffer.data, cc_buffer.size, 0);
	}

	// Make sure to clean up the buffer
	cleanup_buffer(&cr_buffer);
	cleanup_buffer(&cc_buffer);

	// return struct with confirmation status, room number, and username
	HandshakeResult handshake_result;
	handshake_result.status = cc.status;
	handshake_result.room_number = cc.connected_room.room_number;
	strncpy(handshake_result.username, cr.username, MAX_USERNAME_LEN);
	return handshake_result;
}

int main()
{
	init_server_state();
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	// avoid "Address already in use" error
	// https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html
	int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, BACKLOG); // maximum number of connections = 5
	
	while(1) {
		
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);

		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) {
			error("ERROR on accept");
		}
		
		/*=================SET THREAD ARGS=============================*/
		// ThreadArgs* args = init_thread_args(&cc, cr.username, newsockfd);
		ThreadArgs* args = init_thread_args(newsockfd);

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) {
			error("ERROR creating a new thread");
		}
	}
	close(sockfd);

	return 0; 
}
