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
void recv_email_body(int socket, char *recipient, char *sender);

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
    
    if (bind(server_fd, (struct sockaddr *)&address, addrlen) < 0) {
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
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        

        // printf("Waiting for a client to connect...\n");
        int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        // printf("Client port: %d\n", ntohs(client_addr.sin_port));
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&client_socket) != 0) {
            perror("Thread creation failed");
            close(client_socket);
            continue;
        }
        
        // Detach thread to clean up automatically
        pthread_detach(thread_id);
    }
    
    close(server_fd);
    return 0;
}

void *handle_client(void *arg) {
    // printf("Thread created.\n");
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    char sender[100] = "";
    char recipient[100] = "";
    int state = 0;  // 0: initial, 1: HELO received, 2: MAIL FROM received, 3: RCPT TO received, 4: DATA mode
    char email_domain[100] = "";
    
    // Communication loop
    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Remove newline character
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        // printf("Received command: %s, state: %d\n", buffer, state); // for debugging
        
        // Process commands
        if (strncmp(buffer, "HELO ", 5) == 0) {
            if (sscanf(buffer, "HELO %s", email_domain) == 1) {
                // printf("email domain set to %s\n", email_domain);
                send_response(client_socket, 200, "OK");
                printf("HELO received from %s\n", email_domain);
                state = 1;
            } 
            else send_response(client_socket, 400, "Invalid syntax");
        } 
        else if (strncmp(buffer, "MAIL FROM: ", 11) == 0) {
            if (state < 1) send_response(client_socket, 403, "Send HELO first");    
            else if (sscanf(buffer, "MAIL FROM: %s", sender) == 1) {
                // Check if the sender is from the same domain
                char* at_sign = strchr(sender, '@');
                if (at_sign != NULL) {
                    char* sender_domain = at_sign + 1;
                    // Compare sender's domain with the email domain from HELO
                    if (strcmp(sender_domain, email_domain) == 0) {
                        send_response(client_socket, 200, "OK");
                        printf("MAIL FROM: %s\n", sender);
                        state = 2;
                    } 
                    else send_response(client_socket, 400, "Sender domain does not match HELO domain, you may set the sender domain again with HELO");
                } 
                else send_response(client_socket, 400, "Invalid email format, missing @");
            } 
            else send_response(client_socket, 400, "Invalid syntax");
        } 
        else if (strncmp(buffer, "RCPT TO: ", 9) == 0) {
            if (state < 2) {
                send_response(client_socket, 403, "Send MAIL FROM first");
            } else if (sscanf(buffer, "RCPT TO: %s", recipient) == 1) {
                send_response(client_socket, 200, "OK");
                printf("RCPT TO: %s\n", recipient);
                state = 3;
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } 
        else if (strcmp(buffer, "DATA") == 0) {
            if (state < 3) {
                send_response(client_socket, 403, "Send RCPT TO first");
            } else {
                send_response(client_socket, 200, "Enter message, end with a single dot(.) in a new line");
                char email_body[MAX_EMAIL_SIZE] = "";
                int body_len = recv(client_socket, email_body, MAX_EMAIL_SIZE - 1, 0);
                if (body_len < 0) {
                    perror("Failed to receive email body");
                    send_response(client_socket, 500, "Failed to receive email body");
                }
                else{
                    if (store_email(recipient, sender, email_body) == 0) {
                        send_response(client_socket, 200, "Message stored successfully");
                        printf("DATA received, message stored.\n");
                    } else {
                        send_response(client_socket, 500, "Failed to store message");
                    }
                    state = 0;
                    printf("~~~ State reset to 0: ");
                    printf("Start again by sending HELO <email_domain>\n");
                }
            }
        } 
        else if (strncmp(buffer, "LIST ", 5) == 0) {
            char email[100];
            if (sscanf(buffer, "LIST %s", email) == 1) {
                printf("LIST %s\n", email);
                list_emails(client_socket, email);
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } 
        else if (strncmp(buffer, "GET_MAIL ", 9) == 0) {
            char email[100];
            int id;
            if (sscanf(buffer, "GET_MAIL %s %d", email, &id) == 2) {
                printf("GET_MAIL %s %d\n", email, id);
                get_email(client_socket, email, id);
            } else {
                send_response(client_socket, 400, "Invalid syntax");
            }
        } 
        else if (strcmp(buffer, "QUIT") == 0) {
            send_response(client_socket, 200, "Goodbye");
            printf("Client disconnected.\n");
            break;
        } 
        else send_response(client_socket, 400, "Unknown command");
    }
    
    close(client_socket);
    return NULL;
}

void send_response(int socket, int code, const char *message) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "%d %s", code, message);
    send(socket, response, strlen(response), 0);
    // printf("Sent response: %s", response);
}

void recv_email_body(int socket, char *recipient, char *sender) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char email_body[MAX_EMAIL_SIZE] = "";
    
    // Receive the email content line by line
    while (1) {
        // clear buffer
        memset(buffer, 0, sizeof(buffer));
        bytes_read = recv(socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) break;
        // printf("Received: %s\n", buffer);
        buffer[bytes_read] = '\0';
        
        // Check for end of message marker
        if (strcmp(buffer, ".") == 0) {
            // printf("End of message\n");
            break;  // End of message
        }
        
        // Append to email body
        strcat(email_body, buffer);
    }
    
    if (store_email(recipient, sender, email_body) == 0) {
        send_response(socket, 200, "Message stored successfully");
        printf("DATA received, message stored.\n");
    } else {
        send_response(socket, 500, "Failed to store message");
    }
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
    fprintf(file, "===END_EMAIL===\n\n");
    
    fclose(file);
    return 0;
}

void list_emails(int client_socket, const char *recipient) {
    char filename[256];
    snprintf(filename, sizeof(filename), "mailbox/%s.txt", recipient);
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        send_response(client_socket, 401, "Requested email does not exist");
        return;
    }
    
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
    
    // Combine status code and email list in a single response
    char response[MAX_EMAIL_SIZE + 100];
    email_list[strlen(email_list) - 1] = '\0';
    snprintf(response, sizeof(response), "OK\n%s", email_list);
    send_response(client_socket, 200, response);
    
    printf("Emails retrieved; list sent.\n");
}

void get_email(int client_socket, const char *recipient, int id) {
    char filename[256];
    snprintf(filename, sizeof(filename), "mailbox/%s.txt", recipient);
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        send_response(client_socket, 401, "Requested email does not exist");
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
        // Combine status and email content in a single response
        char response[MAX_EMAIL_SIZE + 100];
        email_content[strlen(email_content) - 1] = '\0';
        snprintf(response, sizeof(response), "OK\n%s", email_content);
        send_response(client_socket, 200, response);
        printf("Email with id %d sent.\n", id);
    } else {
        send_response(client_socket, 401, "Email not found");
    }
}