CC = gcc
CFLAGS = -Wall -Wextra -g
OBJ_SERVER = main_server.o util.o handshake.o file_transfer.o connection_status_monitor.o socket_setup.o
OBJ_CLIENT = main_client.o util.o handshake.o connection_status_monitor.o socket_setup.o file_transfer.o

all: main_server main_client

main_server: $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o $@ $(OBJ_SERVER)

main_client: $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $@ $(OBJ_CLIENT)

main_server.o: main_server.c handshake.h util.h file_transfer.h
	$(CC) $(CFLAGS) -c main_server.c

main_client.o: main_client.c handshake.h util.h connection_status_monitor.h file_transfer.h
	$(CC) $(CFLAGS) -c main_client.c

util.o: util.c util.h
	$(CC) $(CFLAGS) -c util.c

handshake.o: handshake.c handshake.h
	$(CC) $(CFLAGS) -c handshake.c

connection_status_monitor.o: connection_status_monitor.c connection_status_monitor.h
	$(CC) $(CFLAGS) -c connection_status_monitor.c

socket_setup.o: socket_setup.c socket_setup.h
	$(CC) $(CFLAGS) -c socket_setup.c

file_transfer.o: file_transfer.c file_transfer.h
	$(CC) $(CFLAGS) -c file_transfer.c

clean:
	rm -f *.o main_server main_client
