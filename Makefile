all:
	gcc -o chat_server_full chat_server_full.c -lpthread
	gcc -o chat_client_full chat_client_full.c -lpthread

clean:
	rm chat_server_full chat_client_full

