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
#define BUFFER_SIZE 256
#define BACKLOG 5
#define JOINED 1
#define LEFT 0
#define SERVER_SHUTDOWN 1
#define SERVER_RUNNING 0


typedef struct _ThreadArgs {
	char username[MAX_USERNAME_LEN];
	int clisockfd;
	int room_number;
} ThreadArgs;

typedef struct _USR {
	int clisockfd;						// socket file descriptor
	char username[MAX_USERNAME_LEN];	// client username
	int color_code;						// user color
	struct _USR* next;					// for linked list queue
} USR;

// TODO: add num_clients in room
typedef struct _ROOM {
	int room_number;
	USR* usr_head;
	USR* usr_tail;
	struct _ROOM* next;
} ROOM;

ROOM* room_head = NULL;
ROOM* room_tail = NULL;



int create_room();
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

void initiate_client_handshake(int sockfd, ConnectionRequest* cr);


void clean_up();



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



// TODO: create max rooms and max clients
// API: char* generate_rooms_summary() -> [Room1: 1people\n,...\0]
// refactor names to be main_client and main_server


int create_room()
{
	if (room_head == NULL) {
		room_head = (ROOM*) malloc(sizeof(ROOM));
		room_head->room_number = 1;
		room_head->usr_head = NULL;
		room_head->usr_tail = NULL;
		room_head->next = NULL;
		room_tail = room_head;
	} else {
		room_tail->next = (ROOM*) malloc(sizeof(ROOM));
		room_tail->next->room_number = room_tail->room_number + 1;
		room_tail->next->usr_head = NULL;
		room_tail->next->usr_tail = NULL;
		room_tail->next->next = NULL;
		room_tail = room_tail->next;
	}

	return room_tail->room_number;
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

	/* remove room from room list */
	// if room is head of list
	if (cur == room_head) {
		if (room_head == room_tail) {
			room_head = NULL;
			room_tail = NULL;
			free(cur);
		}
		else {
			room_head = cur->next;
			cur->next = NULL;
			free(cur);
		}
	}
	// if room is tail of list
	else if (cur == room_tail) {
		prev->next = NULL;
		room_tail = prev;
		free(cur);
	}
	// if room is neither head or tail of list
	else {
		prev->next = cur->next;
		cur->next = NULL;
		free(cur);
	}
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

	/* remove client from client list */
	// if client is head of list
	if (cur == room->usr_head) {
		if (room->usr_head == room->usr_tail) {
			room->usr_head = NULL;
			room->usr_tail = NULL;
			free(cur);
		}
		else {
			room->usr_head = cur->next;
			cur->next = NULL;
			free(cur);
		}
	}
	// if client is tail of list
	else if (cur == room->usr_tail) {
		prev->next = NULL;
		room->usr_tail = prev;
		free(cur);
	}
	// if client is neither head or tail of list
	else {
		prev->next = cur->next;
		cur->next = NULL;
		free(cur);
	}
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
			if (nsen != nmsg) error("ERROR send() failed");
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
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}


void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());


	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	char username[MAX_USERNAME_LEN];
	strncpy(username, ((ThreadArgs*) args)->username, MAX_USERNAME_LEN);
	int room_number = ((ThreadArgs*) args)->room_number;
	free(args);
	// get room node
	ROOM* room = find_room(room_number);
	
	// TODO: it's a little silly to add client then search for it. Have add_client return the client node
	// TODO: ideally the client creation should set the color code

	// add this new client to the specific client list
	add_client(room, clisockfd, username);
	
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
	int nsen, nrcv;

	memset(buffer, 0, 256);

		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed first recv");

	while (nrcv > 0 && buffer[0] != '\n') {
		// we send the message to everyone except the sender
		broadcast(room, clisockfd, username, color_code, buffer);

			memset(buffer, 0, 256);
			nrcv = recv(clisockfd, buffer, 255, 0);
			if (nrcv < 0) error("ERROR recv() failed in while loop");
	}


	// send message to all users that user has left
	announce_status(room, clisockfd, username, LEFT);
	
	// print log in server that user has disconnected
	printf("Disconnected: %s (%s)\n", username, inet_ntoa(addr.sin_addr));

	remove_client(room, clisockfd);

	print_client_list(room);
	
	close(clisockfd);
	
	room = NULL;

	client = NULL;

	return NULL;
}


int init_connection_confirmation(ConnectionConfirmation* cc, ConnectionRequest* cr) {
	memset(cc, 0, sizeof(ConnectionConfirmation));
	switch(cr->type) {
		case JOIN_ROOM: // client passed in room that they want to join
			if (find_room(cr->room_number) != NULL) {
				cc->status = CONFIRMATION_SUCCESS;
			}
			else {
				cc->status = CONFIRMATION_FAILURE;
			}
			break;
		case CREATE_NEW_ROOM: // client wants to create a new room
			cc->status = CONFIRMATION_SUCCESS;
			break;
		case SELECT_ROOM: // client wants to select a room to join
			cc->status = CONFIRMATION_PENDING;
			break;
		default:
			cc->status = CONFIRMATION_FAILURE;
			break;
	}
	return 0;
}

// void initiate_client_handshake(int sockfd, ConnectionRequest* cr) {
// }
size_t deserialize_connection_request(unsigned char* buffer, ConnectionRequest *cr) {
	size_t offset = 0;

	// deserialize username
	memcpy(cr->username, buffer + offset, sizeof(cr->username));
	offset += sizeof(cr->username);

	// deserialize type
	int32_t type_net = 0;
	memcpy(&type_net, buffer + offset, sizeof(type_net));
	cr->type = (ConnectionRequestType) ntohl(type_net);
	offset += sizeof(type_net);

	// deserialize room number
	int32_t room_number_net = 0;
	memcpy(&room_number_net, buffer + offset, sizeof(room_number_net));
	cr->room_number = ntohl(room_number_net);
	offset += sizeof(room_number_net);

	return offset;
}


int main(int argc, char *argv[])
{

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
	
	char buffer[BUFFER_SIZE];
	int offset;
	int nrcv;

	while(1) {
		offset = 0;
		
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);

		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) {
			error("ERROR on accept");
		}
		
		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		// retrieve room_number and username from client
		unsigned char handshake_buffer[sizeof(ConnectionRequest)];
		memset(handshake_buffer, 0, sizeof(handshake_buffer));
		nrcv = recv(newsockfd, handshake_buffer, sizeof(ConnectionRequest), 0);
		if (nrcv < 0) error("ERROR recv() failed");

		ConnectionRequest cr;
		deserialize_connection_request(handshake_buffer, &cr);
		print_connection_request(&cr);

		// ConnectionConfirmation cc;
		// init_connection_confirmation(&cc, &cr);




		memset(buffer, 0, BUFFER_SIZE);
		nrcv = recv(newsockfd, buffer, BUFFER_SIZE, 0);
		if (nrcv < 0) error("ERROR recv() failed");
		
		/* set thread args */
		// set room_number
		args->room_number = ntohl(*(int *)(buffer));
		offset += sizeof(int);
		
	
		// set username
		strncpy(args->username, (buffer + offset), MAX_USERNAME_LEN);
		offset += strlen(args->username);
		
		// set socket file descriptor
		args->clisockfd = newsockfd;
		
		// if room number = -1, then create new room and assign new room number
		if (args->room_number == -1) {
			args->room_number = create_room();
		}
		else if (find_room(args->room_number) == NULL) {
			error("Invalid room number");
		}

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) {
			error("ERROR creating a new thread");
		}
	}

	close(sockfd);

	return 0; 
}