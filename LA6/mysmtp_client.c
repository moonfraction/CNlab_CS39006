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
void handle_data_mode(int socket);
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
            send_command(sock, buffer);
            int response_code = receive_response(sock);
            
            // Only proceed with data entry if the server accepted DATA command
            if (response_code == 200) {
                handle_data_mode(sock);
            }
        } 
        else if (strncmp(buffer, "LIST ", 5) == 0 || strncmp(buffer, "GET_MAIL ", 9) == 0) {
            // Special handling for LIST and GET_MAIL to receive multi-line responses
            send_command(sock, buffer);
            int response_code = receive_response(sock);
            
            // If successful, receive additional data
            if (response_code == 200) {
                char data_buffer[MAX_EMAIL_SIZE] = {0};
                int bytes_read = 0;
                int total_bytes = 0;
                
                // Give server a moment to prepare data
                usleep(100000);  // 100ms
                
                // Read all available data
                while ((bytes_read = recv(sock, data_buffer + total_bytes, 
                                         sizeof(data_buffer) - total_bytes - 1, MSG_DONTWAIT)) > 0) {
                    total_bytes += bytes_read;
                    data_buffer[total_bytes] = '\0';
                    
                    // Safety check against buffer overflow
                    if (total_bytes >= sizeof(data_buffer) - 1) {
                        break;
                    }
                }
                
                // If we received any additional data, display it
                if (total_bytes > 0) {
                    printf("%s", data_buffer);
                }
            }
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
void handle_data_mode(int socket) {
    char buffer[BUFFER_SIZE] = {0};
    
    // printf("Enter your message (end with a single dot '.' on a new line):\n");
    
    // Read and send message line by line
    while (1) {
        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            printf("Error reading input\n");
            return;
        }
        
        // Check for end of message (a single dot on a line)
        if (strcmp(buffer, ".\n") == 0 || strcmp(buffer, ".\r\n") == 0 || strcmp(buffer, ".") == 0) {
            // Send the terminating dot
            send_command(socket, ".");
            break;
        }
        
        // Send each line immediately
        if (send(socket, buffer, strlen(buffer), 0) < 0) {
            perror("Failed to send email content");
            return;
        }
    }
    
    // Get final response
    receive_response(socket);
}
