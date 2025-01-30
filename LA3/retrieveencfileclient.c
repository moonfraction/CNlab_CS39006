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
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 100

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char filename[100];
    char key[27];
    char continue_encryption[10];
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    while (1) {
        // Clear buffers
        memset(filename, 0, sizeof(filename));
        memset(key, 0, sizeof(key));
        memset(buffer, 0, sizeof(buffer));
        
        // Get filename from user
        while (1) {
            printf("Enter the filename to encrypt: ");
            if (scanf("%99s", filename) != 1) {
                printf("Error reading filename\n");
                while (getchar() != '\n'); // Clear input buffer
                continue;
            }
            
            // Check if file exists
            FILE *file = fopen(filename, "r");
            if (!file) {
                printf("NOTFOUND %s\n", filename);
                continue;
            }
            fclose(file);
            break;
        }
        
        // Clear input buffer
        while (getchar() != '\n');
        
        // Get encryption key from user
        while (1) {
            printf("Enter the 26-character encryption key: ");
            if (fgets(key, sizeof(key), stdin) == NULL) {
                printf("Error reading key\n");
                continue;
            }
            
            // Remove newline if present
            key[strcspn(key, "\n")] = 0;
            
            if (strlen(key) != 26) {
                printf("Error: Key must be exactly 26 characters (got %lu characters)\n", strlen(key));
                continue;
            }
            break;
        }
        
        // Send key to server
        if (send(sock, key, 26, 0) != 26) {
            printf("Error sending key to server\n");
            break;
        }
        
        // Send file content to server
        FILE *input_file = fopen(filename, "r");
        if (!input_file) {
            printf("Error opening input file\n");
            break;
        }
        
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE - 1, input_file)) > 0) {
            if (send(sock, buffer, bytes_read, 0) != bytes_read) {
                printf("Error sending file content\n");
                fclose(input_file);
                goto cleanup;
            }
            memset(buffer, 0, BUFFER_SIZE);
        }
        fclose(input_file);
        
        // Small delay to ensure server processes the file
        usleep(100000);  // 100ms delay
        
        // Create encrypted filename
        char enc_filename[120];
        snprintf(enc_filename, sizeof(enc_filename), "%s.enc", filename);
        
        // Receive encrypted file from server
        FILE *output_file = fopen(enc_filename, "w");
        if (!output_file) {
            printf("Error creating output file\n");
            break;
        }
        
        memset(buffer, 0, BUFFER_SIZE);
        while ((bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            if (fwrite(buffer, 1, bytes_read, output_file) != bytes_read) {
                printf("Error writing to output file\n");
                fclose(output_file);
                goto cleanup;
            }
            memset(buffer, 0, BUFFER_SIZE);
            
            // If we received less than buffer size, this might be the end
            if (bytes_read < BUFFER_SIZE - 1) {
                break;
            }
        }
        fclose(output_file);
        
        printf("File encrypted successfully!\n");
        printf("Original file: %s\n", filename);
        printf("Encrypted file: %s\n", enc_filename);
        
        // Ask user if they want to encrypt another file
        while (1) {
            printf("Do you want to encrypt another file? (Yes/No): ");
            if (scanf("%9s", continue_encryption) != 1) {
                printf("Error reading input\n");
                while (getchar() != '\n'); // Clear input buffer
                continue;
            }
            while (getchar() != '\n'); // Clear input buffer
            
            if (strcasecmp(continue_encryption, "Yes") == 0 || strcasecmp(continue_encryption, "No") == 0) {
                break;
            }
            printf("Please enter either 'Yes' or 'No'\n");
        }
        
        // Send response to server
        if (send(sock, continue_encryption, strlen(continue_encryption), 0) <= 0) {
            printf("Error sending continue response\n");
            break;
        }
        
        if (strcasecmp(continue_encryption, "No") == 0) {
            break;
        }
    }
    
cleanup:
    close(sock);
    return 0;
}