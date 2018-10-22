INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CFLAGS= $(INCLUDE_DIRS) -g -Werror 
LIBS= 

HFILES= 
CFILES= tcpServer.c 

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	tcpServer

clean:
	-rm -f *.o *.d
	-rm -f tcpServer

server: tcpServer.c
	$(CC) $(CFLAGS) $< -o  $@ 


