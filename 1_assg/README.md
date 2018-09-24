#NETWORK SYSTEMS Assignment 1: UDP Sockets

Server:
server folder contains Makefile and udp_server.c files
Build: make
Clean: make clean

Client:
client folder contains Makefile and udp_client.c files
Build: make
Clean: make clean


Have implemented a reliable file transfer application using UDP Sockets.
I have used Stop and Wait method and built-in timeout for recvfrom API for reliability.
Message packet for reliable packet contains sequence number, no of bytes and buffer data (1024 bytes max).
** Please don't use undefined filenames. **

Timeout period: 20 msec


Functions supported:

1. GET File from server
usage: get <filename>
Downloads the file from the server reliably.

2. PUT File into server
usage: put <filename>
Uploads the file in the server reliably.

3. DELETE File from server
usage: delete <filename>
Deletes the file using rf -f command executed using system() API from the server.

4. LIST Files from server
uasge: ls
Lists the files on the server using ls -la command and piping the output to filelist.txt and transferring it back to host.

5. Exit the server
uasge: exit
Exits the server and client gracefully.

6. MD5SUM of file from the client and server
usage: md5sum <filename>
Prints the MD5 checksum of the file on server and client. Helpful for testing the file's authenticity.

