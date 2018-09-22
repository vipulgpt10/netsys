INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CFLAGS= $(INCLUDE_DIRS) -g -Werror
LIBS= 

HFILES= 
CFILES= udp_client.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	client

clean:
	-rm -f *.o *.d
	-rm -f client

client: udp_client.c 
	$(CC) $(CFLAGS) $(SRCS) -o  $@


