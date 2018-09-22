INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CFLAGS= $(INCLUDE_DIRS) -g -Werror 
LIBS= 

HFILES= 
CFILES= udp_server.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	server

clean:
	-rm -f *.o *.d
	-rm -f server

server: udp_server.c
	$(CC) $(CFLAGS) $(SRCS) -o  $@ 

