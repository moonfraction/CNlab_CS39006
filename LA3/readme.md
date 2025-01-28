### [⭐️ Beej's Guide to Network Programming Using Internet Sockets ⭐️](https://www.gta.ufrj.br/ensino/eel878/sockets/index.html)

### ✨ The above link is a must read ✨

#### self notes :

A `sockfd` file descriptor is **an integer that uniquely identifies a socket within a process**. This descriptor is used to perform operations on the socket, such as reading from or writing to it. When creating a server, the socket function is called to initialize a `sockfd`. This descriptor is then used with other functions like bind, listen, and accept to set up the server and manage incoming connections.

For example, in a typical server setup, the socket function is called to create a TCP socket descriptor, which is stored in the `sockfd`. The bind function is then used to associate the socket with a specific address and port. The listen function is used to put the socket into a listening state, allowing it to accept incoming connections. Finally, the accept function is used to accept a connection request from a client, which returns a new `sockfd` for the accepted connection.

The `sockfd` is crucial for managing communication over the network and is treated similarly to other file descriptors in Unix-like systems, allowing operations like read and write to be used on sockets as well.

```c
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Cannot create socket");
        exit(0);
    }
```

The following system call opens a socket. The first parameter indicates the family of the protocol to be followed. For internet protocols we use `AF_INET` (this indicates IPv4 address). For TCP sockets the second parameter is `SOCK_STREAM`. The third parameter is set to 0 for user applications.

For UDP socket the second parameter is `SOCK_DGRAM`.

```c
/*
 * Socket address, internet style.
 */
struct sockaddr_in {
	__uint8_t       sin_len;
	sa_family_t     sin_family;
	in_port_t       sin_port;
	struct  in_addr sin_addr;
	char            sin_zero[8];
};

struct in_addr {
	in_addr_t s_addr;
};

```

> - [htons meaning](https://jameshfisher.com/2016/12/21/htons/)
> - also see `man byteorder`


#### Key Differences Without Fork vs With Fork
1. Process Model
    * Without fork: Sequential/iterative server
    * With fork: Concurrent server
2. Client Handling
    * Without fork:
        * One client at a time
        * Other clients must wait in queue
        * Server blocked until current client finishes
    * With fork:
        * Multiple clients simultaneously
        * Each client gets dedicated process
        * Parent continues accepting new connections
        * Children handle individual clients independently

3. Example
```
Without Fork:
Client A connects → Server handles A → Client B waits → A finishes → Server handles B

With Fork:
Client A connects → Child process 1 handles A
Client B connects → Child process 2 handles B (simultaneously)
```

4. Resource Usage
    * Without fork: Lower memory usage, simpler
    * With fork: Higher memory usage, more complex but better scalability

5. Code structure
```c
// Without Fork:
while(1) {
    accept_connection();
    handle_client();  // Blocks until client finished
}

// With Fork:
while(1) {
    accept_connection();
    if(fork() == 0) {
        handle_client();  // Only child process handles client
        exit(0);
    }
    // Parent continues accepting new connections
}
```