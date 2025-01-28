/*=====================================
Assignment 3 Submission
Name: Chandransh Singh
Roll number: 22CS30017
Link of the pcap file:
=====================================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>


#define CHUNK_SIZE 100

// Function to encrypt the file using substitution cipher
void encrypt_file(const char* input_file, const char* output_file, const char* key) {
    FILE *fin = fopen(input_file, "r");
    FILE *fout = fopen(output_file, "w");
    int c;
    
    while ((c = fgetc(fin)) != EOF) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            int idx = (c >= 'a') ? (c - 'a') : (c - 'A');
            char mapped = key[idx];
            fputc((c >= 'a') ? (mapped + 32) : mapped, fout);
        } else {
            fputc(c, fout);
        }
    }
    
    fclose(fin);
    fclose(fout);
}

int main() {
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    char buffer[CHUNK_SIZE];
    socklen_t clilen;
    
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Cannot create socket");
        exit(0);
    }
    
    // Initialize server address structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(20000);
    
    // Bind socket
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to bind local address");
        exit(0);
    }
    
    // Listen for connections
    listen(sockfd, 5);
    
    while (1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
        
        if (newsockfd < 0) {
            perror("Accept error");
            continue;
        }

        // Get client information
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        getpeername(newsockfd, (struct sockaddr*)&peer_addr, &peer_len);
        
        // Get current time
        time_t now = time(NULL);
        char time_str[26];
        ctime_r(&now, time_str);
        time_str[24] = '\0'; // Remove newline

        // Print connection info
        printf("[%s] New client connected from %s:%d\n", 
               time_str,
               inet_ntoa(peer_addr.sin_addr), 
               ntohs(peer_addr.sin_port));

        
        while (1) {
            // Receive the key first
            char key[27];
            memset(key, 0, sizeof(key));
            if (recv(newsockfd, key, 26, 0) <= 0) break;
            
            // Create temporary filename using client's IP and port
            char temp_filename[100];
            snprintf(temp_filename, sizeof(temp_filename), "%s.%d.txt", 
                    inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            
            // Open temporary file for writing
            FILE *temp_file = fopen(temp_filename, "w");
            
            // Receive file content in chunks
            int bytes_received;
            while ((bytes_received = recv(newsockfd, buffer, CHUNK_SIZE - 1, 0)) > 0) {
                buffer[bytes_received] = '\0';
                if (bytes_received < CHUNK_SIZE - 1) {  // Last chunk
                    fputs(buffer, temp_file);
                    break;
                }
                fputs(buffer, temp_file);
            }
            fclose(temp_file);
            
            // Create encrypted filename
            char enc_filename[120];
            snprintf(enc_filename, sizeof(enc_filename), "%s.enc", temp_filename);
            
            // Encrypt the file
            encrypt_file(temp_filename, enc_filename, key);
            
            // Send encrypted file back to client
            FILE *enc_file = fopen(enc_filename, "r");
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, CHUNK_SIZE - 1, enc_file)) > 0) {
                send(newsockfd, buffer, bytes_read, 0);
                if (bytes_read < CHUNK_SIZE - 1) {  // Last chunk
                    break;
                }
            }
            fclose(enc_file);
            
            // Remove temporary files
            // remove(temp_filename);
            // remove(enc_filename);
            
            // Check if client wants to continue
            memset(buffer, 0, CHUNK_SIZE);
            if (recv(newsockfd, buffer, CHUNK_SIZE - 1, 0) <= 0) break;
            if (strcmp(buffer, "No") == 0) {
                // Print disconnection info
                printf("[%s] Client disconnected from %s:%d\n",
                       time_str,
                       inet_ntoa(peer_addr.sin_addr),
                       ntohs(peer_addr.sin_port)
                    );
                break;
            }
        }
        
        close(newsockfd);
    }
    
    return 0;
}