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

#define PORT_NUM 1004

#define MAX_USERNAME_LEN 32
#define BUFFER_SIZE 256 

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


void error(const char *msg);
void csm_init(ConnectionStatusMonitor* monitor);
void csm_destroy(ConnectionStatusMonitor* monitor);
void* thread_main_recv(void* args);
void* thread_main_send(void* args);




void error(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
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



void* thread_main_recv(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	ConnectionStatusMonitor* csm = ((ThreadArgs*) args)->csm;
	free(args);
	
	// keep receiving and displaying message from server
	char buffer[512];
	int n;

	memset(buffer, 0, 512);

	n = recv(sockfd, buffer, 512, 0);
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
				memset(buffer, 0, 512);
				n = recv(sockfd, buffer, 512, 0);
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
	char buffer[256];
	int n;

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		//printf("\nPlease enter the message: ");
		memset(buffer, 0, 256);
		fgets(buffer, 255, stdin);

		// if (strlen(buffer) == 1) buffer[0] = '\0';

		n = send(sockfd, buffer, strlen(buffer), 0);
		if (n < 0) {
			error("ERROR writing to socket");
		}

		// Handle user manual disconnect
		if ((n == 1 && buffer[0] == '\n') || n == 0) {
			pthread_mutex_lock(&csm->connection_status_mutex);
			csm->connection_status = SENT_DISCONNECT_REQUEST;
			pthread_cond_signal(&csm->connection_status_cond);
			pthread_mutex_unlock(&csm->connection_status_mutex);
			break;
		}
	}
	return NULL;
}

int main(int argc, char *argv[])
{

	if (argc < 2) {
		error("Please specify hostname");
	}

	if (argc < 3) {
		error("Please specify room number");
	}

	// Initialize connection status monitor
	ConnectionStatusMonitor csm;
	csm_init(&csm);
	
	// get room number
	int room_number;
	
	// if new specified, set room number to -1 to signal server to make new room
	if (strncmp(argv[2], "new", 3) == 0) {
		// create new room
		room_number = -1;
	}
	else {
		room_number = (int)strtol(argv[2], NULL, 10);
	}

	/* get username from user */
	char username[MAX_USERNAME_LEN];
	printf("Type your user name: ");
	fgets(username, MAX_USERNAME_LEN - 1, stdin);
	if (username[0] == '\n') {
		error("Invalid username (Please type a valid username before pressing ENTER.");
	}
	username[strlen(username) - 1] = '\0'; // get rid of newline char

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	pthread_t tid_recv;
	pthread_t tid_send;

	ThreadArgs* args;
	
	char buffer[BUFFER_SIZE];
	int offset = 0;

	memset(buffer, 0, BUFFER_SIZE);
	*(int *)buffer = htonl(room_number);
	offset += sizeof(int);

	strncpy(buffer + offset, username, strlen(username));
	offset += strlen(username);

	int n = send(sockfd, buffer, BUFFER_SIZE, 0);
	if (n < 0) error("ERROR writing to socket");
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	args->csm = &csm;
	pthread_create(&tid_recv, NULL, thread_main_recv, (void*) args);
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	args->csm = &csm;
	pthread_create(&tid_send, NULL, thread_main_send, (void*) args);


	// NOTE: I'm pretty sure you can't/shouldn't join a detached thread
	// pthread_join(tid_send, NULL);
	pthread_mutex_lock(&csm.connection_status_mutex);
	while (csm.connection_status != RECEIVED_DISCONNECT_CONFIRMATION) {
		pthread_cond_wait(&csm.connection_status_cond, &csm.connection_status_mutex);
	}
	pthread_mutex_unlock(&csm.connection_status_mutex);

	close(sockfd);

	csm_destroy(&csm);

	return 0;
}

