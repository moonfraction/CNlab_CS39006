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
void receive_response(int socket);
void handle_data_mode(int socket);

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    
    // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    
    // Set up server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    
    // Convert IPv4 address from text to binary form
    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to My_SMTP server.\n");
    
    // Main loop for user interaction
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);
        
        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Check for exit command
        if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "QUIT") == 0) {
            send_command(sock, "QUIT");
            receive_response(sock);
            break;
        }
        
        // Check if the command is DATA
        if (strcmp(buffer, "DATA") == 0) {
            send_command(sock, buffer);
            receive_response(sock);
            handle_data_mode(sock);
        } else {
            // Send regular command
            send_command(sock, buffer);
            receive_response(sock);
        }
    }
    
    close(sock);
    return 0;
}

void send_command(int socket, const char *command) {
    send(socket, command, strlen(command), 0);
    send(socket, "\n", 1, 0); // Send newline
}

void receive_response(int socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    // First receive the standard response line
    memset(buffer, 0, sizeof(buffer));
    bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        perror("Server disconnected");
        exit(EXIT_FAILURE);
    }
    
    buffer[bytes_read] = '\0';
    printf("%s", buffer);
    
    // Check if this is a response that includes more data (like LIST or GET_MAIL)
    int response_code;
    if (sscanf(buffer, "%d", &response_code) == 1 && response_code == 200) {
        // If the command was LIST or GET_MAIL and successful, there might be more data
        if (strstr(buffer, "OK\n") != NULL) {
            // Continue receiving until we get all data
            // This is a simplistic approach and might need improvement for large responses
            while ((bytes_read = recv(socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
                buffer[bytes_read] = '\0';
                printf("%s", buffer);
                
                // Simple check to see if we've received everything
                if (bytes_read < (sizeof(buffer) - 1)) {
                    break;
                }
            }
        }
    }
}

void handle_data_mode(int socket) {
    char buffer[BUFFER_SIZE];
    char email_body[MAX_EMAIL_SIZE] = "";
    
    printf("Enter your message (end with a single dot '.'):\n");
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, BUFFER_SIZE, stdin);
        
        // Check for end of message
        if (strcmp(buffer, ".\n") == 0 || strcmp(buffer, ".") == 0) {
            send_command(socket, ".");
            break;
        }
        
        // Add to email body
        strcat(email_body, buffer);
    }
    
    // Send the email body
    send(socket, email_body, strlen(email_body), 0);
    
    // Get response
    receive_response(socket);
}