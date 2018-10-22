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
#include <sys/time.h>

#define TIMEOUT		(900000)	// 200 msec = 200,000 usec
#define TIMEOUT_0	(0)

/* Turn on timeout for recvfrom()*/
#define TIMEOUT_ON()	(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, \
							(char*)&timeout, sizeof(struct timeval)))

/* Turn off timeout for recvfrom()*/
#define TIMEOUT_OFF()	(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, \
							(char*)&timeout_0, sizeof(struct timeval)))

#define BUFSIZE 1024

/* structure for storing command and filename */
typedef struct request
{
    char cmd[32];
    char file[64];
}Request_t;

/* Structure for message packet: sequence number, no of bytes, buffer. */
typedef struct msg_pkt
{
	int seq;
	int nbytes;
	char buffer[BUFSIZE];
}msg_pkt_t;


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
    int pseq;
    size_t len;
    int ls_flag = 0;
    off_t offset;
    Request_t cmd_msg;
    msg_pkt_t msgbuff;

    /* Timer structure for timeout */
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT;

    struct timeval timeout_0;
    timeout_0.tv_sec = 0;
    timeout_0.tv_usec = TIMEOUT_0;


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

    /* Turn off timeout for recvfrom() */
    TIMEOUT_OFF();

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

        /* receive cmd msg (command and filename) from host */
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

        /* ACK to host */
        n = sendto(sockfd, "OK", 3, 0, 
               (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0) 
            error("ERROR in sendto");

    label1:

        /**** Case 1: GET File from server ****/
        if((strcmp(cmd_msg.cmd, "get")) == 0)
        { 
            /* Read the file in chunck of 1024 bytes and transfer it */
            fd = open(cmd_msg.file, O_RDONLY);    
            
            if (fd == -1)
                error("Error opening file\n");

            /* send file size */
            len = file_size(fd);
            char * buffer = (char *)malloc(len);

            /* Send filesize */
            n = sendto(sockfd, (void *)&len, sizeof(size_t), 0,
                        (struct sockaddr *)&clientaddr, clientlen);
            if (n < 0) 
                error("ERROR in sendto");

            /* send the file in packet size of 1024 bytes */
            lseek(fd, 0, SEEK_SET);
            offset = 0;
            pseq = 0;

            /* Turn timeout on for recvfrom() */
            TIMEOUT_ON();

            while (offset < len)
            {
                size_t readnow;
                readnow = read(fd, buffer + offset, BUFSIZE);

                if (readnow < 0)
                {
                    close (fd);
                    error("Read Unsuccessful\n");
                }

                /* Form the packet to transfer */
                pseq++;            
                bzero((void *)&msgbuff, sizeof(msg_pkt_t));
                msgbuff.seq = pseq;
                msgbuff.nbytes = readnow;
                memcpy((void *)msgbuff.buffer, (void*)(buffer + offset), readnow);

            	label_rep1:

                /* Send the packet */
                n = sendto(sockfd, (void *)&msgbuff, sizeof(msg_pkt_t), 0,
                			(struct sockaddr *) &clientaddr, clientlen);

                if (n < 0) 
                    error("ERROR in sendto");

                /* Wait for ACK */
                bzero((void *)&msgbuff, sizeof(msg_pkt_t));
        		n = recvfrom(sockfd, &msgbuff, sizeof(msg_pkt_t), 0,
             				(struct sockaddr *) &clientaddr, &clientlen);

        		/* If timeout then repeat at label label_rep1 */
                if(n >= 0 && (msgbuff.seq == pseq) && 
        					((strcmp(msgbuff.buffer, "ACK")) == 0))
        		{
        			offset += msgbuff.nbytes;
        			continue;
        		}
        		else
        		{
        			goto label_rep1;
        		}

            }

            TIMEOUT_OFF();
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
            /* Create a new file and write the contents in chunck of 1024 bytes */
            fd = open(cmd_msg.file, O_CREAT | O_RDWR, S_IRWXU);

    		if (fd == -1)
            	error("Error opening file\n");

            /* Receive file size */
            n = recvfrom(sockfd, (void *)&len, sizeof(size_t), 0, 
                        (struct sockaddr *) &clientaddr, &clientlen);
            if (n < 0) 
                error("ERROR in recvfrom");

            /* send ACK */
            n = sendto(sockfd, "OK", 3, 0,
                        (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) 
                error("ERROR in sendto");

	    	char * buffer = (char *)malloc(len);

	    	offset = 0;
	    	pseq = 0;

	    	while(offset < len)
	    	{

                // bzero((void *)&msgbuff, sizeof(msg_pkt_t));
	    		/* Reaceive new message packet */
                n = recvfrom(sockfd, &msgbuff, sizeof(msg_pkt_t), 0, 
	    					(struct sockaddr *) &clientaddr, &clientlen);
	    		if (n < 0) 
	    			error("ERROR in recvfrom");

                printf("Seq no: %d, msg_seq: %d\n", pseq, msgbuff.seq);

	    		/* Check for next sequence number/packet or else repeat*/
                if((msgbuff.seq - pseq) == 1)
	    		{
	    			
                    memcpy((void*)(buffer + offset),(void *)msgbuff.buffer, msgbuff.nbytes);
	    			/* write the contents to the file */
                    write(fd, buffer + offset, msgbuff.nbytes);
	    			offset += msgbuff.nbytes;

	    			strcpy(msgbuff.buffer, "ACK");
                    //send ACK
                    n = sendto(sockfd, (void *)&msgbuff, sizeof(msg_pkt_t), 0,
                            (struct sockaddr *) &clientaddr, clientlen);

                    if (n < 0) 
                        error("ERROR in sendto");
	    			
					pseq++;
	    			continue;
	    		}
                else if((msgbuff.seq - pseq) == 0 || (pseq > msgbuff.seq))
                {
                    strcpy(msgbuff.buffer, "ACK");

                    n = sendto(sockfd, (void *)&msgbuff, sizeof(msg_pkt_t), 0,
                            (struct sockaddr *) &clientaddr, clientlen);

                    if (n < 0) 
                        error("ERROR in sendto");

                    pseq = msgbuff.seq + 1;
                    
                    continue;
                }
	    		else
	    		{
	    			continue;
	    		}

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

            /* execute command */
            system(buf);
        }

        /**** Case 4: LIST Files from server ****/
        else if((strcmp(cmd_msg.cmd, "ls")) == 0)
        {
            printf("[Server] Sending file list\n");

            /* execute command */
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

            /* execute command */
            system(buf);
    	}

        else
            printf("[Server] Command: %s undefined!\n", cmd_msg.cmd);

        continue;
    }

    close(sockfd);
    return 0;

}
