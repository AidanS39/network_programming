#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> 

#include "handshake.h"
#include "util.h"
#include "connection_status_monitor.h"
#include "socket_setup.h"

#define BUFFER_SIZE 512
#define EXIT_COMMAND "\n"


typedef struct _ThreadArgs {
	int clisockfd;
	ConnectionStatusMonitor* csm;
} ThreadArgs;


void* thread_main_recv(void* args);
void* thread_main_send(void* args);


// TODO: revisit when dealing with messages cleanly and use Buffer struct
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
	Buffer cr_buffer;
	init_buffer(&cr_buffer, sizeof(ConnectionRequest));
	prepare_connection_request(argc, room_arg, &cr_buffer);


	/*================================CONNECTION STATUS MONITOR============================*/
	// Initialize connection status monitor
	ConnectionStatusMonitor csm;
	csm_init(&csm);
	
	/*================================SOCKET CONNECTION====================================*/
	// Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	set_server_addr(argv[1], &serv_addr);

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
	
	// TODO: loop until handshake is successful
	initiate_server_handshake(sockfd, &cr_buffer);
	cleanup_buffer(&cr_buffer);


	
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

