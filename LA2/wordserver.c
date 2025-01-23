// =====================================
// Assignment 2: UDP Sockets Submission: 
// Name: Chandransh Singh
// Roll number: 22CS30017
// Link of the pcap file: https://drive.google.com/file/d/1EYTn8NapdMjklnCubh9ia7iAl28YT12_/view
// =====================================


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFFER_SIZE 1024

/*
// from /usr/include/netinet/in.h

struct sockaddr_in {
	__uint8_t       sin_len;
	sa_family_t     sin_family;
	in_port_t       sin_port;
	struct  in_addr sin_addr;
	char            sin_zero[8];
};
*/

int main() {
    int sockfd; //The socket file descriptor from which data is to be received
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(cliaddr);
    char buffer[BUFFER_SIZE];
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server is running on port %d ...\n", PORT);
    

    // The recvfrom function returns:
    //      The number of bytes received in the incoming message.
    //      If the function fails, it returns -1, and errno is set to indicate the error
    //      If no messages are available at the socket, the receive call waits for a message to arrive, unless the socket is nonblocking.

    int n;

    // recvfrom(sockid, recvBuf, bufLen, flags, &clientAddr, addrlen);
    // sendto(sockid, msg, msgLen, flags, &foreignAddr, addrlen);
    // Calls are blocking -> returns only after data is sent / received

    // Receive filename
    n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
    buffer[n] = '\0';
    printf("Received filename: %s\n", buffer);
    

    // Check if file exists
    FILE *fp = fopen(buffer, "r");
    if (fp == NULL) {
        char notfound[BUFFER_SIZE];
        sprintf(notfound, "NOTFOUND %s", buffer);
        sendto(sockfd, notfound, strlen(notfound), 0, (struct sockaddr *)&cliaddr, len);
        printf("### File %s not found.\n", buffer);
        close(sockfd);
        return 0;
    }
    
    // Send HELLO
    char line[BUFFER_SIZE];
    fgets(line, BUFFER_SIZE, fp);
    strtok(line, "\n");
    sendto(sockfd, line, strlen(line), 0, (struct sockaddr *)&cliaddr, len);
    
    // Process WORD_i requests
    while (1) {
        n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0';
        
        if (strncmp(buffer, "WORD", 4) == 0) {
            if (fgets(line, BUFFER_SIZE, fp) != NULL) {
                strtok(line, "\n");
                sendto(sockfd, line, strlen(line), 0, (struct sockaddr *)&cliaddr, len);
                if (strcmp(line, "FINISH") == 0) {
                    printf("==> File transfer complete.\n");
                    break;
                }
            }
        }
    }
    
    fclose(fp);
    close(sockfd);
    return 0;
}