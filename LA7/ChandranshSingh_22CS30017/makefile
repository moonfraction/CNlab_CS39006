CC = gcc
CFLAGS = -Wall

all: cldp_server cldp_client

cldp_server: cldp_server.c
	$(CC) $(CFLAGS) -o s cldp_server.c

cldp_client: cldp_client.c
	$(CC) $(CFLAGS) -o c cldp_client.c

rs: cldp_server
	sudo ./s

rc: cldp_client
	sudo ./c

clean:
	rm -f s c