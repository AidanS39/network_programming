#include "handshake.h"

// TODO: split this into two or 3 files (severside, clientside, shared)

/*========================================= CONNECTION REQUEST ==========================================*/

// sends a ConnectionRequest struct to the server and receives a ConnectionConfirmation struct in return
// until the server confirms or denies the connection
int perform_handshake(int sockfd, struct sockaddr_in* serv_addr, Buffer* cr_buffer, char* username)
{
	int n = send(sockfd, cr_buffer->data, cr_buffer->size, 0);
	if (n < 0) error("ERROR writing to socket");

	// listen in a loop until you get a successful connection confirmation
	ConnectionConfirmation cc;
	Buffer cc_buffer;
	init_buffer(&cc_buffer, sizeof(ConnectionConfirmation));
	while (1) {
		n = recv(sockfd, cc_buffer.data, cc_buffer.size, 0);
		if (n < 0) { // error reading from socket
			cleanup_buffer(&cc_buffer);
			error("ERROR reading from socket");
		}
		deserialize_connection_confirmation(&cc, &cc_buffer);
		// print_connection_confirmation(&cc);
		if (cc.status == CONFIRMATION_SUCCESS) {
			break;
		} else if (cc.status == CONFIRMATION_SUCCESS_NEW) {
			// is this getting the server or client ipv4?
			printf("Connected to %s with new room number %d\n",
			inet_ntoa(serv_addr->sin_addr),
			cc.connected_room.room_number);
			break;
		} else if (cc.status == CONFIRMATION_PENDING) {
			handle_pending_confirmation(sockfd, &cc, username);
		} else {
			cleanup_buffer(&cc_buffer);
			error("ERROR: server refused connection");
		}
		memset(cc_buffer.data, 0, cc_buffer.size);
	}
	cleanup_buffer(&cc_buffer);

	return 0;
}


// given command line arguments, populates a serialized connection request buffer
void prepare_connection_request(int argc, char* room_arg, Buffer* cr_buffer,
char* username) {
	// printf("Preparing connection request...\n");
	// printf("size of cr_buffer: %zu\n", cr_buffer->size);
	// printf("size of ConnectionRequest: %zu\n", sizeof(ConnectionRequest));
	ConnectionRequest cr;
	ConnectionRequestType type = -1;
	int room_number = UNINITIALIZED_ROOM_NUMBER;
	if (argc == 2) {
		type = SELECT_ROOM;
	} else if (argc == 3 && room_arg != NULL) {
		// if the user wants to create a new room, set the type to CREATE_NEW_ROOM
		// otherwise they should enter a room number to join
		if (strcmp(room_arg, CREATE_NEW_ROOM_COMMAND) == 0) {
			type = CREATE_NEW_ROOM;
		} else {
			type = JOIN_ROOM;
			room_number = strtol(room_arg, NULL, 10);
		}
	} else {
		error("ERROR: Invalid number of arguments");
	}

	init_connection_request_struct(type, room_number, &cr, username);
	if (serialize_connection_request(cr_buffer, &cr) != sizeof(ConnectionRequest)) {
		error("ERROR: Failed to serialize connection request");
	}
}

// populates a ConnectionRequest struct
// NOTE: you technically don't need type, but it makes things more explicit than if I just checked selection_arg being NULL
int init_connection_request_struct(ConnectionRequestType type, int room_number,
ConnectionRequest *cr, char* username)
{
	memset(cr, 0, sizeof(ConnectionRequest));

	switch (type) {
		case SELECT_ROOM: // display rooms and ask user to choose one
			cr->type = SELECT_ROOM;
			cr->room_number = UNINITIALIZED_ROOM_NUMBER;
			break;
		case CREATE_NEW_ROOM: // join room with specified room number or new room
			cr->type = CREATE_NEW_ROOM;
			cr->room_number = UNINITIALIZED_ROOM_NUMBER;
			break;
		case JOIN_ROOM:
			cr->type = JOIN_ROOM;
			cr->room_number = room_number;
			break;
		case CANCEL_HANDSHAKE: // canceling handshake
			cr->type = CANCEL_HANDSHAKE;
			cr->room_number = UNINITIALIZED_ROOM_NUMBER;
			break;
		default:
			error("ERROR: Invalid number of arguments");
	}

	// set the username from global variable passed in
	strcpy(cr->username, username);

	return 0;
}

// if the client sends a request without a room number or asking for a new room, the server will send back a pending confirmation
// this function handles that by displaying the prompt for a user to select a room
// and sending the user's choice back to the server
int handle_pending_confirmation(int sockfd, ConnectionConfirmation* cc, char* username) {
	// display the prompt for a user to select a room
	print_room_selection_prompt(cc);
	
	// get the user's choice
	char room_arg[MAX_USERNAME_LEN];
	fgets(room_arg, MAX_USERNAME_LEN - 1, stdin);
	// translate room choice to valid room number
	trim_whitespace(room_arg);

	ConnectionRequestType type;
	int room_number;
	if (strcmp(room_arg, CREATE_NEW_ROOM_COMMAND) == 0) {
		type = CREATE_NEW_ROOM;
		room_number = UNINITIALIZED_ROOM_NUMBER;
	} else if (is_number(room_arg)) { // if the room arg is a number
		type = JOIN_ROOM;
		room_number = strtol(room_arg, NULL, 10);
	} else {
		type = CANCEL_HANDSHAKE;
		room_number = UNINITIALIZED_ROOM_NUMBER;
		printf("Your room choice is invalid. Disconnecting...\n");
	}
	// build a new connection request with the user's choice
	ConnectionRequest cr;
	init_connection_request_struct(type, room_number, &cr, username);
	Buffer cr_buffer;
	init_buffer(&cr_buffer, sizeof(ConnectionRequest));
	serialize_connection_request(&cr_buffer, &cr);

	// send the new connection request to the server
	send(sockfd, cr_buffer.data, cr_buffer.size, 0);
	
	cleanup_buffer(&cr_buffer);

	return 0;
}


/* ---------------------------------------- SERIALIZATION ---------------------------------------- */

// Takes a ConnectionRequest and serializes it into a buffer of the same size
// NOTE: structs are packed already so no padding
size_t serialize_connection_request(Buffer* handshake_buffer, ConnectionRequest *cr) {
	assert(handshake_buffer->size == sizeof(ConnectionRequest));
	size_t offset = 0;

	// serialize username (string is already array of chars so no need to worry about endianness)
	memcpy(handshake_buffer->data + offset, cr->username, sizeof(cr->username));
	offset += sizeof(cr->username);

	// serialize type (enum int)
	int32_t type_net = htonl((int32_t) cr->type);
	memcpy(handshake_buffer->data + offset, &type_net, sizeof(type_net));
	offset += sizeof(type_net);

	// serialize room number
	int32_t room_number_net = htonl(cr->room_number);
	memcpy(handshake_buffer->data + offset, &room_number_net, sizeof(room_number_net));
	offset += sizeof(room_number_net);

	return offset;
}

// takes in a buffer that has presumably been serialized, converts it to a ConnectionRequest struct and prints it
// NOTE: useful for seeing the htonl bytes
void print_serialized_connection_request(Buffer* cr_buffer) {
	printf("Printing serialized connection request...\n");
	ConnectionRequest cr;
	memcpy(&cr, cr_buffer->data, sizeof(ConnectionRequest));
	print_connection_request_struct(&cr);
}



/* ---------------------------------------- DESERIALIZATION ---------------------------------------- */

// Takes a buffer that represents a serialized ConnectionRequest struct and deserializes it into a ConnectionRequest struct
size_t deserialize_connection_request(ConnectionRequest *cr, Buffer* cr_buffer) {
	assert(cr_buffer->size == sizeof(ConnectionRequest));

	size_t offset = 0;

	// deserialize username
	memcpy(cr->username, cr_buffer->data + offset, sizeof(cr->username));
	offset += sizeof(cr->username);

	// deserialize type
	int32_t type_net = 0;
	memcpy(&type_net, cr_buffer->data + offset, sizeof(type_net));
	cr->type = (ConnectionRequestType) ntohl(type_net);
	offset += sizeof(type_net);

	// deserialize room number
	int32_t room_number_net = 0;
	memcpy(&room_number_net, cr_buffer->data + offset, sizeof(room_number_net));
	cr->room_number = ntohl(room_number_net);
	offset += sizeof(room_number_net);

	return offset;
}

// prints out members of a ConnectionRequest struct
void print_connection_request_struct(ConnectionRequest *cr)
{
	printf("Connection request username: %s\n", cr->username);
	printf("Connection request type: %d\n", cr->type);
	printf("Connection request room number: %d\n", cr->room_number);
}



/* ---------------------------------------- MOCKING ---------------------------------------- */

// quick mocking of the ConnectionConfirmation struct the server would send to
// the client in response to a ConnectionRequest
ConnectionConfirmation mock_server_connection_confirmation()
{
	ConnectionConfirmation cc;
	cc.status = CONFIRMATION_SUCCESS;
	cc.connected_room.room_number = 1;
	cc.connected_room.num_connected_clients = 3;

	cc.available_rooms.rooms[0].room_number = 1;
	cc.available_rooms.rooms[0].num_connected_clients = 5;

	cc.available_rooms.rooms[1].room_number = 2;
	cc.available_rooms.rooms[1].num_connected_clients = 10;

	cc.available_rooms.rooms[2].room_number = 3;
	cc.available_rooms.rooms[2].num_connected_clients = 1;

	return cc;
}

/*========================================= CONNECTION CONFIRMATION ==========================================*/

/* ---------------------------------------- SERIALIZATION ---------------------------------------- */
// serializes a ConnectionConfirmation struct into a buffer. During initial
// handshake, the server sends this information to the client when it receives a ConnectionRequest
size_t serialize_connection_confirmation(Buffer* cc_buffer, ConnectionConfirmation *cc) {
	assert(cc_buffer->size == sizeof(ConnectionConfirmation));

	size_t offset = 0;
	// serialize status
	int32_t status_net = htonl((int32_t) cc->status);
	memcpy(cc_buffer->data + offset, &status_net, sizeof(status_net));
	offset += sizeof(status_net);

	// serialize connected room
	Buffer cr_buffer;
	init_buffer(&cr_buffer, sizeof(HandshakeRoomDescription));
	assert(serialize_handshake_room_description(&cr_buffer, &cc->connected_room) == sizeof(HandshakeRoomDescription));
	memcpy(cc_buffer->data + offset, cr_buffer.data, sizeof(HandshakeRoomDescription));
	cleanup_buffer(&cr_buffer);
	offset += sizeof(HandshakeRoomDescription);

	// serialize available rooms
	Buffer ar_buffer;
	init_buffer(&ar_buffer, sizeof(HandshakeAvailableRooms));
	assert(serialize_handshake_available_rooms(&ar_buffer, &cc->available_rooms) == sizeof(HandshakeAvailableRooms));
	memcpy(cc_buffer->data + offset, ar_buffer.data, sizeof(HandshakeAvailableRooms));
	cleanup_buffer(&ar_buffer);
	offset += sizeof(HandshakeAvailableRooms);

	return offset;
}

// serializes a HandshakeAvailableRooms struct into a buffer (for building out the ConnectionConfirmation buffer)
size_t serialize_handshake_available_rooms(Buffer* har_buffer, HandshakeAvailableRooms *har) {
	assert(har_buffer->size == sizeof(HandshakeAvailableRooms));

	size_t offset = 0;

	// serialize num available rooms
	int32_t num_rooms_net = htonl(har->num_rooms);
	memcpy(har_buffer->data + offset, &num_rooms_net, sizeof(num_rooms_net));
	offset += sizeof(num_rooms_net);
	
	// serialize room descriptions
	for (int i = 0; i < MAX_ROOMS; i++) {
		Buffer hrd_buffer;
		init_buffer(&hrd_buffer, sizeof(HandshakeRoomDescription));
		assert(serialize_handshake_room_description(&hrd_buffer, &har->rooms[i]) == sizeof(HandshakeRoomDescription));
		memcpy(har_buffer->data + offset, hrd_buffer.data, sizeof(HandshakeRoomDescription));
		cleanup_buffer(&hrd_buffer);
		offset += sizeof(HandshakeRoomDescription);
	}

	return offset;
}

// serializes a HandshakeRoomDescription struct into a buffer (for building out the HandshakeAvailableRooms and ConnectionConfirmation buffers)
size_t serialize_handshake_room_description(Buffer* hrd_buffer, HandshakeRoomDescription *hrd) {
	assert(hrd_buffer->size == sizeof(HandshakeRoomDescription));

	size_t offset = 0;

	// serialize room number
	int32_t room_number_net = htonl(hrd->room_number);
	memcpy(hrd_buffer->data + offset, &room_number_net, sizeof(room_number_net));
	offset += sizeof(room_number_net);

	// serialize num connected clients
	int32_t num_connected_clients_net = htonl(hrd->num_connected_clients);
	memcpy(hrd_buffer->data + offset, &num_connected_clients_net, sizeof(num_connected_clients_net));
	offset += sizeof(num_connected_clients_net);

	return offset;
}

// takes in a buffer that has presumably been serialized, converts it to a ConnectionConfirmation struct and prints it
// NOTE: useful for seeing the htonl bytes
void print_serialized_connection_confirmation(Buffer* cc_buffer) {
	ConnectionConfirmation cc;
	memcpy(&cc, cc_buffer->data, sizeof(ConnectionConfirmation));

	printf("Printing serialized connection confirmation...\n");
	print_connection_confirmation(&cc);
}



/* ---------------------------------------- DESERIALIZATION ---------------------------------------- */

// Takes a buffer that represents a serialized ConnectionConfirmation struct and deserializes it into a ConnectionConfirmation struct
// TODO: I don't love the complexity here. Copying all the pieces into temporary buffers...
size_t deserialize_connection_confirmation(ConnectionConfirmation *cc, Buffer* cc_buffer) {
	assert(cc_buffer->size == sizeof(ConnectionConfirmation));

	size_t offset = 0;


	// printf("Deserializing connection confirmation... beginning\n");
	// print_hex(cc_buffer->data, sizeof(ConnectionConfirmation));

	// deserialize status
	int32_t status_net = 0;
	memcpy(&status_net, cc_buffer->data + offset, sizeof(status_net));
	cc->status = (ConfirmationStatus) ntohl(status_net);
	offset += sizeof(status_net);


	// TODO: pull these out into a function
	// deserialize connected room
	// Make a smaller temporary buffer that just contains the connected room description
	Buffer hrd_buffer;
	init_buffer(&hrd_buffer, sizeof(HandshakeRoomDescription));
	// copy just the connected room description into the temporary buffer
	memcpy(hrd_buffer.data, cc_buffer->data + offset, sizeof(HandshakeRoomDescription));
	assert(deserialize_handshake_room_description(&cc->connected_room, &hrd_buffer) == sizeof(HandshakeRoomDescription));
	offset += sizeof(HandshakeRoomDescription);
	cleanup_buffer(&hrd_buffer);


	// deserialize available rooms
	// Make smaller temporary buffer that just contains the available rooms
	Buffer ar_buffer;
	init_buffer(&ar_buffer, sizeof(HandshakeAvailableRooms));
	// copy just the available rooms into the temporary buffer
	memcpy(ar_buffer.data, cc_buffer->data + offset, sizeof(HandshakeAvailableRooms));
	assert(deserialize_handshake_available_rooms(&cc->available_rooms, &ar_buffer) == sizeof(HandshakeAvailableRooms));
	offset += sizeof(HandshakeAvailableRooms);
	cleanup_buffer(&ar_buffer);

	return offset;
}

// Takes a buffer that represents a serialized HandshakeAvailableRooms struct and deserializes it into a HandshakeAvailableRooms struct
size_t deserialize_handshake_available_rooms(HandshakeAvailableRooms *har, Buffer* har_buffer) {
	assert(har_buffer->size == sizeof(HandshakeAvailableRooms));

	size_t offset = 0;
	
	// deserialize num available rooms
	int32_t num_rooms_net = 0;
	memcpy(&num_rooms_net, har_buffer->data + offset, sizeof(num_rooms_net));
	har->num_rooms = ntohl(num_rooms_net);
	offset += sizeof(num_rooms_net);
	
	// deserialize room descriptions
	for (int i = 0; i < MAX_ROOMS; i++) {
		// For each room you need to make a smaller temporary buffer to hold the room description
		Buffer hrd_buffer;
		init_buffer(&hrd_buffer, sizeof(HandshakeRoomDescription));
		// copy just the room description into the temporary buffer
		memcpy(hrd_buffer.data, har_buffer->data + offset, sizeof(HandshakeRoomDescription));
		assert(deserialize_handshake_room_description(&har->rooms[i], &hrd_buffer) == sizeof(HandshakeRoomDescription));
		offset += sizeof(HandshakeRoomDescription);
		cleanup_buffer(&hrd_buffer);
	}

	return offset;
}	

// Takes a buffer that represents a serialized HandshakeRoomDescription struct and deserializes it into a HandshakeRoomDescription struct
size_t deserialize_handshake_room_description(HandshakeRoomDescription *hrd, Buffer* hrd_buffer) {
	assert(hrd_buffer->size == sizeof(HandshakeRoomDescription));

	size_t offset = 0;

	// deserialize room number
	int32_t room_number_net = 0;
	memcpy(&room_number_net, hrd_buffer->data + offset, sizeof(room_number_net));
	hrd->room_number = ntohl(room_number_net);
	offset += sizeof(room_number_net);

	// deserialize num connected clients
	int32_t num_connected_clients_net = 0;
	memcpy(&num_connected_clients_net, hrd_buffer->data + offset, sizeof(num_connected_clients_net));
	hrd->num_connected_clients = ntohl(num_connected_clients_net);
	offset += sizeof(num_connected_clients_net);

	return offset;
}


// prints out members of a ConnectionConfirmation struct
void print_connection_confirmation(ConnectionConfirmation *cc) {
	// status
	printf("Connection confirmation status: %d\n", cc->status);

	// connected room
	printf("Connection confirmation connected room: %d\n", cc->connected_room.room_number);
	printf("Connection confirmation connected room num connected clients: %d\n", cc->connected_room.num_connected_clients);

	// available rooms
	printf("Connection confirmation num available rooms: %d\n", cc->available_rooms.num_rooms);
	// NOTE: you can't use the available rooms' num rooms because it may be serialized
	for (int i = 0; i < MAX_ROOMS; i++) {
		// room description
		printf("Room number: %d\tNumber of connected clients: %d\n",
				cc->available_rooms.rooms[i].room_number,
				cc->available_rooms.rooms[i].num_connected_clients);
	}
}


/* ---------------------------------------- MISC ---------------------------------------- */

// prints a prompt to the user asking them to select a room from the available rooms
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
