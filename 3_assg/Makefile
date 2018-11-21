INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CFLAGS= $(INCLUDE_DIRS) -g -Werror 
LIBS= 

HFILES= 
CFILES= webproxy.c 

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	webproxy

clean:
	-rm -f *.o *.d
	-rm -f webproxy

webproxy: webproxy.c
	$(CC) $(CFLAGS) $< -o  $@ -lssl -lcrypto



