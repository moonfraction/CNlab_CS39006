/*=====================================
Assignment 3 Submission
Name: Chandransh Singh
Roll number: 22CS30017
Link of the pcap file:
https://drive.google.com/file/d/1NIbnNK2-qJKLXK7qOPr4HyLel8Lc0ICi/view?usp=sharing
=====================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_CHUNK 80 // different from the client, and it does not know
#define SERVER_PORT 8888
#define BUFFER_SIZE 100

struct transfer_stats {
    int chunk_count;
    size_t total_bytes;
};

// Function to handle zombie processes
void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Encrypts a single character using the substitution key
char encrypt_ch(char input, const char* key) {
    if (input >= 'A' && input <= 'Z') {
        return key[input - 'A'];
    }
    if (input >= 'a' && input <= 'z') {
        // Convert to uppercase, encrypt, then back to lowercase
        return key[input - 'a'] - 'A' + 'a';
    }
    return input;
}

void print_transfer_info(const char* ip, int port, int chunk_num, size_t bytes) {
    printf("[%s:%d] chunk #%d: %zu bytes\n", ip, port, chunk_num, bytes);
}

void print_transfer_summary(struct transfer_stats stats, const char* direction) {
    printf("--> Total chunks %s: %d, total bytes: %zu\n\n", direction, stats.chunk_count, stats.total_bytes);
}

// Handle individual client connection
void handle_client(int client_sock, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE] = {0};
    char encryption_key[27] = {0};
    char temp_filename[64], enc_filename[64];
    FILE *input_file, *output_file;
    
    // Get client details for filename
    char client_ip[INET_ADDRSTRLEN];
    // man net_ntop : converts binary IP to string -- network to presentation
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    while (1) {
        // Receive encryption key
        if (recv(client_sock, encryption_key, 26, 0) <= 0) {
            break;
        }
        encryption_key[26] = '\0';
        
        // Create unique temporary filenames
        snprintf(temp_filename, sizeof(temp_filename), "%s.%d.txt", 
                client_ip, ntohs(client_addr.sin_port));
        snprintf(enc_filename, sizeof(enc_filename), "%s.enc", temp_filename);
        
        // Open files for reading/writing
        input_file = fopen(temp_filename, "w");
        if (!input_file) {
            perror("Failed to create temp file");
            continue;
        }
        
        // Receive and store original content with transfer statistics
        struct transfer_stats recv_stats = {0, 0};
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            ssize_t bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) break;
            
            if (strncmp(buffer, "END_OF_FILE", 11) == 0) break;
            
            recv_stats.chunk_count++;
            recv_stats.total_bytes += bytes_received;
            print_transfer_info(client_ip, ntohs(client_addr.sin_port), 
                      recv_stats.chunk_count, bytes_received);
                      
            fprintf(input_file, "%s", buffer);
        }
        print_transfer_summary(recv_stats, "received");
        fclose(input_file);
        
        // Process file and create encrypted version
        input_file = fopen(temp_filename, "r");
        output_file = fopen(enc_filename, "w");
        
        if (!input_file || !output_file) {
            perror("File operation failed");
            if (input_file) fclose(input_file);
            if (output_file) fclose(output_file);
            continue;
        }
        
        // Perform encryption
        int ch;
        while ((ch = fgetc(input_file)) != EOF) {
            fputc(encrypt_ch(ch, encryption_key), output_file);
        }
        
        fclose(input_file);
        fclose(output_file);
        
        // Send encrypted file back to client
        output_file = fopen(enc_filename, "r");
        if (!output_file) {
            perror("Cannot open encrypted file");
            continue;
        }
        
        struct transfer_stats send_stats = {0, 0};
        size_t bytes_read;
        
        while ((bytes_read = fread(buffer, 1, MAX_CHUNK - 1, output_file)) > 0) {
            send(client_sock, buffer, bytes_read, 0);
            send_stats.chunk_count++;
            send_stats.total_bytes += bytes_read;
            print_transfer_info(client_ip, ntohs(client_addr.sin_port), 
                      send_stats.chunk_count, bytes_read);
            usleep(1000);  // Small delay to prevent flooding
        }
        
        send(client_sock, "END_OF_FILE", 11, 0);
        print_transfer_summary(send_stats, "sent");
        fclose(output_file);

        printf("------------------------------------------------------------\n");

        // Check if client wants to continue
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(client_sock, buffer, BUFFER_SIZE - 1, 0) <= 0 || 
            strcmp(buffer, "No") == 0) {
            break;
        }
    }
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(client_sock, (struct sockaddr*)&addr, &addr_len);
    printf("Client disconnected from %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    
    close(client_sock);
    exit(0);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Setup signal handler for zombie processes
    signal(SIGCHLD, handle_sigchld);
    
    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Enable address reuse
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    // Listen for connections
    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        exit(1);
    }
    
    printf("Server listening on port %d...\n", SERVER_PORT);
    
    // Accept and handle client connections
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("\n+++ New client connected: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // Fork to handle client
        if (fork() == 0) {
            close(server_sock);  // Child doesn't need server socket
            handle_client(client_sock, client_addr);
        }
        
        close(client_sock);  // Parent doesn't need client socket
    }
    
    return 0;
}