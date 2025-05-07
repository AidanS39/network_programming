#include "handshake.h"

// NOTE: you technically don't need argc, but it makes things more explicit than if I just checked room_arg being NULL
int init_connection_request(int argc, char* room_arg, ConnectionRequest *cr)
{
	memset(cr, 0, sizeof(ConnectionRequest));

	switch (argc) {
		case 2: // display rooms and ask user to choose one
			cr->type = SELECT_ROOM;
			cr->room_number = UNINITIALIZED_ROOM_NUMBER;
			break;
		case 3: // join room with specified room number or new room
			assert(room_arg != NULL);
			if (strncmp(room_arg, CREATE_NEW_ROOM_COMMAND, strlen(CREATE_NEW_ROOM_COMMAND)) == 0) {
				cr->type = CREATE_NEW_ROOM;
				cr->room_number = UNINITIALIZED_ROOM_NUMBER;
			}
			else {
				cr->type = JOIN_ROOM;
				cr->room_number = (int)strtol(room_arg, NULL, 10);
			}
			break;
	}

	set_username(cr);

	return 0;
}

// NOTE: structs are packed already so no padding
size_t serialize_connection_request(ConnectionRequest *cr, unsigned char* buffer) {
	memset(buffer, 0, sizeof(ConnectionRequest));
	size_t offset = 0;

	// serialize username (string is already array of chars so no need to worry about endianness)
	memcpy(buffer + offset, cr->username, sizeof(cr->username));
	offset += sizeof(cr->username);

	// serialize type (enum int)
	int32_t type_net = htonl((int32_t) cr->type);
	memcpy(buffer + offset, &type_net, sizeof(type_net));
	offset += sizeof(type_net);

	// serialize room number
	int32_t room_number_net = htonl(cr->room_number);
	memcpy(buffer + offset, &room_number_net, sizeof(room_number_net));
	offset += sizeof(room_number_net);

	return offset;
}


void print_connection_request(ConnectionRequest *cr)
{
	printf("Connection request username: %s\n", cr->username);
	printf("Connection request type: %d\n", cr->type);
	printf("Connection request room number: %d\n", cr->room_number);
}

ConnectionConfirmation mock_server_connection_confirmation()
{
	ConnectionConfirmation cc;
	cc.status = CONFIRMATION_SUCCESS;
	cc.connected_room.num_connected_clients = 3;

	cc.available_rooms.rooms[0].room_number = 1;
	cc.available_rooms.rooms[0].num_connected_clients = 5;

	cc.available_rooms.rooms[1].room_number = 2;
	cc.available_rooms.rooms[1].num_connected_clients = 10;

	cc.available_rooms.rooms[2].room_number = 3;
	cc.available_rooms.rooms[2].num_connected_clients = 1;

	return cc;
}

int set_username(ConnectionRequest *cr)
{
	printf("Type your username: ");
	char username[MAX_USERNAME_LEN];
	if (fgets(username, MAX_USERNAME_LEN - 1, stdin) != NULL) {
		char *trimmed_username = trim_whitespace(username);
		// I have to copy the trimmed username to the struct because trim_whitespace() returns a char*
		// and the ConnectionRequest struct expects a char[MAX_USERNAME_LEN]
		strncpy(cr->username, trimmed_username, strlen(trimmed_username));
	} else {
		return 1;
	}
	return 0;
}

void print_connection_confirmation(ConnectionConfirmation *cc) {
	printf("Connection confirmation status: %d\n", cc->status);
	printf("Connection confirmation num available rooms: %d\n", cc->available_rooms.num_rooms);

	for (int i = 0; i < cc->available_rooms.num_rooms; i++) {
		printf("Room number: %d\tNumber of connected clients: %d\n",
				cc->available_rooms.rooms[i].room_number,
				cc->available_rooms.rooms[i].num_connected_clients);
	}
}

void print_room_selection_prompt(ConnectionConfirmation *cc) {
	printf("Server says following options are available:\n");
	for (int i = 0; i < cc->available_rooms.num_rooms; i++) {
		char people_person[7];
		if (cc->available_rooms.rooms[i].num_connected_clients == 1) {
			strcpy(people_person, "person");
		} else {
			strcpy(people_person, "people");
		}
		printf("Room %d: %d %s\n", cc->available_rooms.rooms[i].room_number, cc->available_rooms.rooms[i].num_connected_clients, people_person);
	}
	printf("Choose the room number or type [new] to create a new room: ");
}

int initiate_server_handshake(int sockfd, unsigned char* handshake_buffer)
{
	int n = send(sockfd, handshake_buffer, sizeof(ConnectionRequest), 0);
	if (n < 0) error("ERROR writing to socket");

	// ConnectionConfirmation cc;
	// n = recv(sockfd, &cc, sizeof(cc), 0);
	// if (n < 0) error("ERROR reading from socket");

	// print_connection_confirmation(&cc);

	return 0;
}

// given command line arguments, populates a serialized connection request buffer
void prepare_connection_request(int argc, char* room_arg, unsigned char* buffer) {
	ConnectionRequest cr;
	init_connection_request(argc, room_arg, &cr);
	if (serialize_connection_request(&cr, buffer) != sizeof(ConnectionRequest)) {
		error("ERROR: Failed to serialize connection request");
	}
}

// prints the serialized connection request buffer as a ConnectionRequest struct
void print_serialized_connection_request(unsigned char* buffer) {
	printf("Printing serialized connection request...\n");
	ConnectionRequest cr;
	memcpy(&cr, buffer, sizeof(ConnectionRequest));
	print_connection_request(&cr);
}

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