CC = gcc
CFLAGS = -Wall -pthread

all: mysmtp_server mysmtp_client

mysmtp_server: mysmtp_server.c
	$(CC) $(CFLAGS) -o s mysmtp_server.c

mysmtp_client: mysmtp_client.c
	$(CC) $(CFLAGS) -o c mysmtp_client.c

rs: mysmtp_server
	./s 5050

rc: mysmtp_client
	./c 127.0.0.1 5050

clean:
	rm -f s c

deepclean: clean
	rm -rf mailbox