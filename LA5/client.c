#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 256
#define SERVER_IP "127.0.0.1"

// Function to evaluate arithmetic expressions
int evaluate_expression(const char *expression) {
    int a, b;
    char op;
    
    // Parse the expression: format "a op b"
    sscanf(expression, "%d %c %d", &a, &op, &b);
    
    // Perform the operation
    switch (op) {
        case '+':
            return a + b;
        case '-':
            return a - b;
        case '*':
            return a * b;
        case '/':
            if (b == 0) {
                fprintf(stderr, "Error: Division by zero\n");
                return 0;
            }
            return a / b;
        default:
            fprintf(stderr, "Error: Unknown operator %c\n", op);
            return 0;
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Set up server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IP address from string to binary
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        return 1;
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }
    
    printf("Connected to the Task Queue Server\n");
    
    // Make socket non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    int active = 1;
    int waiting_for_task = 0;
    
    while (active) {
        if (!waiting_for_task) {
            // Request a task
            memset(buffer, 0, BUFFER_SIZE);
            strcpy(buffer, "GET_TASK");
            
            // Send request using send()
            if (send(sock, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                break;
            }
            
            printf("Requested a task from the server\n");
            waiting_for_task = 1;
            sleep(1); // Give server time to respond
        }
        
        // Clear buffer for server response
        memset(buffer, 0, BUFFER_SIZE);
        
        // Read server response using recv()
        int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available yet, wait a bit
                usleep(500000); // 500ms
                continue;
            } else {
                perror("Receive error");
                break;
            }
        } else if (n == 0) {
            // Server closed connection
            printf("Server disconnected\n");
            break;
        }
        
        buffer[n] = '\0';
        printf("Server response: %s\n", buffer);
        
        // Process server response
        if (strncmp(buffer, "Task:", 5) == 0) {
            // Extract the task (skip "Task: ")
            char *task = buffer + 6;
            printf("Processing task: %s\n", task);
            
            // Evaluate the expression
            int result = evaluate_expression(task);
            printf("Result: %d\n", result);
            
            // Send result back to server
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "RESULT %d", result);
            
            // Send result using send()
            if (send(sock, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                break;
            }
            
            waiting_for_task = 0;
            sleep(1); // Pause before requesting the next task
        } else if (strncmp(buffer, "No tasks available", 18) == 0) {
            printf("No more tasks available, exiting...\n");
            // Send exit message
            memset(buffer, 0, BUFFER_SIZE);
            strcpy(buffer, "exit");
            
            // Send exit message using send()
            if (send(sock, buffer, strlen(buffer), 0) < 0) {
                perror("Send failed");
                break;
            }
            
            active = 0;
        }
    }
    
    close(sock);
    return 0;
}