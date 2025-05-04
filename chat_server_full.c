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

#define PORT_NUM 1004

#define MAX_USERNAME_LEN 32
#define BUFFER_SIZE 256
#define JOINED 1
#define LEFT 0

int server_stop = 0; // for future use: see main function comments

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;						// socket file descriptor
	char username[MAX_USERNAME_LEN];	// client username
	int color_code;						// user color
	struct _USR* next;					// for linked list queue
} USR;

USR *head = NULL;
USR *tail = NULL;

typedef struct _ROOM {
	int room_number;
	USR* usr_head;
	USR* usr_tail;
	struct _ROOM* next;
} ROOM;

ROOM* room_head = NULL;
ROOM* room_tail = NULL;

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
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

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

typedef struct _ThreadArgs {
	char username[MAX_USERNAME_LEN];
	int clisockfd;
	int room_number;
} ThreadArgs;

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
	
	// add this new client to the specific client list
	add_client(room, clisockfd, username);
	
	// get client node
	USR* client = find_client(room, clisockfd);
	
	// get peername (ip & other info) of client socket
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if (getpeername(clisockfd, (struct sockaddr*)&addr, &len) < 0) {
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
	
	// get color code
	int color_code = get_color_code(room, client);

	memset(buffer, 0, 256);
	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(room, clisockfd, username, color_code, buffer);

		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}
	
	// send message to all users that user has left
	announce_status(room, clisockfd, username, LEFT);
	
	// print log in server that user has disconnected
	printf("Disconnected: %s (%s)\n", username, inet_ntoa(addr.sin_addr));

	remove_client(room, clisockfd);
	
	close(clisockfd);
	//-------------------------------
	
	print_client_list(room);
	
	room = NULL;

	client = NULL;

	return NULL;
}

int main(int argc, char *argv[])
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

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

	listen(sockfd, 5); // maximum number of connections = 5
	
	char buffer[BUFFER_SIZE];
	int offset;
	int nrcv;

	while(!server_stop) {
		offset = 0;
		
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");
		
		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		// retrieve room_number and username from client
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

		/** TODO: possibly create a thread for stopping server
		 *  IDEA - create a thread that would listen for the server to say exit 
		 *  or something else along those lines...then thread would clean up all
		 *  resources and then set server_stop to 0, which would end the program. */
	}

	return 0; 
}

