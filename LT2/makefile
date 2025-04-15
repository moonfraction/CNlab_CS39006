all: server client

server: tcp_echo_server.c
	gcc tcp_echo_server.c -o s

client: tcp_client.c
	gcc tcp_client.c -o c

clean: 
	rm -f s c