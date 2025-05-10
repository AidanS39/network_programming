#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include "util.h"

// FIXME: HACK!
#define FT_REQUEST_ID 0xDEADBEEF

typedef enum _FTRequestType {
    CLIENT_INITIATED,
    SERVER_INITIATED
} FTRequestType;

typedef struct _FTRequest {
    // FIXME: this is a hack and i don't like it but i'm rushing
    int32_t ft_identifier; // just a way to identify that this is a FTRequest
    int32_t type;
    char sender_username[MAX_USERNAME_LEN];
    int32_t sender_sockfd;
    int32_t room_number;
	char receiver_username[MAX_USERNAME_LEN];
	char file_name[MAX_FILENAME_LEN];
} FTRequest;

typedef enum _FTConfirmationStatus {
	ACCEPTED,
	REJECTED
} FTConfirmationStatus;

typedef struct _FTConfirmation {
	FTConfirmationStatus status;
} FTConfirmation;

FTRequest* create_ft_request(char* buffer, int sockfd, char* username, int32_t* global_room_number, FTRequestType type);
FTConfirmation* send_server_file_transfer_request(FTRequest* ft_req, int sockfd);
size_t serialize_ftreq(Buffer* buffer, FTRequest* ft_req, int sockfd);
size_t deserialize_ftreq(FTRequest* ft_req, Buffer* buffer);
size_t serialize_ftconf(Buffer* buffer, FTConfirmation* ft_conf);
size_t deserialize_ftconf(FTConfirmation* ft_conf, Buffer* buffer);

void print_ft_request(FTRequest* ft_req);
void print_ft_confirmation(FTConfirmation* ft_conf);

void send_ft_confirmation(FTConfirmation* ft_conf, int sockfd);
#endif