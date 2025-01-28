CC = gcc
CFLAGS = -Wall

all: server client

server: doencfileserver.c
	$(CC) $(CFLAGS) -o svr doencfileserver.c

client: retrieveencfileclient.c
	$(CC) $(CFLAGS) -o cli retrieveencfileclient.c

runsv: server
	./svr

runcl: client
	./cli

clean:
	rm -f svr cli *.enc 127.0.0.1.*