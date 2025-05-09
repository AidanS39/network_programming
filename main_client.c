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

// TODO: implement client state in such a way that the client can set a username
// and not have to ask the user for it again if the server asks for more information

// typedef struct _ClientState {
// 	char username[MAX_USERNAME_LEN];
// 	pthread_mutex_t client_state_mutex;
// } ClientState;

// global username variable
char username[MAX_USERNAME_LEN];

typedef struct _ThreadArgs {
	int clisockfd;
	ConnectionStatusMonitor* csm;
} ThreadArgs;

void init_username();
int is_filetransfer(char* buffer);
void* thread_main_recv(void* args);
void* thread_main_send(void* args);

// sets the global username. only should be called once (for threading)
void init_username() {
	printf("Type your username: ");
	if (fgets(username, MAX_USERNAME_LEN - 1, stdin) != NULL) {
		trim_whitespace(username);
	} else {
		error("Invalid username.");
	}
}

// NOTE: can (and should) be used for both send and receive
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

int receive_file(char* buffer, int sockfd) {
	// create copy of message in buffer (needed for strtok_r)
	char message[BUFFER_SIZE];
	strncpy(message, buffer, strlen(buffer));
	
	char* saveptr;
	char* send_token = strtok_r(message, " ", &saveptr);
	if (send_token == NULL) {
		return -1;
	}
	char* send_user = strtok_r(NULL, " ", &saveptr);
	if (send_user == NULL) {
		return -1;
	}
	char* file_name = strtok_r(NULL, " ", &saveptr);
	if (file_name == NULL) {
		return -1;
	}
	
	printf("%s wants to send a file %s to you. Receive? [Y/N]: ", send_user, file_name);
	
	// send approval or denial to server
	char response[2];
	response[0] = (char)fgetc(stdin);
	response[1] = '\0';

	int n = send(sockfd, response, 1, 0);
	if (n < 0) {
		error("ERROR sending file approval");
	}

	// receive file

	return 0;
}

// sends file to server to send to receiving client
// returns status of file transfer (success, fail, other?)
int send_file(char* buffer, int sockfd) {
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

	// open file

	// send file to server

	return 0;
}

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
			// TODO: receive file
			if (is_filetransfer(buffer)) {
				if (receive_file(buffer, sockfd) == -1) {
					error("ERROR receiving file");
				}
			}
			else {
				printf("\n%s\n", buffer);
			}

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
						if (is_filetransfer(buffer)) {
							// TODO: receive file
							if (receive_file(buffer, sockfd) == -1) {
								error("ERROR receiving file");
							}
						}
						else {
							printf("\n%s\n", buffer);
						}
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
		
		if (is_filetransfer(buffer)) {
			// TODO: send file
			if (send_file(buffer, sockfd) == -1) {
				error("ERROR sending file");
			}
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
	
	/*===========================USERNAME HANDLING================================*/
	// initialize global variable username 	
	init_username();

	// Parse command line arguments and set up initial connection request (serialized buffer)
	Buffer cr_buffer;
	init_buffer(&cr_buffer, sizeof(ConnectionRequest));
	prepare_connection_request(argc, room_arg, &cr_buffer, username);


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
	
	perform_handshake(sockfd, &serv_addr, &cr_buffer, username);
	cleanup_buffer(&cr_buffer);
	
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

