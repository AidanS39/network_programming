#ifndef HANDSHAKE_H
#define HANDSHAKE_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "util.h"

#define MAX_USERNAME_LEN 32
#define MAX_ROOMS 32
#define UNINITIALIZED_ROOM_NUMBER -1
#define CREATE_NEW_ROOM_COMMAND "new"

/* NOTE: packing all my structs so that I don't have to worry about padding when
 * serializing and deserializing. Using stdint types to ensure the sizes are 
 * portable across all systems (host and clients).
 */
#pragma pack(push, 1)
typedef enum _ConnectionRequestType {
	JOIN_ROOM,
	CREATE_NEW_ROOM,
	SELECT_ROOM
} ConnectionRequestType;

typedef struct _ConnectionRequest {
	char username[MAX_USERNAME_LEN];
	ConnectionRequestType type;
	int32_t room_number;
} ConnectionRequest;

typedef enum _ConfirmationStatus {
	CONFIRMATION_SUCCESS,
	CONFIRMATION_PENDING,
	CONFIRMATION_FAILURE
} ConfirmationStatus;

typedef struct _HandshakeRoomDescription {
	int32_t room_number;
	int32_t num_connected_clients;
} HandshakeRoomDescription;

typedef struct _HandshakeRoomsInfo {
	int32_t num_rooms;
	HandshakeRoomDescription rooms[MAX_ROOMS];
} HandshakeRoomsInfo;

typedef struct _ConnectionConfirmation {
	ConfirmationStatus status;
	HandshakeRoomsInfo rooms_info;
} ConnectionConfirmation;



int set_username(ConnectionRequest *cr);
int init_connection_request(int argc, char *room_arg, ConnectionRequest *cr);
void print_connection_request(ConnectionRequest *cr);
int initiate_server_handshake(int sockfd, unsigned char* handshake_buffer);
void print_connection_confirmation(ConnectionConfirmation *cc);
ConnectionConfirmation mock_server_connection_confirmation();
void prepare_connection_request(int argc, char* room_arg, unsigned char* buffer);
void print_serialized_connection_request(unsigned char* buffer);
void print_room_selection_prompt(ConnectionConfirmation *cc);
size_t serialize_connection_request(ConnectionRequest *cr, unsigned char* buffer);
size_t deserialize_connection_request(unsigned char* buffer, ConnectionRequest *cr);
#pragma pack(pop)

#endif


