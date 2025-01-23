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
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5000
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // input filename
    printf("Enter the filename to request: ");
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character
    // store the filename in output_filename

    char received_filename[BUFFER_SIZE];
    snprintf(received_filename, BUFFER_SIZE, "received_%s", buffer);  // Create received filename

    // Send filename to server
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&server_addr, addr_len);

    // Receive responses from server
    FILE *output_file = NULL;
    int word_count = 0;
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);

        if (strncmp(buffer, "NOTFOUND", 8) == 0) {
            printf("+++ File %s not found on server.\n", buffer + 9);
            break;
        } else if (strncmp(buffer, "HELLO", 5) == 0) {
            // Create a new file for writing
            output_file = fopen(received_filename, "w");
            if (!output_file) {
                perror("### Failed to create output file");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            printf("+++ Receiving file from server...\n");
            fprintf(output_file, "%s\n", buffer);  // Write HELLO

        } else if (strcmp(buffer, "FINISH") == 0) {
            fprintf(output_file, "%s\n", buffer);  // Write FINISH
            printf("==> File transfer complete.\n");
            printf("==> File saved as %s\n", received_filename);
            break;
        } else {
            // Write received word to file
            fprintf(output_file, "%s\n", buffer);
        }
        printf("+++ Received word %d: %s\n", word_count, buffer);

        char request[BUFFER_SIZE];
        sprintf(request, "WORD%d", ++word_count);
        sendto(sockfd, request, strlen(request), 0, (struct sockaddr *)&server_addr, addr_len);
    }

    if (output_file) {
        fclose(output_file);
    }

    close(sockfd);
    return 0;
}
