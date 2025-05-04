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
#define JOINED 1
#define LEFT 0

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

int get_color_code(int sockfd) {

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
		if (head == NULL) {
			color_found = 1;
		}
		else {
			cur = head;
			while (cur != NULL) {
				// if taken, pick another color
				if ((cur->clisockfd != sockfd) && (cur->color_code == random_color_code)) {
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
	
	// find new socket, set random color code
	cur = head;
	while (cur != NULL) {
		if (cur->clisockfd == sockfd) {
			cur->color_code = random_color_code;
			break;
		}
		cur = cur->next;
	}

	return random_color_code;
}

void print_client_list() {
	
	USR *cur = head;

	printf("CONNECTED CLIENTS:\n");
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

void remove_client(int sockfd) {
	USR *cur = head;
	USR *prev;
	
	/* find client in client list */
	while (cur != NULL && cur->clisockfd != sockfd) {
		prev = cur;
		cur = cur->next;
	}

	/* remove client from client list */
	if (cur == head) {
		if (head == tail) {
			head = NULL;
			tail = NULL;
			free(cur);
		}
		else {
			head = cur->next;
			cur->next = NULL;
			free(cur);
		}
	}
	else if (cur == tail) {
		prev->next = NULL;
		tail = prev;
		free(cur);
	}
	else {
		prev->next = cur->next;
		cur->next = NULL;
		free(cur);
	}
}

void add_tail(int newclisockfd, char* username)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		strncpy(head->username, username, MAX_USERNAME_LEN);
		head->next = NULL;
		tail = head;
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		strncpy(tail->next->username, username, MAX_USERNAME_LEN);
		tail->next->next = NULL;
		tail = tail->next;
	}
}

void broadcast(int fromfd, char* username, int color_code, char* message)
{
	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(fromfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// traverse through all connected clients
	USR* cur = head;
	
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

void announce_status(int fromfd, char* username, int status)
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

			// prepare join announcement specifically for new client
			memset(buffer, 0, 512);
			sprintf(buffer, "%s (%s) has %s the chat room!\n", username,
			inet_ntoa(cliaddr.sin_addr), status_string);
			int nmsg = strlen(buffer);

			// send!
			int nsen = send(fromfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
	}
	else {
		status_string = "left";
	}

	// traverse through all connected clients
	USR* cur = head;
	
	while (cur != NULL) {
		// check if cur is not the one who sent the message
		if (cur->clisockfd != fromfd) {
			
			memset(buffer, 0, 512);

			// prepare status announcement
			sprintf(buffer, "%s (%s) has %s the chat room!\n", username,
			inet_ntoa(cliaddr.sin_addr), status_string);
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
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	char username[MAX_USERNAME_LEN];
	strncpy(username, ((ThreadArgs*) args)->username, MAX_USERNAME_LEN);
	free(args);
	
	add_tail(clisockfd, username); // add this new client to the client list
	
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if (getpeername(clisockfd, (struct sockaddr*)&addr, &len) < 0) {
		error("ERROR Unknown sender!");
	}

	// print log in server that user has connected
	printf("Connected: %s (%s)\n", username, inet_ntoa(addr.sin_addr));
	
	// print the updated list of clients
	print_client_list();
	
	announce_status(clisockfd, username, JOINED);
	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	int nsen, nrcv;
	
	int color_code = get_color_code(clisockfd);

	memset(buffer, 0, 256);
	nrcv = recv(clisockfd, buffer, 255, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		// we send the message to everyone except the sender
		broadcast(clisockfd, username, color_code, buffer);

		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 255, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}
	
	// send message to all users that user has left
	announce_status(clisockfd, username, LEFT);
	
	// print log in server that user has disconnected
	printf("Disconnected: %s (%s)\n", username, inet_ntoa(addr.sin_addr));
	
	remove_client(clisockfd);
	
	close(clisockfd);
	//-------------------------------
	
	print_client_list();
	


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
	
	char username[MAX_USERNAME_LEN];
	int nrcv;

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");
		
		// retrieve username from client
		memset(username, 0, MAX_USERNAME_LEN);
		nrcv = recv(newsockfd, username, MAX_USERNAME_LEN - 1, 0);
		if (nrcv < 0) error("ERROR recv() failed");
		
		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;
		strncpy(args->username, username, MAX_USERNAME_LEN);

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) {
			error("ERROR creating a new thread");
		}
	}

	return 0; 
}

