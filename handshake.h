#ifndef HANDSHAKE_H
#define HANDSHAKE_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "util.h"

#define MAX_USERNAME_LEN 32
#define MAX_ROOMS 8
#define UNINITIALIZED_ROOM_NUMBER -1
#define UNINITIALIZED_NUM_CONNECTED_CLIENTS -1
#define CREATE_NEW_ROOM_COMMAND "new" 


/*=========================================STRUCTS=========================================*/

/* NOTE: packing all my structs so that I don't have to worry about padding when
 * serializing and deserializing. Using stdint types to ensure the sizes are 
 * portable across all systems (host and clients).
 */
#pragma pack(push, 1)

/* ---------------------------------------- CONNECTION REQUEST ---------------------------------------- */

// Type of request for the initial handshake request from client to server
typedef enum _ConnectionRequestType {
	JOIN_ROOM,
	CREATE_NEW_ROOM,
	SELECT_ROOM
} ConnectionRequestType;


// Initial handshake request from client to server
typedef struct _ConnectionRequest {
	char username[MAX_USERNAME_LEN];
	ConnectionRequestType type;
	int32_t room_number;
} ConnectionRequest;


/* ---------------------------------------- CONNECTION CONFIRMATION ---------------------------------------- */

// Status of the handshake confirmation from server to client
typedef enum _ConfirmationStatus {
	CONFIRMATION_SUCCESS, // client successfully joined a room
	CONFIRMATION_PENDING, // server wants more information from client before connecting them
	CONFIRMATION_FAILURE // client requested an invalid operation
} ConfirmationStatus;

// basic information regarding a single room on the server
typedef struct _HandshakeRoomDescription {
	int32_t room_number;
	int32_t num_connected_clients;
} HandshakeRoomDescription;

// information regarding all rooms on the server so the client can select a room to join
typedef struct _HandshakeAvailableRooms {
	int32_t num_rooms;
	HandshakeRoomDescription rooms[MAX_ROOMS];
} HandshakeAvailableRooms;

// confirmation from server to client after the handshake request
typedef struct _ConnectionConfirmation {
	ConfirmationStatus status;
	HandshakeRoomDescription connected_room; // if the client successfully joined a room
	HandshakeAvailableRooms available_rooms; // for selecting a room to join
} ConnectionConfirmation;
#pragma pack(pop)

/*=========================================FUNCTIONS=========================================*/

/* ---------------------------------------- CONNECTION REQUEST ---------------------------------------- */

int initiate_server_handshake(int sockfd, Buffer* cr_buffer);
void prepare_connection_request(int argc, char* room_arg, Buffer* cr_buffer);
int init_connection_request_struct(int argc, char *room_arg, ConnectionRequest *cr);
int set_username(ConnectionRequest *cr);

// SERIALIZATION

size_t serialize_connection_request(Buffer* cr_buffer, ConnectionRequest *cr);
void print_serialized_connection_request(Buffer* cr_buffer);

// DESERIALIZATION

size_t deserialize_connection_request(ConnectionRequest *cr, Buffer* cr_buffer);
void print_connection_request_struct(ConnectionRequest *cr);

// MOCKING

ConnectionConfirmation mock_server_connection_confirmation();

/* ---------------------------------------- CONNECTION CONFIRMATION ---------------------------------------- */

// SERIALIZATION

size_t serialize_connection_confirmation(Buffer* cc_buffer, ConnectionConfirmation *cc);
size_t serialize_handshake_available_rooms(Buffer* har_buffer, HandshakeAvailableRooms *har);
size_t serialize_handshake_room_description(Buffer* hrd_buffer, HandshakeRoomDescription *hrd);

void print_serialized_connection_confirmation(Buffer* cc_buffer);

// DESERIALIZATION

size_t deserialize_connection_confirmation(ConnectionConfirmation *cc, Buffer* cc_buffer);
size_t deserialize_handshake_available_rooms(HandshakeAvailableRooms *har, Buffer* har_buffer);
size_t deserialize_handshake_room_description(HandshakeRoomDescription *hrd, Buffer* hrd_buffer);

void print_connection_confirmation(ConnectionConfirmation *cc);

// MISC

void print_room_selection_prompt(ConnectionConfirmation *cc);
#endif


