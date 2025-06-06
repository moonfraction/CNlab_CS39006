/*=====================================
Assignment 6 Submission
Name: Chandransh Singh
Roll number: 22CS30017
=====================================*/

/*
    Steps to compile and run:
    1. make -> to compile server and client
    2. make rs -> to run the server (or run ==> ./s <port>)
    3. make rc -> to run the client in another terminal (or run ==> ./c <server_ip> <port>)
    4. make clean -> to clean the executables
    5. make deepclean -> to clean the executables and the mailbox directory
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
#define MAX_EMAIL_SIZE 4096

// Function prototypes
void send_command(int socket, const char *command);
int receive_response(int socket);
int connect_to_server(const char *ip, const char *port);

int main(int argc, char *argv[]) {
    
    // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int sock = connect_to_server(argv[1], argv[2]);
    char buffer[BUFFER_SIZE];
    
    // Main loop for user interaction
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        printf("> ");
        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            printf("Error reading input\n");
            break;
        }
        
        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Skip empty commands
        if (strlen(buffer) == 0) {
            continue;
        }
        
        // Check for exit command
        if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "QUIT") == 0) {
            send_command(sock, "QUIT");
            receive_response(sock);
            break;
        }
        
        // Check for different command types
        if (strcmp(buffer, "DATA") == 0) {
            printf("Enter message, end with a single dot(.) in a new line: \n");
            char email_body_buf[BUFFER_SIZE] = "";
            char email_body[MAX_EMAIL_SIZE] = "";
            
            // Read and concatenate message line by line
            while (1) {
                // clean the buffer
                memset(email_body_buf, 0, sizeof(email_body_buf));
                if (!fgets(email_body_buf, BUFFER_SIZE, stdin)) {
                    printf("Error reading input\n");
                    break;
                }
                
                // printf("email_body_buf: %s\n", email_body_buf);

                // Check for end of message (a single dot on a line)
                if (strcmp(email_body_buf, ".\n") == 0) {
                    break;
                }
                
                // Check if adding this line would exceed buffer size
                if (strlen(email_body) + strlen(email_body_buf) >= MAX_EMAIL_SIZE) {
                    printf("Email too large, truncating\n");
                    break;
                }
                
                // Concatenate the line to the send buffer
                strcat(email_body, email_body_buf);
            }
    
            // printf("email_body: %s\n", email_body);
            char send_buffer[MAX_EMAIL_SIZE + 10] = "DATA\n";
            strcat(send_buffer, email_body);

            send_command(sock, send_buffer);
            receive_response(sock);

            // the domain is reset to 0 by server
            printf("~~~ State reset to 0: ");
            printf("Start again by sending HELO <email_domain>\n");
        } 
        else {
            // Handle regular commands -> HELO, MAIL FROM, RCPT TO
            send_command(sock, buffer);
            receive_response(sock);
        }
    }
    
    close(sock);
    return 0;
}

int connect_to_server(const char *ip, const char *port) {
    int sock;
    struct sockaddr_in serv_addr;
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    
    // Set up server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    
    // Convert IPv4 address from text to binary form
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to My_SMTP server.\n");
    return sock;
}

void send_command(int socket, const char *command) {
    if (send(socket, command, strlen(command), 0) < 0) {
        perror("Failed to send command");
        exit(EXIT_FAILURE);
    }
    // printf("Sent: %s\n", command);
}

int receive_response(int socket) {
    char buffer[BUFFER_SIZE] = {0};
    // printf("waiting for response\n");
    int bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        perror("Failed to receive server response");
        exit(EXIT_FAILURE);
    }
    
    buffer[bytes_read] = '\0';
    printf("%s\n", buffer);
    
    // Extract response code
    int response_code = 0;
    sscanf(buffer, "%d", &response_code);
    return response_code;
}
