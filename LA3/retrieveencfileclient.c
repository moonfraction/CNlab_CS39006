/*=====================================
Assignment 3 Submission
Name: Chandransh Singh
Roll number: 22CS30017
Link of the pcap file: 
https://drive.google.com/file/d/1a9u-WHTwXj3sgDpkU9xcX34nqLSDF1Z6/view?usp=sharing
=====================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_CHUNK 70 // different from the server, and it does not know
#define SERVER_PORT 8888
#define MAX_FILENAME 256
#define BUFFER_SIZE 100
struct transfer_stats {
    int chunk_count;
    size_t total_bytes;
};

void print_transfer_info(const char* direction, int chunk_num, size_t bytes) {
    printf("[Client] %s chunk #%d: %zu bytes\n", direction, chunk_num, bytes);
}

void print_transfer_summary(struct transfer_stats stats, const char* direction) {
    printf("--> Total chunks %s: %d, total bytes: %zu\n\n", 
           direction, stats.chunk_count, stats.total_bytes);
}

// Clear input buffer
void flush_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};
    char filename[MAX_FILENAME];
    char enc_key[27];
    char continue_choice[10];
    
    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    // Convert IP address from string to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(1);
    }
    
    // Connect to server
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }
    
    while (1) {
        FILE *input_file = NULL;
        
        // Get valid filename from user
        while (1) {
            printf("\nEnter filename to encrypt: ");
            scanf("%s", filename);
            input_file = fopen(filename, "r");
            if (input_file) {
            break;
            }
            printf("NOTFOUND %s\n", filename);
        }
        
        // Get valid encryption key
        while (1) {
            printf("Enter 26-character encryption key: ");
            scanf("%s", enc_key);
            if (strlen(enc_key) == 26) {
            break;
            }
            printf("Error: Key must be exactly 26 characters\n");
        }
        
        // Send encryption key
        send(sock_fd, enc_key, 26, 0);
        
        // Send file contents
        struct transfer_stats send_stats = {0, 0};
        size_t bytes_read;
        
        while ((bytes_read = fread(buffer, 1, MAX_CHUNK - 1, input_file)) > 0) {
            send(sock_fd, buffer, bytes_read, 0);
            send_stats.chunk_count++;
            send_stats.total_bytes += bytes_read;
            print_transfer_info("sent", send_stats.chunk_count, bytes_read);
            usleep(1000);  // Small delay to prevent flooding
        }
        
        send(sock_fd, "END_OF_FILE", 11, 0);
        print_transfer_summary(send_stats, "sent");
        fclose(input_file);
        
        // Create encrypted filename
        char enc_filename[MAX_FILENAME + 5];
        snprintf(enc_filename, sizeof(enc_filename), "%s.enc", filename);
        
        // Receive and save encrypted file
        FILE *enc_file = fopen(enc_filename, "w");
        if (!enc_file) {
            perror("Cannot create encrypted file");
            break;
        }
        
        // Receive encrypted content
        struct transfer_stats recv_stats = {0, 0};
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            ssize_t bytes = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) break;
            
            if (strncmp(buffer, "END_OF_FILE", 11) == 0) break;
            
            recv_stats.chunk_count++;
            recv_stats.total_bytes += bytes;
            print_transfer_info("received", recv_stats.chunk_count, bytes);
            fprintf(enc_file, "%s", buffer);
        }
        print_transfer_summary(recv_stats, "received");
        fclose(enc_file);
        
        printf("File encrypted successfully!\n");
        printf("Original file: %s\n", filename);
        printf("Encrypted file: %s\n", enc_filename);
    
        printf("------------------------------------------------------------\n");

        // Ask about continuing
        flush_stdin();
        printf("\nEncrypt another file? (Yes/No): ");
        fgets(continue_choice, sizeof(continue_choice), stdin);
        continue_choice[strcspn(continue_choice, "\n")] = 0;
        
        // Send response to server
        send(sock_fd, continue_choice, strlen(continue_choice), 0);
        
        if (strcasecmp(continue_choice, "No") == 0) {
            break;
        }
    }
    
    close(sock_fd);
    return 0;
}