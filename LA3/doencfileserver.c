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
#include <signal.h>

#define CHUNK_SIZE 100

// Global variables
volatile int active_children = 0;

// Signal handler for child termination
void handle_child_exit(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        active_children--;
    }
    
    if (active_children == 0) {
        char choice;
        printf("\nAll clients disconnected. Wait for new clients? (y/n): ");
        scanf(" %c", &choice);
        
        if (choice == 'n' || choice == 'N') {
            printf("Server shutting down...\n");
            exit(0);
        }
        printf("Waiting for new connections...\n");
    }
}

void handle_client_disconnect(int sockfd, struct sockaddr_in *peer_addr) {
    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[24] = '\0';
    
    printf("!!! [%s] Client %s:%d disconnected unexpectedly\n",
           time_str,
           inet_ntoa(peer_addr->sin_addr),
           ntohs(peer_addr->sin_port));
}


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
    // Register signal handler for child termination
    signal(SIGCHLD, handle_child_exit);

    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    char buffer[CHUNK_SIZE];
    socklen_t clilen;
    
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Cannot create socket");
        exit(0);
    }

    // Set socket options to reuse address
    // to avoid "Address already in use" error while binding
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
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
    // up to 5 concurrent client requests will be queued up
    listen(sockfd, 5);

    // Get current time
    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[24] = '\0'; // Remove newline
    
    // Print server start message
    printf("[%s] Server started. Listening on port %d...\n", 
           time_str, ntohs(serv_addr.sin_port));
    
    
    while (1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
        
        if (newsockfd < 0) {
            perror("Accept error");
            continue;
        }

        if(fork()==0){

            // Close the old socket since all communications will be through the new socket
            close(sockfd);

            // Get client information
            struct sockaddr_in peer_addr;
            socklen_t peer_len = sizeof(peer_addr);
            getpeername(newsockfd, (struct sockaddr*)&peer_addr, &peer_len);

            // Set up signal handler for SIGPIPE
            signal(SIGPIPE, SIG_IGN);

            // Get current time
            time_t now = time(NULL);
            char time_str[26];
            ctime_r(&now, time_str);
            time_str[24] = '\0'; // Remove newline

            // Print connection info
            printf("--> [%s] New client connected from %s:%d\n", 
                time_str,
                inet_ntoa(peer_addr.sin_addr), 
                ntohs(peer_addr.sin_port));

            
            while (1) {
                // Receive the key first
                char key[27];
                memset(key, 0, sizeof(key));
                // Add error checking for send/recv operations
                if (recv(newsockfd, key, 26, 0) <= 0) {
                    handle_client_disconnect(newsockfd, &peer_addr);
                    break;
                }

                // Print received filename info
                printf("+++ [%s] Received Key for encryption of file: %s\n", time_str, key);
                
                // Create temporary filename using client's IP and port
                char temp_filename[100];
                snprintf(temp_filename, sizeof(temp_filename), "%s.%d.txt", 
                        inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                
                // Open temporary file for writing
                FILE *temp_file = fopen(temp_filename, "w");

                // Track total bytes received
                long total_bytes = 0;
                int chunk_count = 0;
                
                // Receive file content in chunks
                int bytes_received;
                while ((bytes_received = recv(newsockfd, buffer, CHUNK_SIZE - 1, 0)) > 0) {
                    buffer[bytes_received] = '\0';
                    chunk_count++;
                    total_bytes += bytes_received;
                    
                    printf("+++ Received chunk %d: %d bytes\n", chunk_count, bytes_received);
                    printf("+++ Chunk %d content:\n %s\n", chunk_count, buffer);
                    if (bytes_received < CHUNK_SIZE - 1) {  // Last chunk
                        fputs(buffer, temp_file);
                        break;
                    }
                    fputs(buffer, temp_file);
                }
                
                printf("--> File transfer complete. Total: %ld bytes in %d chunks\n", 
                    total_bytes, chunk_count);
                
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
                    printf("==> [%s] Client disconnected from %s:%d\n",
                        time_str,
                        inet_ntoa(peer_addr.sin_addr),
                        ntohs(peer_addr.sin_port));
                    break;
                }
            }
            
            close(newsockfd);
            exit(0);
        }
        else{
            active_children++;
            close(newsockfd);
        }
        
    }
    

    return 0;
}