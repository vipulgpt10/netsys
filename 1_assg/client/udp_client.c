/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h> 

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
    exit(0);
}

int main(int argc, char **argv) 
{
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent * server;
    char *hostname;
    char buf[BUFSIZE];
    char msg[100];

    int fd;
    int ls_flag = 0;
    size_t len;
    off_t offset;
    Request_t cmd_msg;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"[Client] usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    while(1)
    {
	    ls_flag = 0;

	    /* socket: create a new socket for each request */
	    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	    if (sockfd < 0) 
	        error("[Client] ERROR opening socket");

	    /* gethostbyname: get the server's DNS entry */
	    server = gethostbyname(hostname);
	    if (server == NULL) {
	        fprintf(stderr,"[Client] ERROR, no such host as %s\n", hostname);
	        exit(0);
	    }

	    /* build the server's Internet address */
	    bzero((char *) &serveraddr, sizeof(serveraddr));
	    serveraddr.sin_family = AF_INET;
	    bcopy((char *)server->h_addr, 
		  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	    serveraddr.sin_port = htons(portno);

	    serverlen = sizeof(serveraddr);

	    
	    /* available APIs to users */
	    printf("\n[Client] List of commands available:\n");
		printf("\tget  <filename>\n");
		printf("\tput  <filename>\n");
		printf("\tdelete  <filename>\n");
		printf("\tmd5sum  <filename>\n");
		printf("\tls \n");
		printf("\texit \n");

		printf(" \n");

		printf("[Client] Enter the command: ");


	    fgets(msg, 100, stdin);

	    /* Split the command and filename */
	    char * token = strtok(msg, " ");
	    strcpy(cmd_msg.cmd, token);
	    token = strtok(NULL, " ");

	    if(token != NULL)
	    {
	    	/* make filename as null-terminated string */
	    	strcpy(cmd_msg.file, token);
	    	len = strlen(cmd_msg.file);
	    	if(cmd_msg.file[len - 1] == '\n')
	    		cmd_msg.file[len - 1] = '\0'; 

	    }
	    else
	    {
	    	/* make cmd as null-terminated string */
	    	len = strlen(cmd_msg.cmd);
	    	if(cmd_msg.cmd[len - 1] == '\n')
	    		cmd_msg.cmd[len - 1] = '\0'; 
	    	strcpy(cmd_msg.file, " ");
	    }

	    bzero(buf, BUFSIZE);

	    n = sendto(sockfd, (void *)&cmd_msg, sizeof(Request_t), 0,
	    			 (struct sockaddr *)&serveraddr, serverlen);
	    if (n < 0) 
			error("ERROR in sendto");

		n = recvfrom(sockfd, buf, 3, 0, 
					(struct sockaddr *)&serveraddr, &serverlen);
	    if (n < 0 || ((strcmp(buf, "OK")) != 0)) 
			error("ERROR in recvfrom");

	    printf("[Client] Echo from server: %s\n", buf);


	label1:

		/**** Case 1: GET File from server ****/
		if((strcmp(cmd_msg.cmd, "get")) == 0)
		{
    		fd = open(cmd_msg.file, O_CREAT | O_RDWR, S_IRWXU);

    		if (fd == -1)
            	error("Error opening file\n");
			
			n = recvfrom(sockfd, (void *)&len, sizeof(size_t), 0, 
						(struct sockaddr *)&serveraddr, &serverlen);
	    	if (n < 0) 
	    		error("ERROR in recvfrom");

	    	char * buffer = (char *)malloc(len);

	    	// printf("file size = %ld\n", len);

	    	offset = 0;

	    	while(offset < len)
	    	{
	    		n = recvfrom(sockfd, buffer + offset, BUFSIZE, 0, 
							(struct sockaddr *)&serveraddr, &serverlen);
	    		if (n < 0) 
	    			error("ERROR in recvfrom");

	    		// printf("Writing! %ld\n", offset);

	    		write(fd, buffer + offset, n);

	    		offset += n;

	    	}

	    	close(fd);
	    	free(buffer);

	    	if(ls_flag == 1)
	    	{
	    		system("cat file_list.txt");
	    		printf("\n=======================================================================\n");
	    		system("rm -rf file_list.txt");
	    		printf("** file_list.txt temp file for ls...deleted after use!\n");

	    	}
	    	else
	    		printf("[Client] File downloaded\n");
    	}

    	/**** Case 2: PUT File into server ****/
    	else if((strcmp(cmd_msg.cmd, "put")) == 0)
    	{
    		fd = open(cmd_msg.file, O_RDONLY);    
            
            if (fd == -1)
                error("Error opening file\n");

            /* send file size */
            len = file_size(fd);
            char * buffer = (char *)malloc(len);

            n = sendto(sockfd, (void *)&len, sizeof(size_t), 0,
                        (struct sockaddr *)&serveraddr, serverlen);
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
                            (struct sockaddr *)&serveraddr, serverlen);

                if (n < 0) 
                    error("ERROR in sendto");

                offset += n;
            }
            free(buffer);
            close(fd);

            printf("[Client] File transferred\n");

    	}

    	/**** Case 3: DELETE File from server ****/
    	else if((strcmp(cmd_msg.cmd, "delete")) == 0)
    	{
    		printf("[Client] File %s deleted\n", cmd_msg.file);
    	}

    	/**** Case 4: LIST Files from server ****/
    	else if((strcmp(cmd_msg.cmd, "ls")) == 0)
    	{
    		
    		printf("\n=======================================================================\n");
    		printf("[Client] List of Files on Server:    cmd(ls -la)\n");

    		strcpy(cmd_msg.cmd, "get");
            strcpy(cmd_msg.file, "file_list.txt"); 
            ls_flag = 1;

            goto label1;    		

    	}

    	/**** Case 5: Exit the server ****/ 
    	else if((strcmp(cmd_msg.cmd, "exit")) == 0)
    	{
    		printf("[Client] Server closed!\n");
    		printf("[Client] Shuting down client...\n");
    		break;
    	}

    	/**** Case 6: MD5SUM of file from the server ****/ 
    	else if((strcmp(cmd_msg.cmd, "md5sum")) == 0)
    	{
    		bzero(buf, BUFSIZE);
            sprintf(buf, "md5sum %s", cmd_msg.file);
            printf("[Client] MD5SUM of file %s on client:\n\n", cmd_msg.file);

            system(buf);
    	}

    	else
    		printf("Def:\n"); 


	    close(sockfd);
	    continue;


	    
	    // n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
	    // if (n < 0) 
	    //   error("ERROR in sendto");
	    
	    // /* print the server's reply */
	    // n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
	    // if (n < 0) 
	    //   error("ERROR in recvfrom");
	    // printf("Echo from server: %s", buf);


	    // close(sockfd);    	
    }

    close(sockfd);
    return 0;
}
