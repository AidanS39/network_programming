CC = gcc
CFLAGS = -Wall -Wextra -g
OBJ_SERVER = chat_server_full.o util.o handshake.o
OBJ_CLIENT = chat_client_full.o util.o handshake.o connection_status_monitor.o socket_setup.o

all: chat_server_full chat_client_full

chat_server_full: $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o $@ $(OBJ_SERVER)

chat_client_full: $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $@ $(OBJ_CLIENT)

chat_server_full.o: chat_server_full.c handshake.h util.h
	$(CC) $(CFLAGS) -c chat_server_full.c

chat_client_full.o: chat_client_full.c handshake.h util.h connection_status_monitor.h
	$(CC) $(CFLAGS) -c chat_client_full.c

util.o: util.c util.h
	$(CC) $(CFLAGS) -c util.c

handshake.o: handshake.c handshake.h
	$(CC) $(CFLAGS) -c handshake.c

connection_status_monitor.o: connection_status_monitor.c connection_status_monitor.h
	$(CC) $(CFLAGS) -c connection_status_monitor.c

socket_setup.o: socket_setup.c socket_setup.h
	$(CC) $(CFLAGS) -c socket_setup.c

clean:
	rm -f *.o chat_server_full chat_client_full