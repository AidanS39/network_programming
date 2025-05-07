#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#define MAX_USERNAME_LEN 32

typedef enum _ConnectionRequestType {
	JOIN_ROOM,
	CREATE_NEW_ROOM,
	SELECT_ROOM
} ConnectionRequestType;

typedef struct _ConnectionRequest {
	char username[MAX_USERNAME_LEN];
	ConnectionRequestType type;
	int room_number;
} ConnectionRequest;

typedef enum _ConfirmationStatus {
	CONFIRMATION_SUCCESS,
	CONFIRMATION_PENDING,
	CONFIRMATION_FAILURE
} ConfirmationStatus;

typedef struct _HandshakeRoomDescription {
	int room_number;
	int num_connected_clients;
} HandshakeRoomDescription;

typedef struct _HandshakeRoomsInfo {
	int num_rooms;
	HandshakeRoomDescription* rooms;
} HandshakeRoomsInfo;

typedef struct _ConnectionConfirmation {
	ConfirmationStatus status;
	HandshakeRoomsInfo rooms_info;
} ConnectionConfirmation;

#endif


