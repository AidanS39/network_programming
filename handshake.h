#ifndef HANDSHAKE_H
#define HANDSHAKE_H
#include <stdint.h>

#define MAX_USERNAME_LEN 32
#define MAX_ROOMS 32

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

#pragma pack(pop)

#endif


