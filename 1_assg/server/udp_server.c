/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>  //directory

#define BUFSIZE 1024

typedef struct request
{
    char cmd[32];
    char file[64];
}Request_t;


size_t file_size(int fd)
{
    struct stat buf;
    fstat(fd, &buf);
    return (size_t)buf.st_size;
}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char **argv) 
{
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buf */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */

    int fd;
    size_t len;
    int ls_flag = 0;
    off_t offset;
    Request_t cmd_msg;

    /* 
    * check command line arguments 
    */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /* 
    * socket: create the parent socket 
    */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
    * us rerun the server immediately after we kill it; 
    * otherwise we have to wait about 20 secs. 
    * Eliminates "ERROR on binding: Address already in use" error. 
    */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                (const void *)&optval , sizeof(int));

    /*
    * build the server's Internet address
    */
    /* similar to memset zero */
    bzero((char *) &serveraddr, sizeof(serveraddr));    
    serveraddr.sin_family = AF_INET;    // IPv4
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
    * bind: associate the parent socket with a port 
    */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    printf("[Server] Server runnning...\n");

    /* 
    * main loop: wait for a datagram, then echo it
    */
    clientlen = sizeof(clientaddr);
    
    while (1) 
    {

        ls_flag = 0;
        bzero(buf, BUFSIZE);
        printf("\n");

        /* receive cmd msg from host */
        n = recvfrom(sockfd, &cmd_msg, sizeof(Request_t), 0,
             		(struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        printf("[Server] CMD: %s, File: %s\n", cmd_msg.cmd, cmd_msg.file);

        /* get host address */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
                  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        // printf("server received datagram from %s (%s)\n", 
        //    hostp->h_name, hostaddrp);

        /* ACK to host */
        n = sendto(sockfd, "OK", 3, 0, 
               (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
            error("ERROR in sendto");

    label1:

        /**** Case 1: GET File from server ****/
        if((strcmp(cmd_msg.cmd, "get")) == 0)
        {
            fd = open(cmd_msg.file, O_RDONLY);    
            
            if (fd == -1)
                error("Error opening file\n");

            /* send file size */
            len = file_size(fd);
            char * buffer = (char *)malloc(len);

            n = sendto(sockfd, (void *)&len, sizeof(size_t), 0,
                        (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0) 
                error("ERROR in sendto");

            /* send the file in packet size of 1024 bytes */
            lseek(fd, 0, SEEK_SET);
            offset = 0;

            while (offset < len)
            {
                size_t readnow;
                readnow = read(fd, buffer + offset, BUFSIZE);

                if (readnow < 0)
                {
                    close (fd);
                    error("Read Unsuccessful\n");
                }

                n = sendto(sockfd, (void *)(buffer + offset), readnow, 0,
                            (struct sockaddr *) &clientaddr, clientlen);

                if (n < 0) 
                    error("ERROR in sendto");

                offset += n;
            }
            free(buffer);
            close(fd);

            if(ls_flag == 1)
                system("rm -rf file_list.txt");
            else
            	printf("[Server] File transferred\n");
        }

        /**** Case 2: PUT File into server ****/
        else if((strcmp(cmd_msg.cmd, "put")) == 0)
        {
            fd = open(cmd_msg.file, O_CREAT | O_RDWR, S_IRWXU);

    		if (fd == -1)
            	error("Error opening file\n");
			
			n = recvfrom(sockfd, (void *)&len, sizeof(size_t), 0, 
						(struct sockaddr *) &clientaddr, &clientlen);
	    	if (n < 0) 
	    		error("ERROR in recvfrom");

	    	char * buffer = (char *)malloc(len);

	    	// printf("file size = %ld\n", len);

	    	offset = 0;

	    	while(offset < len)
	    	{
	    		n = recvfrom(sockfd, buffer + offset, BUFSIZE, 0, 
	    					(struct sockaddr *) &clientaddr, &clientlen);
	    		if (n < 0) 
	    			error("ERROR in recvfrom");

	    		// printf("Writing! %ld\n", offset);

	    		write(fd, buffer + offset, n);

	    		offset += n;

	    	}

	    	close(fd);
	    	free(buffer);

	    	printf("[Server] File transferred\n");
        }

        /**** Case 3: DELETE File from server ****/
        else if((strcmp(cmd_msg.cmd, "delete")) == 0)
        {
            bzero(buf, BUFSIZE);
            sprintf(buf, "rm -f %s", cmd_msg.file);
            printf("[Server] Deleting file %s\n", cmd_msg.file);

            system(buf);
        }

        /**** Case 4: LIST Files from server ****/
        else if((strcmp(cmd_msg.cmd, "ls")) == 0)
        {
            printf("[Server] Sending file list\n");

            system("ls -la >> file_list.txt");

            strcpy(cmd_msg.cmd, "get");
            strcpy(cmd_msg.file, "file_list.txt");
            ls_flag = 1; 

            goto label1;

            
        } 

        /**** Case 5: Exit the server ****/  
        else if((strcmp(cmd_msg.cmd, "exit")) == 0)
        {
            printf("[Server] Closing server...\n");
            break;
        }

        /**** Case 6: MD5SUM of file from the server ****/ 
    	else if((strcmp(cmd_msg.cmd, "md5sum")) == 0)
    	{
    		bzero(buf, BUFSIZE);
            sprintf(buf, "md5sum %s", cmd_msg.file);
            printf("[Server] MD5SUM of file %s on server:\n\n", cmd_msg.file);

            system(buf);
    	}

        else
            printf("Def:\n");

        continue;


        /*
         * recvfrom: receive a UDP datagram from a client
         */
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
        	 (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        /* 
         * gethostbyaddr: determine who sent the datagram
         */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
        		  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server received datagram from %s (%s)\n", 
           hostp->h_name, hostaddrp);
        printf("server received %ld/%d bytes: %s\n", strlen(buf), n, buf);

        /* 
         * sendto: echo the input back to the client 
         */
        n = sendto(sockfd, buf, strlen(buf), 0, 
               (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
            error("ERROR in sendto");
    }

    close(sockfd);
    return 0;

}
