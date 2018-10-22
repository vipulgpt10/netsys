/* 
 * tcpserver.c - A TCP server handling multiple client
 * requests concurrently. 
 * 
 * usage: ./server <port>
 * 
 * author: Vipul Gupta
 * 
 * ref: https://www.cs.dartmouth.edu/~campbell/cs50/socketprogramming.html

 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXLINE 4096            /*max text line length*/
#define LISTENQ 16              /*maximum number of client connections*/
#define DOC_ROOT    "/www"      /* document dir name */


/* Get file size from file descriptor */
size_t file_size(int fd)
{
    struct stat buf;
    fstat(fd, &buf);
    return (size_t)buf.st_size;
}

/* To get file extension from filename */
const char * get_ext(const char * filename) 
{
    /* ptr to last occurence of '.' */
    const char * dot = strrchr(filename, '.');
    if(!dot || dot == filename) 
        return "";
    return dot + 1;
}

/* Error wrapper */
void error(char *msg) 
{
    perror(msg);
    exit(1);
}

int main (int argc, char **argv)
{
    
    int listenfd, connfd; /* socket */
    int portno; /* port to listen on */
    struct sockaddr_in servaddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    socklen_t clientlen;
    char buf[MAXLINE]; /* message buf */
    char tmp[MAXLINE]; /* temp message buf */
    int n; /* message byte size */
    int optval; /* flag value for setsockopt */

    pid_t childpid; /* Child process */
    char pwd[1024];  /* present working directory */
    char filename[1024];  /* present working directory */
    char reqMethod[32];     /* request method */
    char reqURL[256];       /* request URL */
    char reqVersion[32];    /* request version */
    char content[50];       /* file content info */
    size_t len;
    int fd; /* File descriptor */
    size_t readnow;

    /* check command line arguments */
    if (argc != 2) 
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /* Get current directory */
    getcwd(pwd, sizeof(pwd));
    printf("Current working dir: %s\n", pwd);

    strcat(pwd, DOC_ROOT);
    printf("Current working dir: %s\n", pwd);

    /* socket: create the parent socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
    * us rerun the server immediately after we kill it; 
    * otherwise we have to wait about 20 secs. 
    * Eliminates "ERROR on binding: Address already in use" error. 
    */
    optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                (const void *)&optval , sizeof(int));

    /* preparation of the socket address */  
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons((unsigned short)portno);

    /* bind: associate the parent socket with a port */
    if(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) 
        error("ERROR on binding");

    /* listen to the socket by creating a connection queue, then wait for clients */
    listen (listenfd, LISTENQ);

    printf("Server runnning...and waiting for new connections.\n");

    while(1)
    {

        clientlen = sizeof(clientaddr);
        
        /* accept a new connection */
        connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);

        printf("%s\n","Received request...");

        /* child process pid = 0 */
        if ((childpid = fork()) == 0 ) 
        {
            printf ("%s\n","Child process created...");

            /* close listening socket */
            close (listenfd);

            n = recv(connfd, buf, MAXLINE, 0);

            printf("%s\n","String received from and resent to the client:");

            puts(buf);

            /* Split the request method, URL and version */
            strcpy(tmp, buf);
            char * token = strtok(tmp, " ");
            strcpy(reqMethod, token);
            token = strtok(NULL, " ");
            strcpy(reqURL, token);
            token = strtok(NULL, "\n");
            strcpy(reqVersion, token);
            len = strlen(reqVersion);
            reqVersion[len - 1] = '\0';

            /* Get absolute file name and path */
            strcat(filename, pwd);
            strcat(filename, reqURL);

            /* Content type */
            strcpy(tmp, get_ext(reqURL));
            if(strcmp(tmp, "html") == 0)
                strcpy(content, "text/html");
            else if(strcmp(tmp, "txt") == 0)
                strcpy(content, "text/plain");
            else if(strcmp(tmp, "png") == 0)
                strcpy(content, "image/png");
            else if(strcmp(tmp, "gif") == 0)
                strcpy(content, "image/gif");
            else if(strcmp(tmp, "jpg") == 0)
                strcpy(content, "image/jpg");
            else if(strcmp(tmp, "css") == 0)
                strcpy(content, "text/css");
            else if(strcmp(tmp, "js") == 0)
                strcpy(content, "application/javascript");
            else 
                strcpy(content, " ");

            bzero(buf, MAXLINE);

            /* open file and check if it exists */
            fd = open(filename, O_RDONLY);    
        
            if (fd == -1)       // if it doesn't exist 
            {
                printf("Error opening file: %s\n", filename);
                strcat(buf, "HTTP/1.1 500 Internal Server Error");

                /* Send Header packet */
                send(connfd, buf, sizeof(buf), 0);
            }
            
            else
            {
                /* file size */
                len = file_size(fd);

                /* Form the Header and data content */
                if(strcmp(reqVersion, "HTTP/1.1") == 0)
                    strcat(buf, "HTTP/1.1");
                else if(strcmp(reqVersion, "HTTP/1.0") == 0)
                    strcat(buf, "HTTP/1.0");
                /* Success status code */
                strcat(buf, " 200 Document Follows\r\n");
                strcat(buf, "Content-Type: ");
                strcat(buf, content);
                strcat(buf, "\r\n");

                strcat(buf, "Content-Length: ");
                sprintf(tmp, "%ld", len);
                strcat(buf, tmp);
                strcat(buf, "\r\n\r\n");

                /* Send Header Packet */
                send(connfd, buf, strlen(buf), 0);

                /* Send Data content */
                while(len)
                {
                    bzero(buf, MAXLINE);
                    readnow = read(fd, &tmp, MAXLINE);
                    memcpy((void *)buf, (void*)tmp, readnow);
                    len = len - readnow;
                    send(connfd, buf, readnow, 0);
                }

            }

            if (n < 0)
                printf("%s\n", "Read error");

            close(fd);
            exit(0);
        }
    
    /* close socket of the server, handled by child process */
    close(connfd);
    
    }
}