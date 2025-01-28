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

#define CHUNK_SIZE 100

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[CHUNK_SIZE];
    
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket");
        exit(0);
    }
    
    // Initialize server address structure
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(20000);
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to connect to server");
        exit(0);
    }
    
    while (1) {
        char filename[100];
        char key[27];
        FILE *fp;
        
        // Get filename from user
        while (1) {
            printf("Enter the filename to encrypt: ");
            scanf("%s", filename);
            
            fp = fopen(filename, "r");
            if (fp == NULL) {
                printf("NOTFOUND %s\n", filename);
                continue;
            }
            break;
        }
        
        // Get encryption key from user
        while (1) {
            printf("Enter the 26-character encryption key: ");
            scanf("%s", key);
            
            if (strlen(key) != 26) {
                printf("Error: Key must be exactly 26 characters long\n");
                continue;
            }
            break;
        }
        
        // Send key to server
        send(sockfd, key, 26, 0);
        
        // Send file content to server in chunks
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, CHUNK_SIZE - 1, fp)) > 0) {
            send(sockfd, buffer, bytes_read, 0);
        }
        fclose(fp);
        
        // Create encrypted filename
        char enc_filename[120];
        snprintf(enc_filename, sizeof(enc_filename), "%s.enc", filename);
        
        // Receive encrypted file from server
        FILE *enc_file = fopen(enc_filename, "w");
        int bytes_received;
        
        while ((bytes_received = recv(sockfd, buffer, CHUNK_SIZE - 1, 0)) > 0) {
            buffer[bytes_received] = '\0';
            fputs(buffer, enc_file);
            if (bytes_received < CHUNK_SIZE - 1) {  // Last chunk
                break;
            }
        }
        fclose(enc_file);
        
        printf("File encrypted successfully!\n");
        printf("Original file: %s\n", filename);
        printf("Encrypted file: %s\n", enc_filename);
        
        // Ask user if they want to encrypt another file
        char continue_choice[10];
        printf("Do you want to encrypt another file? (Yes/No): ");
        scanf("%s", continue_choice);
        
        // Send choice to server
        send(sockfd, continue_choice, strlen(continue_choice) + 1, 0);
        
        if (strcmp(continue_choice, "No") == 0) {
            break;
        }
    }
    
    close(sockfd);
    return 0;
}