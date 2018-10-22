#NETWORK SYSTEMS Assignment 2: Webserver

Have implemented a webserver over TCP connection that receives requests from the clients and sends the processed result back to the client as a response to the command.
The server handles all the client requests concurrently using fork() system call.

supports HTTP/1.1 and HTTP/1.0

Makefile instruction:

To build the executable file : $ make all 

To clean : $ make clean

Execution instruction:

./tcpServer <port number>

Webpage (client)
http://localhost:<port number>/index.html 




