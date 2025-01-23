CC = gcc
CFLAGS = -Wall

all: server client

server: wordserver.c
	$(CC) $(CFLAGS) -o wordserver wordserver.c

client: wordclient.c
	$(CC) $(CFLAGS) -o wordclient wordclient.c

rsv: server
	./wordserver

rcl: client
	./wordclient

clean:
	rm -f wordserver wordclient received_*