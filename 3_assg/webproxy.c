/* 
 * webproxy.c - A HTTP webproxy server
 * 
 * usage: ./webproxy <port> <timeout_sec>
 * 
 * author: Vipul Gupta
 * 
 * ref: https://www.cs.dartmouth.edu/~campbell/cs50/socketprogramming.html
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <openssl/md5.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>


#define LISTENQ 50              /* maximum number of client connections*/
#define MAXLINE 512             /* max text line length*/

int connfd;
int timeout = 0;

/* Error wrapper */
void error(char *msg) 
{
    perror(msg);
    exit(1);
}

/* Get file size from file descriptor */
size_t file_size(int fd)
{
    struct stat buf;
    fstat(fd, &buf);
    return (size_t)buf.st_size;
}


char * computeMD5(const char * str, int length);

/* Return 1 if cache-copy is valid else 0 */
int check_cache(char* cache_name);

/* Return 0 if not blocked else return 1 */
int check_blocklist(char * host_name, char* host_ip);

void prefetch_all_links(char *filename,int sockfd);

void getcachefiletime(char * name, char time_created[1000]);


int main(int argc, char **argv)
{

	int listenfd; /* socket */
    int portno; /* port to listen on */
    struct sockaddr_in servaddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    socklen_t clientlen;
    int optval; /* flag value for setsockopt */

	
	pid_t childpid;

	int result = 0;
	char * md5sum;
	struct sockaddr_in addr_in;
	struct hostent * host;

	FILE *fp;
	char cache_name[100];


	/* check command line arguments */
	if (argc != 3) 
	{
	    fprintf(stderr, "usage: %s <port> <timeout_sec>\n", argv[0]);
	    exit(1);
	}

	portno = atoi(argv[1]);
	timeout = atoi(argv[2]);

	bzero((char *)&servaddr, sizeof(servaddr));
	bzero((char *)&clientaddr, sizeof(clientaddr));

	/* preparation of the socket address */  
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons((unsigned short)portno);


	/* socket: create the client side socket */
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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


    /* bind: associate the parent socket with a port */
    if(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) 
        error("ERROR on binding");

    /* listen to the socket by creating a connection queue, then wait for clients */
    listen (listenfd, LISTENQ);

    printf("Proxy Server runnning on port %d ...\n", portno);


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

            struct sockaddr_in hostaddr;
			int flag = 0;
			int servSocnew, n, port = 0, i, servSoc;

			char buffer[MAXLINE];
			char reqMethod[64];
			char reqType[200];
			char reqVersion[32];
			char path[MAXLINE];
			char sendbuffer[10000];
			char filenamepath[1024];
			char * URL = NULL;
			int ret = 0;


			bzero(buffer, MAXLINE);

            /* close listening socket */
            close (listenfd);

            n = recv(connfd, buffer, MAXLINE, 0);

            sscanf(buffer,"%s %s %s", reqMethod, reqType, reqVersion);

            /* Valid Method */
            if((strncmp(reqMethod, "GET", 3) == 0)
            	&& ((strncmp(reqVersion, "HTTP/1.1", 8) == 0) || (strncmp(reqVersion, "HTTP/1.0", 8) == 0))
            	&& (strncmp(reqType, "http://", 7) == 0))
			{
				strcpy(reqMethod, reqType);
				strcpy(path, reqType);

				md5sum = computeMD5(path, strlen(path));
				printf("MD5sum is %s\n",md5sum);
				sprintf(cache_name, "%s.html",md5sum);

				result = check_cache(cache_name);
				
				/* no valid cache-copy : Connect to server */
				if(result == 0)
				{
  					printf("No Valid Cache-copy: Connecting to server\n");
  					sprintf(filenamepath, "Cache/%s", cache_name);

  					fp = fopen(filenamepath, "w+");
					flag=0;
  					
  					for(i = 7; i < strlen(reqType); i++)
  					{
    					if(reqType[i] == ':')
    					{
    						flag=1;
    						break;
    					}
  					}
  					
  					URL = strtok(reqType, "//");
  					
  					if(flag == 0)
  					{
  						port = 80;
  						URL = strtok(NULL, "/");
  					}
  					else
  					{
  						URL = strtok(NULL, ":");
  					}

					sprintf(reqType, "%s", URL);
					printf("Host = %s", reqType);

  					host = gethostbyname(reqType);

  					printf("\nhost->h_addr:%s,hostaddr->h_length:%d\n",(char *)host->h_addr,host->h_length);

  					if(flag == 1)
  					{
  						URL = strtok(NULL, "/");
  						port = atoi(URL);
  					}
  					
  					strcat(reqMethod, "^]");
  					URL = strtok(reqMethod,"//");
  					URL = strtok(NULL,"/");
  					
  					if(URL != NULL)
  						URL = strtok(NULL, "^]");
  					
  					printf("\npath: %s\nPort: %d\n", URL, port);


  					bzero((char*)&hostaddr, sizeof(hostaddr));
  					hostaddr.sin_port = htons(port);
  					hostaddr.sin_family = AF_INET;
  					bcopy((char*)host->h_addr, (char*)&hostaddr.sin_addr.s_addr, host->h_length);

  					servSoc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					servSocnew = connect(servSoc, (struct sockaddr*)&hostaddr, sizeof(struct sockaddr));
  					
  					sprintf(buffer,"\nConnected to %s  IP - %s\n", reqType, inet_ntoa(hostaddr.sin_addr));
  					
  					ret = check_blocklist(reqType, inet_ntoa(hostaddr.sin_addr));
  					
  					/* host is blocked */
  					if(ret == 1)
  					{
    					char blocked[100] = "<html><body><H1>Error 403 Forbidden </H1></body></html>";
    					send(connfd, blocked, strlen(blocked), 0);
  					}
  					/* Not blocked */
  					else
  					{
  						if(servSocnew < 0)
  							error("Error in connecting to remote server");
  						
  						bzero((char*)buffer, sizeof(buffer));

  						//Creating URL to send to hostaddr
  						if(URL != NULL)
  							sprintf(buffer, "GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",URL, reqVersion, reqType);
  						else
  							sprintf(buffer, "GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n", reqVersion, reqType);

  						//sending request to hostaddr server
  						n = send(servSoc, buffer, strlen(buffer), 0);
  						
  						if( n < 0)
  							error("Error writing to socket");
  						else
  						{
  							do
  							{
    							bzero((char*)buffer, 500);
    							n = recv(servSoc, buffer, 500, 0);
    							
    							if(!(n <= 0))
    							{
    								send(connfd, buffer, n, 0);
    								fwrite(buffer, 1, n, fp);
    							}
  							}while(n > 0);
  						}
  						
  						fclose(fp);
  					}
  					
  					prefetch_all_links(filenamepath, servSoc);
				}
				/* Present in cache */
				else
				{
  					char fname_path[1000];
  					sprintf(fname_path,"Cache/%s",cache_name);
  					printf("present in cache\n");
  					
  					fp = fopen(fname_path,"r");
  					
  					do 
  					{
    					n = fread(sendbuffer, 1, 9999, fp);
    					send(connfd, sendbuffer, n, 0);
    					printf("Sent\n");
  					}while(n > 0);
  					
  					fclose(fp);
				}
			}
			/* Invalid Method */
			else
			{
  				char invalid_method[200] = "<html><body><H1>Error 400 Bad Request: Invalid method </H1></body></html>";
  				send(connfd, invalid_method, strlen(invalid_method), 0);
			}
			
			close(servSoc);
			close(connfd);
			_exit(0);
		}
		
		close(connfd);
	}

	return 0;
		
}

char * computeMD5(const char * str, int length)
{
	int n;
	MD5_CTX key;
	unsigned char digest[16];

	char * out = (char *)malloc(64);
	MD5_Init(&key);

	while(length > 0)
	{
		if(length >512)
			MD5_Update(&key, str, 512);
		else
	  		MD5_Update(&key, str, length);
		
		length -= 512;
		str += 512;
	}
	
	MD5_Final(digest, &key);

	for(n = 0; n<16; n++)
	{
		snprintf(&(out[n*2]), 32, "%02x", (unsigned int)digest[n]);
	}
	
	return out;
}

/* Return 1 if cache-copy is valid else 0 */
int check_cache(char* cache_name)
{
	struct stat st = {0};
	FILE* fp;
	char str[1000];
	int file_time;
	int curr_time;

	if(stat("Cache", &st) == -1)
		mkdir("Cache", 0755);

	sprintf(str, "Cache/%s", cache_name);

	fp = fopen(str,"r");
	char time_created[1000];
	
	if(fp == NULL)
		return 0;
	else
	{
		getcachefiletime(str,time_created);
		char * hr, * min,* sec;

		hr = strtok(time_created,":") ;
		min = strtok(NULL,":") ;
		sec = strtok(NULL,":") ;
		sec = strtok(sec, " ");
		hr = strtok(hr," ") ;
		hr = strtok(NULL," ");
		hr = strtok(NULL," ");
		hr = strtok(NULL," ");

		file_time = atoi(hr)*3600 + atoi(min)*60 + atoi(sec);

		time_t current_time;
		time(&current_time);
		bzero(time_created, sizeof(time_created));
		sprintf(time_created, "%s", ctime(&current_time));

		printf("Present Time: %s\n", time_created);

		hr = strtok(time_created,":");

		min = strtok(NULL,":") ;

		sec = strtok(NULL,":") ;
		sec = strtok(sec, " ");

		hr = strtok(hr," ") ;
		hr = strtok(NULL," ");
		hr = strtok(NULL," ");
		hr = strtok(NULL," ");

		curr_time = atoi(hr)*3600 + atoi(min)*60 + atoi(sec);
		
		if (curr_time - file_time > timeout)
		{
	   		printf("TIMEOUT EXPIRED\n");
	   		return 0;
		}
		else
		{
	 		fclose(fp);
	 		return 1;
		}
	}
}

/* Return 0 if not blocked else return 1 */
int check_blocklist(char * host_name, char* host_ip)
{
	int fd;
	int found = 0;
	char buffer[8000];
	char filename[] = "blocked_list.txt";
	
	fd = open(filename, O_RDONLY);

	if(fd == -1)
	{
		printf("There are no blocked websites\n");
		return 0;
	}
	else
	{
		read(fd, buffer, file_size(fd));

		if((found = strstr(buffer, host_name) != NULL) || 
			(found = strstr(buffer, host_ip) != NULL))
		{
	  		printf("blocked website %s:%s", host_name, host_ip);
	  		close(fd);
	  		return 1;
		}
		else
		{
			close(fd);
	  		return 0;
		}
	}
}


void prefetch_all_links(char *filename,int sockfd)
{
	
	printf("prefetching links\n");
	char buf_to_send[1000000];
	char filepath[9999];
	int i, openFile;
	char * md5;
	FILE * fp;
	char http_url[9999];
	char * ret;
	char * retVal;
	char path[9999];
	char prefetch_req[9999];
	char buffer[9999];
	int readFile, n;
	char md5sum[9999];
	char * new_ptr;


	bzero(http_url,sizeof(http_url));
	bzero(path,sizeof(path));
	bzero(buffer,sizeof(buffer));
	bzero(buf_to_send,sizeof(buf_to_send));
	bzero(filepath,sizeof(filepath));
	bzero(prefetch_req,sizeof(prefetch_req));
	bzero(md5sum,sizeof(md5sum));

	openFile = open(filename,O_RDONLY);
	if(openFile==-1)
		printf("\n error in reading the file");
	

	// reading file
	readFile = read(openFile,buf_to_send, sizeof buf_to_send);
	if (readFile < 0)
		printf("FILE reading error\n");

	// md5sum
	if((ret=strstr(buf_to_send,"href=\"http://"))!= NULL)
	{
		while((ret=strstr(ret,"href=\"http://")) )
		{
			ret = ret+13;
			i=0;
			while(*ret!='"')
	  		{
	      		http_url[i] = *ret;
	      		ret++;
	      		i++;
	  		}
			
			new_ptr = ret;
			http_url[i]='\0';

			strcpy(md5sum, http_url);
			md5 = computeMD5(md5sum,strlen(md5sum));


			printf("\n md5 sum of the link:%s",md5);
			sprintf(filepath,"Cache/%s.html",md5);
			printf("filepath: %s",filepath);
			retVal = strstr(http_url,"/");
			
			if ( retVal == NULL)
				continue;
			
			if(retVal!=NULL)
	  			strcpy(path,retVal);

			*retVal='\0';
			ret =new_ptr +1;

			printf("\nGET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, http_url);
			sprintf(prefetch_req,"GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",path,http_url);

			n = send(sockfd,prefetch_req,strlen(prefetch_req),0);
			printf("\n sending request");

			fp = fopen(filepath,"w");
			
			do
			{
	  			printf("\n getting written into file");
	  			n = recv(sockfd,buffer,500,0);
	  			fwrite(buffer,1,n,fp);
			}while(n>0);
		}
	}
}

void getcachefiletime(char * name, char time_created[1000])
{
	struct stat attr;
	stat(name, &attr);
	sprintf(time_created,"%s",ctime(&attr.st_mtime));
}
