#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<stdbool.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<unistd.h>
#include<inttypes.h>
#include<sys/wait.h>
#include<openssl/crypto.h>
#include<openssl/md5.h>
#include<errno.h>
#include<limits.h>
#include<sys/stat.h>
#include<dirent.h>

#define TCP_SOCKETS SOCK_STREAM

#define MAX_SERV 4

#define CHUNK_DUPLICATES 2

typedef enum status_info_t {
  NULL_VALUE = 0,
  INCORRECT_INPUT,
  VALID_RETURN,
  SUCCESS,
  FAILURE
} status_info;

typedef struct __client_IP_and_Port_t {
  char IP_Addr[10][10];
  uint16_t port_num[10];
} client_ip_port_t;

typedef struct __client_config_data_t {
  char DFS_Server_Names[10][10];
  client_ip_port_t client_ports;
  char filename[20];
  char username[15];
  char password[15];
} client_config_data_t;

typedef struct __server_config_data_t {
  char username[5][15];
  char password[5][15];
} server_config_data_t;

char chunk_filename_list[4][20];

char global_client_buffer[2][20];

uint8_t put_file_count = 0;
char cache_put_filenames[5][20];


int auth_server_list[MAX_SERV];

char chunk_names[MAX_SERV][10] = {
  ".chunk_0", ".chunk_1", ".chunk_2", ".chunk_3"
  };

uint8_t dist_array[MAX_SERV][MAX_SERV][CHUNK_DUPLICATES] = {
{{1, 4}, {1, 2}, {2, 3}, {3, 4}},\
{{1, 2}, {2, 3}, {3, 4}, {4, 1}},\
{{2, 3}, {3, 4}, {4, 1}, {1, 2}},\
{{3, 4}, {4, 1}, {1, 2}, {2, 3}}
};

char *valid_commands[] = {  
  "put", 
  "get",
  "list",
  "exit",
  "clear"
};

status_info validate_login(char *input_buffer,\
                  server_config_data_t *config_data, uint8_t *server_count)
{
  if(input_buffer == NULL) {
    printf("<<%s>>: Null Value passed!\n", __func__);
    return NULL_VALUE;
  }
   
  char *local_buffer = strtok(input_buffer, " \n");
  while(local_buffer != NULL) {
    if(strcmp(local_buffer, config_data->username[0])) {// auth. failed! 
      return AUTH_FAILURE;
    } else {
      local_buffer = strtok(NULL, " \n");
      printf("<<%s>>: %s\n", __func__, local_buffer);
      if(strcmp(local_buffer, config_data->password[0])) {
        return AUTH_FAILURE;
      } else {
        /* Tell the client that all auth was successful */
        return AUTH_SUCCESS;
      
      }
    }
    local_buffer = strtok(NULL, " \n");
  }
}

status_info Send_auth_Login(int client_socket, client_config_data_t *client_data)
{

  char buffer[50];
  memset(buffer, '\0', sizeof(buffer));

  /* Combine the client data and password in one string and then send that */
  strncat(buffer, client_data->username, strlen(client_data->username));
  strncat(buffer, " ", strlen(" "));
  strncat(buffer, client_data->password, strlen(client_data->password)); 

  int send_ret = send(client_socket, buffer, strlen(buffer), 0);
  if(send_ret < 0) {
    perror("SEND");
    return AUTH_FAILURE;
  }
  
  /* Receive pass/fail status from server */
  memset(buffer, '\0', sizeof(buffer));
  recv(client_socket, buffer, 50, 0);
  //printf("<%s>:buffer: %s\n", __func__, buffer);
 
  if(!strcmp(buffer, "fail")) { /* Auth Failed! */
    /* Don't close connection, just decline and exit */
    return AUTH_FAILURE;
  } else { /* Auth Passed! */
    printf("<%s>: Auth Passed!\n", __func__);
    return AUTH_SUCCESS;
  }
}


void Chunk_Store_File(void *filename, uint8_t hash_mod_value, client_config_data_t *client_data)
{
  FILE *fp = fopen(filename, "rb");
  if(fp == NULL) {
    printf("file %s can't be opened!\n", (char *)filename);
    return;
  }

  /* 1. Get the size of the file first and alloc an array accordingly */
  uint32_t file_size = 0;
  fseek(fp, 0L, SEEK_END);
  file_size = ftell(fp);
  rewind(fp);

  uint8_t seq = 0;
  uint32_t read_count = 0;

  /* If the file size is not a multiple of 4, stuff the remaining 
   * bytes into the last file chunk
   */
  uint32_t packet_size = (file_size/4);
 
  printf("File Size: %d packet_size: %d\n", file_size, packet_size);

  char chunk_storage[10000];
  memset(chunk_storage, '\0', sizeof(chunk_storage));

  while(!feof(fp)) {
    /* Buffer Cleanup */ 
    memset(chunk_storage, '\0', sizeof(chunk_storage));
   
    /* Need to handle the case for the last packet */
    if(seq == (MAX_SERV-1)) {
      printf("Last packet NOW!\n");
      read_count = fread(chunk_storage, 1, (packet_size + (file_size%4)), fp);
      //printf("<<%s>>: read_count: %d\n", __func__, read_count);

      Send_Chunk(filename, hash_mod_value, chunk_storage, read_count, seq, client_data); 
      return;
    } else {
      read_count = fread(chunk_storage, 1, packet_size, fp);
      printf("<<%s>>: read_count: %d\n", __func__, read_count);
    } 

    /* Send the chunk to its appropriate location */
    Send_Chunk(filename, hash_mod_value, chunk_storage, read_count, seq, client_data); 
    seq++;
  }

  fclose(fp);

  return;
}

void Chunk_File(void *filename)
{
  FILE *fp = fopen(filename, "rb");
  if(fp == NULL) {
    printf("file %s can't be opened!\n", (char *)filename);
    return;
  }
  
  /* 1. Get the size of the file first and alloc an array accordingly */
  uint32_t file_size = 0;
  fseek(fp, 0L, SEEK_END);
  file_size = ftell(fp);
  rewind(fp);

  /* If the file size is not a multiple of 4, stuff the remaining 
   * bytes into the last file chunk
   */
  uint32_t packet_size = (file_size/4);
 
#ifdef DEBUG_GENERAL
  printf("PacketSize: %d\n", packet_size);
  printf("File Size: %d div/4: %d\n", file_size, file_size/4);
#endif

  int i = 0;
  int cnt = 0;
  char fp_read_char = '\0';

  uint8_t file_counter = 1;
  FILE *output_file = NULL;
  
  memset(chunk_filename_list, '\0', sizeof(chunk_filename_list));

  sprintf(chunk_filename_list[file_counter-1], "chunk_%d", file_counter);
  output_file = fopen(chunk_filename_list[file_counter-1], "w");

  while((cnt = fgetc(fp)) != EOF) {
    fp_read_char = (char)cnt;  
    fputc(fp_read_char, output_file);
    i++;
    
    if(i == packet_size && file_counter < 4) {
      
      fclose(output_file);
      file_counter++; i = 0;

      sprintf(chunk_filename_list[file_counter-1], "chunk_%d", file_counter);
      output_file = fopen(chunk_filename_list[file_counter-1], "w");
      continue;
    } 
  }

  fclose(output_file);
  fclose(fp);
  return;
}

void create_md5_hash(char *digest_buffer, char *buffer, int buffer_size)
{
  char digest[16];
  MD5_CTX md5_struct;

  MD5_Init(&md5_struct);
  MD5_Update(&md5_struct, buffer, buffer_size);
  MD5_Final(digest, &md5_struct);

  return;
}

uint8_t Generate_MD5_Hash(void *filename, unsigned char *ret_digest)
{
  FILE *fp = fopen(filename, "rb");
  if(fp == NULL) {
    printf("file %s can't be opened!\n", (char *)filename);
    return 0;
  }
  
  MD5_CTX md5context;
  int bytes = 0;
  unsigned char data[1024];
  memset(data, '\0', sizeof(data));
  
  unsigned int digest_buffer[MD5_DIGEST_LENGTH];
  memset(&digest_buffer, 0, sizeof(digest_buffer));

  MD5_Init(&md5context);
  while((bytes = fread(data, 1, 1024, fp)) != 0)
    MD5_Update(&md5context, data, bytes);

  MD5_Final(ret_digest, &md5context);
 
  char temp_store[5];
  memset(temp_store, '\0', sizeof(temp_store));
  uint32_t hash_hex_value = 0;
  uint8_t temp = 0;

  for(int i = 0; i<MD5_DIGEST_LENGTH; i++) {
    
    sprintf(temp_store, "%x", ret_digest[i]);
    temp = strtol(temp_store, NULL, 16);
    
    hash_hex_value = hash_hex_value | (temp << ((MD5_DIGEST_LENGTH/2) * (3-i)));
    printf("\n");
    if((3 - i) == 0)
      break;
  }

  fclose(fp);

  return (hash_hex_value%4);
}


void Create_Client_Connections(uint8_t *client_socket, uint16_t port_number,\
                                 struct sockaddr_in *server_addr, size_t server_addr_len)
{
  *client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if(*client_socket < 0) {
    perror("ERROR:socket()\n");
    return;
  }

  printf("port_num: %d\n",port_number);
  server_addr->sin_family = AF_INET;
  server_addr->sin_port = htons(port_number);
  server_addr->sin_addr.s_addr = INADDR_ANY;
  memset(server_addr->sin_zero, '\0', sizeof(server_addr->sin_zero));

  /* Connect to the IP on the port */
  connect(*client_socket, (struct sockaddr *)server_addr, server_addr_len);

  return;
}

void Parse_Server_Config_File(void *filename, server_config_data_t *config)
{
  size_t read_line = 0;
  char *line = (char *)malloc(50);
  FILE *fp_config_file = NULL;
  char *buffer = NULL;
  uint8_t cntr = 0;

  static uint8_t username_cntr = 0;
  static uint8_t password_cntr = 0;

  fp_config_file = fopen((char *)filename, "rb");
  if(fp_config_file == NULL) {
    fprintf(stderr, "%s: ", (char *)filename);
    perror("");
    exit(0);
  }

  /* Clear the struct buffers */
  memset(config->username, '\0', sizeof(config->username));
  memset(config->password, '\0', sizeof(config->password));

  while( (read_line = getline(&line, &read_line, fp_config_file)) != -1) {
    cntr = 0;
    buffer = strtok(line, " \n");
    while(buffer != NULL) {
      switch(cntr)
      {
        case 0: /* Get the username and store it */
          memcpy(config->username[username_cntr], buffer, strlen(buffer));
          username_cntr++;
          break;

        case 1: /* Get the password and store it */
          memcpy(config->password[password_cntr], buffer, strlen(buffer));
          password_cntr++;
          break;

        default: /* Do Nothing */
          break;
      }
      buffer = strtok(NULL, " \n");
      cntr++;
    }
  }



  free(line);
  return;
}

void Create_Server_Connections(int *server_sock, struct sockaddr_in *server_addr,\
                                int server_addr_len, int tcp_port)
{
  if(server_addr == NULL) {
    return;
  }

  (*server_sock) = socket(AF_INET, TCP_SOCKETS, 0);

  /* set the parameters for the server */
  server_addr->sin_family = AF_INET;
  server_addr->sin_port = htons(tcp_port);
  server_addr->sin_addr.s_addr = INADDR_ANY;

  /* Bind the server to the client */
  int ret_val = bind(*server_sock, (struct sockaddr *)server_addr, server_addr_len);
  if(ret_val < 0) {
    perror("ERROR:bind()\n");
    close(*server_sock); 
    exit(1);
  }


  ret_val = listen(*server_sock, 5);
  if(ret_val < 0) {
    perror("ERROR:listen()\n");
    exit(1);
  }
  
  return;
}


void parse_config(void *filename, client_config_data_t *config)
{
  size_t read_line = 0;
  uint8_t cntr_words = 0;
  char *line = (char *)malloc(50);
  FILE *fp_config_file = NULL;
  
  fp_config_file = fopen((char *)filename, "rb");
  if(fp_config_file == NULL) {
    fprintf(stderr, "%s: ", (char *)filename);
    perror("");
    exit(0);
  }

  /* Clear the buffers before proceeding */
  memset(config->DFS_Server_Names, '\0', sizeof(config->DFS_Server_Names));
  memset(config->client_ports.IP_Addr, '\0', sizeof(config->client_ports.IP_Addr));
  memset(config->client_ports.port_num, 0, sizeof(config->client_ports.port_num));
  memset(config->username, '\0', sizeof(config->username));
  memset(config->password, '\0', sizeof(config->password));


  while( (read_line = getline(&line, &read_line, fp_config_file)) != -1) {
    
    /* Store the value of the first word of every line */
    char temp_name[10];
    memset(temp_name, '\0', sizeof(temp_name));
    for(int i = 0; i<8; i++) {
      temp_name[i] = line[i];
    }
   
    /* Get the username from the file */
    if(!strcmp(temp_name, "Username")) {
      uint8_t i = 0;
      char *buf = strtok(line, " \n");
      while(buf != NULL) {
        if(i == 1) {
          strcpy(config->username, buf);
        }
        i++;
        buf = strtok(NULL, " \n");
      }
      continue;
    }

    /* Get the password from the file */
    if(!strcmp(temp_name, "Password")) {
      uint8_t i = 0;
      char *buf = strtok(line, " ");
      while(buf != NULL) {
        if(i == 1) {
          strcpy(config->password, buf);
        }
        i++;
        buf = strtok(NULL, "\n");
      }
      continue;
    }

    /* get the server name, IP and Port number from the extracted line */ 
    cntr_words = 0;
    char *buf = NULL;
    char *buffer = strtok(line, " \n");
    uint8_t gen_counter = 0;
    static uint8_t cnt_server_name = 0;
    static uint8_t cnt_ip_port = 0;
    
    while(buffer != NULL) {
      switch(cntr_words)
      {
        case 0:
          /* This is just the word "Server"; do nothing */
          break;

        case 1:
          /* This is the DFS server name.
           * Store the names of the servers in memory 
           */
          memcpy(config->DFS_Server_Names[cnt_server_name], buffer, strlen(buffer));
          cnt_server_name++;
          break;

        case 2:
          /* This is the IP address and the port number */
          buf = strtok(buffer, ":");
          while(buf != NULL) {
            switch(gen_counter)
            {
              case 0: /* This is the IP address */
                      strcpy(config->client_ports.IP_Addr[cnt_ip_port], buffer);
                      break;
              case 1: /* This is the port number */
                      config->client_ports.port_num[cnt_ip_port] = atoi(buf);
                      break;
              default: /*Do Nothing*/
                        printf(" You shouldn't be here!\n");
                        break;
            }
            gen_counter++;
            buf = strtok(NULL, "\n");
          }
          cnt_ip_port++;
          break;

        default:
          /* Nothing to do here */
          printf("default   %s\n", buffer);
          break;
      }
      buffer = strtok(NULL, " \n");
      cntr_words++;
    }
  }

  fclose(fp_config_file);
  free(line);

  return;
}

void put_command(void *filename, client_config_data_t *client_data)
{
  /* Get the hash of the file */
  unsigned char digest_buffer[MD5_DIGEST_LENGTH];
  memset(digest_buffer, '\0', sizeof(digest_buffer));
    
  memcpy(cache_put_filenames[put_file_count++], client_data->filename,\
                    strlen(client_data->filename));
  printf("filename: %s\n", cache_put_filenames[put_file_count-1]);

  uint8_t hash_mod_val = Generate_MD5_Hash(filename, digest_buffer); 
  
  Chunk_Store_File(filename, hash_mod_val, client_data);
 
  return;
}

void list_command(client_config_data_t *client_data)
{
  /* Query all the servers and get the files from them */
  DIR *directory = NULL;
  struct dirent *dir = NULL;

  /* Contruct the path to the directories and check if they are available */
  char path_string[60];
  memset(path_string, '\0', sizeof(path_string));
  
  uint8_t chunk_checklist[MAX_SERV];
  memset(chunk_checklist, 0, sizeof(chunk_checklist));

  /* File Look Up Counter*/
  /* The first index is the server number and the second index
   * denotes the chunk number. This let's you know that chunk 
   * number is there in which server
   */
  uint8_t local_file_lookup[MAX_SERV][MAX_SERV];
  memset(local_file_lookup, 0, sizeof(local_file_lookup));

  char file_prefix[40];
  memset(file_prefix, '\0', sizeof(file_prefix));
 
  /* TODO: This needs to be abstracted further. It's utterly unreadable and 
   *        I can't maintain it like this!
   */

  for(int file_count = 0; file_count < put_file_count; file_count++) {
  
    memset(chunk_checklist, 0, sizeof(chunk_checklist));
    memset(local_file_lookup, 0, sizeof(local_file_lookup));
    memset(file_prefix, '\0', sizeof(file_prefix));
    
    for(int i = 0; i<MAX_SERV; i++) {
      if(auth_server_list[i]) {
        memset(path_string, '\0', sizeof(path_string));
        sprintf(path_string, "../DFS_Server/DFS%d/%s", (i+1), client_data->username);

        directory = opendir(path_string);
        if(directory) {
          while((dir = readdir(directory)) != NULL) {

            /* Discard these 2 cases! */
            if(!strcmp(dir->d_name, ".") || !(strcmp(dir->d_name, "..")))
              continue;

            /* Query through the name list to see if you find a match */
            for(int j = 0; j<MAX_SERV; j++) {
              sprintf(file_prefix, ".%s%s", cache_put_filenames[file_count],\
                  chunk_names[j]);
              if(!strcmp(dir->d_name, file_prefix)) {
                /* If you find a match, just increment the counter for 
                 * that particular chunk number. That way you know what 
                 * is there in what server.
                 */     
                (local_file_lookup[i][j])++;
                chunk_checklist[j]++;
                break;
              }
            }
            }
          }
          closedir(directory);
        }
      }
      printf("FILE: %s\n", cache_put_filenames[file_count]);
      for(int i = 0; i<MAX_SERV; i++) {
        if(chunk_checklist[i] < 1) {
          printf("%s incomplete!\n", chunk_names[i]);
        } else {
          printf("%s\n", chunk_names[i]);
        }
      }
    }

#ifdef DEBUG_GENERAL
  for(int j = 0; j<MAX_SERV; j++) {
    printf("local_file_lookup[%d]: %d %d %d %d\n",j,\
        local_file_lookup[j][0], local_file_lookup[j][1],\
        local_file_lookup[j][2], local_file_lookup[j][3]);
  }
#endif

  return;
}

void get_command(client_config_data_t *client_data)
{
  DIR *directory = NULL;
  struct dirent *dir = NULL;
  
  /* Query the server locations for the chunks; in order */
  char path_string[60];
  memset(path_string, '\0', sizeof(path_string));
 
  bool found = false;

  for(int j = 0; j<MAX_SERV; j++) {
    for(int i = 0; i<MAX_SERV; i++) {
      memset(path_string, '\0', sizeof(path_string));
      sprintf(path_string, "../DFS_Server/DFS%d/%s", (i+1), client_data->username);
      //printf("%s\n", path_string);

      directory = opendir(path_string);
      if(directory) {
        while((dir = readdir(directory)) != NULL) {

          /* Discard these 2 cases! */
          if(!strcmp(dir->d_name, ".") || !(strcmp(dir->d_name, "..")))
            continue;
         
        
          if(!strcmp(dir->d_name, chunk_names[j])) {
            /* Do the read and write operation now! */
            printf("%s\n", dir->d_name);
            found = true;
            break;
          }
        }
      } else {
        printf("Unable to open the file!\n");
      }
      if(found == true) {
        closedir(directory);
        found = false;
        break;
      } else {
        closedir(directory);
      }
    }
  }
  return;
}

int main(int argc, char *argv[])
{
  switch(argc)
  {
    case 1: /* Config. file not passed */
      printf("Configuration file not passed!\n");
      exit(0);
      break;

    case 2: /* Server name */
      printf("Server name not passed!\n");
      exit(0);
      break;

    case 3: /* port number */
      printf("Port number not passed!\n");
      exit(0);
      break;

    default:
      break;
  }


  if(strcmp(argv[2], "DFS1") && strcmp(argv[2], "DFS2") &&\
      strcmp(argv[2],"DFS3") && strcmp(argv[2], "DFS4")) {
    printf("Invalid DFS Server name!\n");
  }

  /* Parse the config file and store its contents */
  server_config_data_t server_config;
  Parse_Server_Config_File(argv[1], &server_config);


  /* Get the port number and setup the server */
  uint16_t port_num = atoi((argv[3]));
  int server_socket = 0;

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));

  /* Setup the webserver connections */
  Create_Server_Connections(&server_socket, &address,\
      sizeof(address), port_num);

  socklen_t addr_len = sizeof(address);
  int accept_ret = 0;

  char buffer[50];
  memset(buffer, '\0', sizeof(buffer));

  static int server_counter = 0;

  while(1) {

    status_info r_val = Accept_Auth_Client_Connections(&accept_ret, server_socket,\
                                        &address, addr_len, &server_config );

#if 1
    /* Get the data from the client and verify username and pass */
    memset(buffer, '\0', sizeof(buffer));
    recv(accept_ret, buffer, 50, 0);

    if(!(strcmp(buffer, "exit"))) {
      /* Exit from the server program */

      break;
    } else {
      printf("Invalid username sent!\n");
      break;
    }
#endif
  
  }

  close(server_socket);

#ifdef TEST_SERVER_CONNECTIONS 
  char buffer[20];
  memset(buffer, '\0', sizeof(buffer));
  strcpy(buffer, "hello world");

  /* Server send test */
  int send_ret = send(accept_ret, buffer, strlen(buffer), 0);
  if(send_ret < 0) {
    perror("SEND");
    exit(0);
  }

  /* Server receive test */
  char buffer[48];
  memset(buffer, '\0', sizeof(buffer));
  recv(accept_ret, buffer, 48, 0);
  printf("buffer: %s\n", buffer);
#endif

  return 0;
}