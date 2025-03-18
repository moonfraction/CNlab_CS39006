#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define MAX_EMAIL_SIZE 4096

typedef struct {
    int socket;
    struct sockaddr_in address;
} client_t;

// Function prototypes
void *handle_client(void *arg);
void send_response(int socket, int code, const char *message);
int store_email(const char *recipient, const char *sender, const char *message);
void list_emails(int client_socket, const char *recipient);
void get_email(int client_socket, const char *recipient, int id);

int main(int argc, char *argv[]) {
    int server_fd, opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Check if port number is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt))) {
        perror("Setsockopt failed for SO_REUSEADDR");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Try to set SO_REUSEPORT but don't fail if not available
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, 
                   &opt, sizeof(opt)) < 0) {
        perror("Warning: SO_REUSEPORT not supported");
    }
    
    // Bind to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(argv[1]));
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %s...\n", argv[1]);
    
    // Create directory for mailboxes if it doesn't exist
    mkdir("mailbox", 0777);
    
    // Accept connections and handle them
    while (1) {
        client_t *client = malloc(sizeof(client_t));
        socklen_t client_size = sizeof(client->address);
        
        client->socket = accept(server_fd, (struct sockaddr *)&client->address, &client_size);
        
        if (client->socket < 0) {
            perror("Accept failed");
            free(client);
            continue;
        }
        
        printf("Client connected: %s\n", inet_ntoa(client->address.sin_addr));
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) != 0) {
            perror("Thread creation failed");
            close(client->socket);
            free(client);
            continue;
        }
        
        // Detach thread to clean up automatically
        pthread_detach(thread_id);
    }
    
    close(server_fd);
    return 0;
}

void *handle_client(void *arg) {
    client_t *client = (client_t *)arg;
    int client_socket = client->socket;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    char sender[100] = "";
    char recipient[100] = "";
    int state = 0;  // 0: initial, 1: HELO received, 2: MAIL FROM received, 3: RCPT TO received, 4: DATA mode
    
    // Communication loop
    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Remove newline character
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        printf("Received command: %s\n", buffer);
        
        if (state == 4) {  // DATA mode
            char email_body[MAX_EMAIL_SIZE] = "";
            strcat(email_body, buffer);
            strcat(email_body, "\n");
            
            // Continue receiving until a single dot on a line
            while (1) {
                bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_read <= 0) break;
                
                buffer[bytes_read] = '\0';
                
                if (strcmp(buffer, ".\n") == 0 || strcmp(buffer, ".\r\n") == 0 || strcmp(buffer, ".") == 0) {
                    break;  // End of message
                }
                
                // Append to email body
                strcat(email_body, buffer);
            }
            
            if (store_email(recipient, sender, email_body) == 0) {
                send_response(client_socket, 200, "Message stored successfully");
                printf("DATA received, message stored.\n");
            } else {
                send_response(client_socket, 500, "Failed to store message");
            }
            
            state = 1;  // Back to command mode after DATA
            continue;
        }
        
        // Process commands
        if (strncmp(buffer, "HELO ", 5) == 0) {
            char client_id[100];
            if (sscanf(buffer, "HELO %s", client_id) == 1) {
                send_response(client_socket, 200, "OK");
                printf("HELO received from %s\n", client_id);
                state = 1;
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } else if (strncmp(buffer, "MAIL FROM: ", 11) == 0) {
            if (state < 1) {
                send_response(client_socket, 400, "Send HELO first");
            } else if (sscanf(buffer, "MAIL FROM: %s", sender) == 1) {
                send_response(client_socket, 200, "OK");
                printf("MAIL FROM: %s\n", sender);
                state = 2;
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } else if (strncmp(buffer, "RCPT TO: ", 9) == 0) {
            if (state < 2) {
                send_response(client_socket, 400, "Send MAIL FROM first");
            } else if (sscanf(buffer, "RCPT TO: %s", recipient) == 1) {
                send_response(client_socket, 200, "OK");
                printf("RCPT TO: %s\n", recipient);
                state = 3;
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } else if (strcmp(buffer, "DATA") == 0) {
            if (state < 3) {
                send_response(client_socket, 400, "Send RCPT TO first");
            } else {
                send_response(client_socket, 200, "Enter message, end with a single dot");
                state = 4;  // Switch to DATA mode
            }
        } else if (strncmp(buffer, "LIST ", 5) == 0) {
            char email[100];
            if (sscanf(buffer, "LIST %s", email) == 1) {
                printf("LIST %s\n", email);
                list_emails(client_socket, email);
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } else if (strncmp(buffer, "GET_MAIL ", 9) == 0) {
            char email[100];
            int id;
            if (sscanf(buffer, "GET_MAIL %s %d", email, &id) == 2) {
                printf("GET_MAIL %s %d\n", email, id);
                get_email(client_socket, email, id);
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } else if (strcmp(buffer, "QUIT") == 0) {
            send_response(client_socket, 200, "Goodbye");
            printf("Client disconnected.\n");
            break;
        } else {
            send_response(client_socket, 400, "Unknown command");
        }
    }
    
    close(client_socket);
    free(client);
    return NULL;
}

void send_response(int socket, int code, const char *message) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "%d %s\n", code, message);
}

int store_email(const char *recipient, const char *sender, const char *message) {
    char filename[256];
    snprintf(filename, sizeof(filename), "mailbox/%s.txt", recipient);
    
    FILE *file = fopen(filename, "a");
    if (!file) {
        perror("Failed to open mailbox file");
        return -1;
    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[20];
    strftime(date, sizeof(date), "%d-%m-%Y", t);
    
    fprintf(file, "===START_EMAIL===\n");
    fprintf(file, "From: %s\n", sender);
    fprintf(file, "Date: %s\n", date);
    fprintf(file, "%s", message);
    fprintf(file, "\n===END_EMAIL===\n\n");
    
    fclose(file);
    return 0;
}

void list_emails(int client_socket, const char *recipient) {
    char filename[256];
    snprintf(filename, sizeof(filename), "mailbox/%s.txt", recipient);
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        send_response(client_socket, 401, "No emails found for this recipient");
        return;
    }
    
    send_response(client_socket, 200, "OK");
    
    char line[BUFFER_SIZE];
    char email_list[MAX_EMAIL_SIZE] = "";
    int id = 1;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "===START_EMAIL===", 17) == 0) {
            char from[100] = "";
            char date[20] = "";
            
            // Get From line
            fgets(line, sizeof(line), file);
            sscanf(line, "From: %s", from);
            
            // Get Date line
            fgets(line, sizeof(line), file);
            sscanf(line, "Date: %s", date);
            
            // Add to email list
            char email_entry[200];
            snprintf(email_entry, sizeof(email_entry), "%d: Email from %s (%s)\n", id, from, date);
            strcat(email_list, email_entry);
            id++;
        }
    }
    
    fclose(file);
    send(client_socket, email_list, strlen(email_list), 0);
    printf("Emails retrieved; list sent.\n");
}

void get_email(int client_socket, const char *recipient, int id) {
    char filename[256];
    snprintf(filename, sizeof(filename), "mailbox/%s.txt", recipient);
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        send_response(client_socket, 401, "No emails found for this recipient");
        return;
    }
    
    char line[BUFFER_SIZE];
    char email_content[MAX_EMAIL_SIZE] = "";
    int current_id = 0;
    int found = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "===START_EMAIL===", 17) == 0) {
            current_id++;
            
            if (current_id == id) {
                found = 1;
                
                // Copy From line
                fgets(line, sizeof(line), file);
                strcat(email_content, line);
                
                // Copy Date line
                fgets(line, sizeof(line), file);
                strcat(email_content, line);
                
                // Copy message body until ===END_EMAIL===
                while (fgets(line, sizeof(line), file)) {
                    if (strncmp(line, "===END_EMAIL===", 15) == 0) {
                        break;
                    }
                    strcat(email_content, line);
                }
                
                break;
            }
        }
    }
    
    fclose(file);
    
    if (found) {
        send_response(client_socket, 200, "OK");
        send(client_socket, email_content, strlen(email_content), 0);
        printf("Email with id %d sent.\n", id);
    } else {
        send_response(client_socket, 401, "Email not found");
    }
}