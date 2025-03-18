CC = gcc
CFLAGS = -Wall -pthread

all: mysmtp_server mysmtp_client

mysmtp_server: mysmtp_server.c
	$(CC) $(CFLAGS) -o s mysmtp_server.c

mysmtp_client: mysmtp_client.c
	$(CC) $(CFLAGS) -o c mysmtp_client.c

clean:
	rm -f s c

deepclean: clean
	rm -rf mailbox