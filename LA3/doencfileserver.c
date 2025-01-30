/*
=====================================
Assignment 3 Submission
Name: [Your_Name]
Roll number: [Your_Roll_Number]
Link of the pcap file: [Google_Drive_Link_of_the_pcap_File]
=====================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 100

// Function to encrypt a character using the substitution cipher
char encrypt_char(char c, const char* key) {
    if (c >= 'A' && c <= 'Z') {
        return key[c - 'A'];
    } else if (c >= 'a' && c <= 'z') {
        return key[c - 'a'] + ('a' - 'A');
    }
    return c;  // Return unchanged if not a letter
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char key[27] = {0};
    
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server is listening on port %d...\n", PORT);
    
    while (1) {
        // Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        // Get client IP and port
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(address.sin_port);
        
        while (1) {
            // First receive the key
            memset(key, 0, sizeof(key));
            int key_bytes = recv(new_socket, key, 26, 0);
            if (key_bytes <= 0) break;
            key[26] = '\0';
            
            // Create temporary file name using client IP and port
            char temp_filename[100];
            snprintf(temp_filename, sizeof(temp_filename), "%s.%d.txt", client_ip, client_port);
            
            // Open temporary file for writing
            FILE *temp_file = fopen(temp_filename, "w");
            if (!temp_file) {
                perror("Error creating temporary file");
                continue;
            }
            
            // Receive file content in chunks
            while (1) {
                memset(buffer, 0, BUFFER_SIZE);
                int bytes_received = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received <= 0) break;
                
                fwrite(buffer, 1, bytes_received, temp_file);
                
                // Check if this is the end of the file
                if (bytes_received < BUFFER_SIZE - 1) break;
            }
            fclose(temp_file);
            
            // Create encrypted file name
            char enc_filename[120];
            snprintf(enc_filename, sizeof(enc_filename), "%s.enc", temp_filename);
            
            // Open files for encryption
            FILE *input = fopen(temp_filename, "r");
            FILE *output = fopen(enc_filename, "w");
            
            if (!input || !output) {
                perror("Error opening files for encryption");
                if (input) fclose(input);
                if (output) fclose(output);
                continue;
            }
            
            // Perform encryption
            int c;
            while ((c = fgetc(input)) != EOF) {
                fputc(encrypt_char(c, key), output);
            }
            
            fclose(input);
            fclose(output);
            
            // Send encrypted file back to client
            FILE *enc_file = fopen(enc_filename, "r");
            if (!enc_file) {
                perror("Error opening encrypted file");
                continue;
            }
            
            while (!feof(enc_file)) {
                size_t bytes_read = fread(buffer, 1, BUFFER_SIZE - 1, enc_file);
                if (bytes_read > 0) {
                    send(new_socket, buffer, bytes_read, 0);
                }
            }
            
            fclose(enc_file);
            
            // Wait for client's next request
            memset(buffer, 0, BUFFER_SIZE);
            int confirmation = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
            if (confirmation <= 0 || strcasecmp(buffer, "No") == 0) {
                break;
            }
        }
        
        close(new_socket);
        printf("Connection with client closed\n");
    }
    
    return 0;
}