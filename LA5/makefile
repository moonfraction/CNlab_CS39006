all: server client
server: server.c
	gcc -o server server.c -lpthread
client: client.c
	gcc -o client client.c -lpthread
clean:
	rm -f server client
rs: server
	./server tasks.txt
rc: client
	./client