# Network Programming - C-Socket Chatroom

Our program can successfully host multiple chat rooms without any hiccups, and clients can join with any of the three methods listed in the project requirements, which are creating a new room, joining an existing room, or selecting a room after being given a menu.

To compile the server and client, simply run make from the root directory of the submission.

The server can be run by executing ./main_server from the root directory of the submission. Any client can be run by executing ./main_client <ip-address> <room-number/”new”>, or by selecting a room from the menu, ./main_client <ip-address>.

For example, if a user wanted to join room 2, they would execute ./main_client 127.0.0.1 2

The clients upon joining will be prompted to enter their username. After entering their username, they will join the specified room, join a newly created room, or be given a menu to select a room depending on what was specified in the command line arguments. After joining the room, the user will be able to freely communicate with any other user connected to the same chatroom. No cross room communication is supported, and is prevented by keeping a separate list of clients for every room.
