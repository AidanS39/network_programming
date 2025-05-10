#include "file_transfer.h"

// NOTE: buffer is in format "SEND <receiver_username> <filename>"
FTRequest* create_ft_request(char* buffer, int sockfd, char* sender_username,
 int32_t* global_room_number, FTRequestType type) {
	FTRequest* ft_req = (FTRequest*) malloc(sizeof(FTRequest));
	char* saveptr;

	// check if buffer is in format "SEND <username> <filename>"
	char* send_token = strtok_r(buffer, " ", &saveptr);
	if (strcmp(send_token, "SEND") != 0) {
		error("Invalid file transfer request");
	}

	// set type
	ft_req->type = (int32_t) type;

	// copy sender username
	strcpy(ft_req->sender_username, sender_username);

	// copy sender sockfd
	ft_req->sender_sockfd = sockfd;
	
	// copy room number
	ft_req->room_number = *global_room_number;

	// copy receiver username
    char* receiver_username = strtok_r(NULL, " ", &saveptr);
    if (receiver_username == NULL) {
        error("Invalid file transfer request");
    }
    strcpy(ft_req->receiver_username, receiver_username);

    // copy file name
    char* file_name = strtok_r(NULL, " ", &saveptr);
    if (file_name == NULL) {
        error("Invalid file transfer request");
    }

    strcpy(ft_req->file_name, file_name);

	return ft_req;
}

// Sends file transfer request to server. Server will send back FTConfirmation
FTConfirmation* send_server_file_transfer_request(FTRequest* ft_req, int sockfd) {
	Buffer ft_req_buffer;
	init_buffer(&ft_req_buffer, sizeof(FTRequest));
	if (serialize_ftreq(&ft_req_buffer, ft_req, sockfd) != sizeof(FTRequest)) {
		error("Failed to serialize file transfer request");
	}

	int n = send(sockfd, ft_req_buffer.data, ft_req_buffer.size, 0);
	if (n < 0) {
		error("ERROR sending file transfer request");
	}

	// print_hex(ft_req_buffer.data, ft_req_buffer.size);
	// FTRequest* ft_req_deserialized = (FTRequest*) malloc(sizeof(FTRequest));
	// if (deserialize_ftreq(ft_req_deserialized, &ft_req_buffer) != sizeof(FTRequest)) {
	// 	error("Failed to deserialize file transfer request");
	// }
	// print_ft_request(ft_req_deserialized);
	// cleanup_buffer(&ft_req_buffer);
	// free(ft_req_deserialized);

	Buffer ft_conf_buffer;
	init_buffer(&ft_conf_buffer, sizeof(FTConfirmation));

	n = recv(sockfd, ft_conf_buffer.data, ft_conf_buffer.size, 0);
	if (n < 0) {
		error("ERROR receiving file transfer confirmation");
	}

	printf("Received FT confirmation\n");
	print_hex(ft_conf_buffer.data, ft_conf_buffer.size);

	FTConfirmation* ft_conf = (FTConfirmation*) malloc(sizeof(FTConfirmation));
	if (deserialize_ftconf(ft_conf, &ft_conf_buffer) != sizeof(FTConfirmation)) {
		error("Failed to deserialize file transfer confirmation");
	}

	printf("File transfer confirmation received from server: %d\n", ft_conf->status);

	cleanup_buffer(&ft_conf_buffer);
	cleanup_buffer(&ft_req_buffer);

	return ft_conf;
}

// Serializes FTRequest into buffer
size_t serialize_ftreq(Buffer* buffer, FTRequest* ft_req, int sockfd) {
    assert(buffer->size == sizeof(FTRequest));
    size_t offset = 0;

	// serialize FTRequest identifier
	int32_t ft_identifier_net = htonl(FT_REQUEST_ID);
	memcpy(buffer->data + offset, &ft_identifier_net, sizeof(ft_identifier_net));
	offset += sizeof(ft_identifier_net);

	// serialize type
	int32_t type_net = htonl(ft_req->type);
	memcpy(buffer->data + offset, &type_net, sizeof(type_net));
	offset += sizeof(type_net);

    // serialize sender username
    memcpy(buffer->data + offset, ft_req->sender_username, sizeof(ft_req->sender_username));
    offset += sizeof(ft_req->sender_username);

	// serialize sender sockfd
	memcpy(buffer->data + offset, &sockfd, sizeof(sockfd));
    offset += sizeof(sockfd);

	// serialize room number
	memcpy(buffer->data + offset, &ft_req->room_number, sizeof(ft_req->room_number));
	offset += sizeof(ft_req->room_number);

    // serialize receiver username
    memcpy(buffer->data + offset, ft_req->receiver_username, sizeof(ft_req->receiver_username));
    offset += sizeof(ft_req->receiver_username);

    // serialize file name
    memcpy(buffer->data + offset, ft_req->file_name, sizeof(ft_req->file_name));
    offset += sizeof(ft_req->file_name);

    return offset;
}

// Deserializes FTRequest from buffer
size_t deserialize_ftreq(FTRequest* ft_req, Buffer* buffer) {
	assert(buffer->size == sizeof(FTRequest));

	size_t offset = 0;

	// deserialize FTRequest identifier
	int32_t ft_identifier_net = 0;
	memcpy(&ft_identifier_net, buffer->data + offset, sizeof(ft_identifier_net));
	ft_req->ft_identifier = ntohl(ft_identifier_net);
	offset += sizeof(ft_identifier_net);

	// deserialize type
	int32_t type_net = 0;
	memcpy(&type_net, buffer->data + offset, sizeof(type_net));
	ft_req->type = ntohl(type_net);
	offset += sizeof(type_net);

	// deserialize sender username
	memcpy(ft_req->sender_username, buffer->data + offset, sizeof(ft_req->sender_username));
	offset += sizeof(ft_req->sender_username);

	// deserialize sender sockfd
	memcpy(&ft_req->sender_sockfd, buffer->data + offset, sizeof(ft_req->sender_sockfd));
	offset += sizeof(ft_req->sender_sockfd);

	// deserialize room number
	memcpy(&ft_req->room_number, buffer->data + offset, sizeof(ft_req->room_number));
	offset += sizeof(ft_req->room_number);

	// deserialize receiver username
	memcpy(ft_req->receiver_username, buffer->data + offset, sizeof(ft_req->receiver_username));
	offset += sizeof(ft_req->receiver_username);

	// deserialize file name
	memcpy(ft_req->file_name, buffer->data + offset, sizeof(ft_req->file_name));
	offset += sizeof(ft_req->file_name);

	return offset;
}

// Serializes FTConfirmation into buffer
size_t serialize_ftconf(Buffer* buffer, FTConfirmation* ft_conf) {
	assert(buffer->size == sizeof(FTConfirmation));
	size_t offset = 0;

	// serialize status
	int32_t status_net = htonl(ft_conf->status);
	memcpy(buffer->data + offset, &status_net, sizeof(status_net));
	offset += sizeof(status_net);

	return offset;
}

// Deserializes FTConfirmation from buffer
size_t deserialize_ftconf(FTConfirmation* ft_conf, Buffer* buffer) {
	assert(buffer->size == sizeof(FTConfirmation));

	size_t offset = 0;

	// deserialize status
	int32_t status_net = 0;
	memcpy(&status_net, buffer->data + offset, sizeof(status_net));
	ft_conf->status = ntohl(status_net);
	offset += sizeof(status_net);

	return offset;
}

// Prints out members of a FTRequest struct
void print_ft_request(FTRequest* ft_req) {
	printf("FTRequest: sender_username = %s, sender_sockfd = %d, sender_room_number = %d, receiver_username = %s, file_name = %s, type = %d\n",
	 ft_req->sender_username, ft_req->sender_sockfd, ft_req->room_number, ft_req->receiver_username, ft_req->file_name, ft_req->type);
}

// Prints out members of a FTConfirmation struct
void print_ft_confirmation(FTConfirmation* ft_conf) {
	printf("FTConfirmation: status = %d\n", ft_conf->status);
}