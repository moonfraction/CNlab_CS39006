/* cldp_client.c
* A CLDP client that:
* - Listens for HELLO messages and maintains a list of active servers.
* - Sends a QUERY message to each active server for metadata.
* - Waits for RESPONSE messages with matching transaction IDs.
* - Removes unresponsive servers from its active server list.
* - Prompts the user to continue or exit.
*
* Compile with:
*   gcc -o cldp_client cldp_client.c
* Run with:
*   sudo ./cldp_client
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>   // using iphdr from here
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

#define PROTOCOL_NUM 253
#define BUF_SIZE 1024
#define MAX_SERVERS 64
#define SERVER_TIMEOUT 30  // seconds

// CLDP message types
#define MSG_HELLO    0x01
#define MSG_QUERY    0x02
#define MSG_RESPONSE 0x03

// Metadata flags (bitmask)
#define META_HOSTNAME 0x01
#define META_TIME     0x02
#define META_CPULOAD  0x04

typedef struct {
    uint8_t  msg_type;
    uint8_t  payload_len;
    uint16_t trans_id;
    uint32_t reserved;
} __attribute__((packed)) cldp_header_t;

// Server info structure
typedef struct {
    struct in_addr addr;
    time_t last_seen;
    char active;  // 1 if active, 0 if marked for removal
} server_info_t;

// Global list of active servers
server_info_t server_list[MAX_SERVERS];
int server_count = 0;

unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (int i = 0; i < nwords; i++) {
        sum += buf[i];
    }
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)(~sum);
}

// Function to add a server to our list or update its last_seen time
void update_server_list(struct in_addr addr) {
    time_t now = time(NULL);
    
    // Check if server is already in the list
    for (int i = 0; i < server_count; i++) {
        if (server_list[i].addr.s_addr == addr.s_addr) {
            server_list[i].last_seen = now;
            server_list[i].active = 1;
            return;
        }
    }
    
    // If not in list and we have space, add it
    if (server_count < MAX_SERVERS) {
        server_list[server_count].addr = addr;
        server_list[server_count].last_seen = now;
        server_list[server_count].active = 1;
        printf("Added new server: %s\n", inet_ntoa(addr));
        server_count++;
    }
}

// Function to clean up old servers from the list
void cleanup_server_list() {
    time_t now = time(NULL);
    int i = 0;
    
    while (i < server_count) {
        // Remove servers that are inactive or haven't been seen in SERVER_TIMEOUT seconds
        if (!server_list[i].active || (now - server_list[i].last_seen > SERVER_TIMEOUT)) {
            printf("Removing server: %s (", inet_ntoa(server_list[i].addr));
            if (!server_list[i].active) {
                printf("unresponsive)\n");
            } else {
                printf("timed out)\n");
            }
            
            // Remove by shifting the rest of the array
            if (i < server_count - 1) {
                memmove(&server_list[i], &server_list[i+1], 
                       (server_count - i - 1) * sizeof(server_info_t));
            }
            server_count--;
        } else {
            i++;
        }
    }
}

// Function to build and send a packet
void send_packet(int sockfd, struct sockaddr_in *dest, uint8_t msg_type,
                uint16_t trans_id, const char *payload, uint8_t payload_len) {
    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);

    // Build IP header using struct iphdr
    struct iphdr *ip_hdr = (struct iphdr *)buffer;
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->tos = 0;
    int total_len = sizeof(struct iphdr) + sizeof(cldp_header_t) + payload_len;
    ip_hdr->tot_len = htons(total_len);
    ip_hdr->id = htons(rand() % 65535);
    ip_hdr->frag_off = 0;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = PROTOCOL_NUM;
    ip_hdr->check = 0;
    ip_hdr->saddr = INADDR_ANY;
    ip_hdr->daddr = dest->sin_addr.s_addr;
    ip_hdr->check = checksum((unsigned short *)ip_hdr, ip_hdr->ihl * 2);

    // Build CLDP header following the IP header
    cldp_header_t *cldp_hdr = (cldp_header_t *)(buffer + sizeof(struct iphdr));
    cldp_hdr->msg_type = msg_type;
    cldp_hdr->payload_len = payload_len;
    cldp_hdr->trans_id = htons(trans_id);
    cldp_hdr->reserved = 0;

    // Copy payload if provided
    if (payload_len > 0 && payload != NULL) {
        memcpy(buffer + sizeof(struct iphdr) + sizeof(cldp_header_t), payload, payload_len);
    }

    if (sendto(sockfd, buffer, total_len, 0, (struct sockaddr *)dest, sizeof(struct sockaddr_in)) < 0) {
        perror("sendto failed");
    }
}

// Function to listen for HELLO messages for a specified duration
void listen_for_hello(int sockfd, int duration_sec) {
    printf("\nListening for HELLO messages (%d seconds)...\n", duration_sec);
    
    fd_set read_fds;
    struct timeval tv;
    time_t start_time = time(NULL);

    while (time(NULL) - start_time < duration_sec) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        tv.tv_sec = 1;  // Check every second
        tv.tv_usec = 0;

        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &tv);

        if (activity < 0 && errno != EINTR) {
            perror("select error");
            continue;
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            char buffer[BUF_SIZE];
            struct sockaddr_in src_addr;
            socklen_t addrlen = sizeof(src_addr);
            
            int data_len = recvfrom(sockfd, buffer, BUF_SIZE, 0, 
                                    (struct sockaddr *)&src_addr, &addrlen);
                                    
            if (data_len > 0) {
                struct iphdr *ip_hdr = (struct iphdr *)buffer;
                if (ip_hdr->protocol != PROTOCOL_NUM)
                    continue;
                    
                cldp_header_t *cldp_hdr = (cldp_header_t *)(buffer + sizeof(struct iphdr));
                
                // If it's a HELLO message, add the server to our list
                if (cldp_hdr->msg_type == MSG_HELLO) {
                    struct in_addr src_ip;
                    src_ip.s_addr = ip_hdr->saddr;
                    printf("Received HELLO from %s\n", inet_ntoa(src_ip));
                    update_server_list(src_ip);
                }
            }
        }
    }
}

// Function to query all servers and wait for responses
void query_all_servers(int sockfd) {
    if (server_count == 0) {
        printf("No active servers to query.\n");
        return;
    }

    printf("Querying %d active servers...\n", server_count);
    
    // Mark all servers as inactive initially
    for (int i = 0; i < server_count; i++) {
        server_list[i].active = 0;
    }
    
    // Generate a unique transaction id for this batch of queries
    uint16_t trans_id = rand() % 65535;
    
    // Query bitmask: request hostname, system time, and CPU load
    uint8_t query_flags = META_HOSTNAME | META_TIME | META_CPULOAD;
    
    // Send a query to each server
    for (int i = 0; i < server_count; i++) {
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr = server_list[i].addr;
        
        send_packet(sockfd, &dest, MSG_QUERY, trans_id, (char *)&query_flags, 1);
        printf("Sent QUERY (trans_id %d) to %s\n", trans_id, inet_ntoa(server_list[i].addr));
    }
    
    // Wait for responses with a timeout
    fd_set read_fds;
    struct timeval tv;
    time_t start_time = time(NULL);
    int responses_received = 0;
    const int response_timeout = 5;  // 5 second timeout for responses
    
    while (time(NULL) - start_time < response_timeout && responses_received < server_count) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            perror("select error");
            continue;
        }
        
        if (FD_ISSET(sockfd, &read_fds)) {
            char buffer[BUF_SIZE];
            struct sockaddr_in src_addr;
            socklen_t addrlen = sizeof(src_addr);
            
            int data_len = recvfrom(sockfd, buffer, BUF_SIZE, 0, 
                                    (struct sockaddr *)&src_addr, &addrlen);
                                    
            if (data_len > 0) {
                struct iphdr *ip_hdr = (struct iphdr *)buffer;
                if (ip_hdr->protocol != PROTOCOL_NUM)
                    continue;
                    
                cldp_header_t *cldp_hdr = (cldp_header_t *)(buffer + sizeof(struct iphdr));
                
                // Check if it's a RESPONSE matching our transaction ID
                if (cldp_hdr->msg_type == MSG_RESPONSE && ntohs(cldp_hdr->trans_id) == trans_id) {
                    // Find which server this response is from
                    for (int i = 0; i < server_count; i++) {
                        if (server_list[i].addr.s_addr == ip_hdr->saddr) {
                            server_list[i].active = 1;  // Mark as active
                            server_list[i].last_seen = time(NULL);
                            responses_received++;
                            
                            char response[512];
                            int payload_len = cldp_hdr->payload_len;
                            if (payload_len > 0 && payload_len < sizeof(response)) {
                                memcpy(response, buffer + sizeof(struct iphdr) + 
                                      sizeof(cldp_header_t), payload_len);
                                response[payload_len] = '\0';
                                printf("\nReceived RESPONSE from %s:\n%s", 
                                      inet_ntoa(server_list[i].addr), response);
                            }
                            break;
                        }
                    }
                }
                // Also update server list if we get any HELLO messages during this time
                else if (cldp_hdr->msg_type == MSG_HELLO) {
                    struct in_addr src_ip;
                    src_ip.s_addr = ip_hdr->saddr;
                    update_server_list(src_ip);
                }
            }
        }
    }
    
    // Cleanup unresponsive servers
    cleanup_server_list();
    
    printf("\nQuery complete. %d servers responded out of %d total.\n", 
           responses_received, server_count + (server_count - responses_received));
}

int main() {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_RAW, PROTOCOL_NUM)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt IP_HDRINCL error");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    while (1) {
        listen_for_hello(sockfd, 10);
        
        // Then query all active servers
        query_all_servers(sockfd);
        
        // Ask user to continue or exit
        printf("\nPress Enter to repeat the process or type 'exit' to quit: ");
        char input[10];
        if (fgets(input, sizeof(input), stdin) != NULL) {
            if (strncmp(input, "exit", 4) == 0)
                break;
        }
    }

    close(sockfd);
    return 0;
}