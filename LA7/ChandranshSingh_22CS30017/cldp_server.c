/*=====================================
Assignment 7 Submission
Name: Chandransh Singh
Roll number: 22CS30017
=====================================*/

/* cldp_server.c
* A CLDP server that:
* - Announces HELLO every 10 seconds.
* - Listens for incoming QUERY messages and responds with the requested metadata.
*
* Compile with:
*   gcc -o s cldp_server.c
* Run with:
*   sudo ./s
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/sysinfo.h>

#define PROTOCOL_NUM 253
#define BUF_SIZE 1024

// CLDP message types
#define MSG_HELLO    0x01
#define MSG_QUERY    0x02
#define MSG_RESPONSE 0x03

// Metadata flags
#define META_HOSTNAME 0x01
#define META_TIME     0x02
#define META_CPULOAD  0x04

// CLDP custom header (8 bytes)
typedef struct {
    uint8_t  msg_type;      // 1 byte
    uint8_t  payload_len;   // 1 bytes (payload length in bytes)
    uint16_t trans_id;      // 2 bytes (transaction ID)
    uint32_t reserved;      // 4 bytes (reserved for future use)
} __attribute__((packed)) cldp_header_t; // packed to avoid padding

// IP header checksum
unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (int i = 0; i < nwords; i++) {
        sum += buf[i];
    }
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)(~sum);
}

// Get metadata functions
void get_hostname(char *buffer, size_t size) {
    if (gethostname(buffer, size) != 0) {
        strncpy(buffer, "unknown", size);
    }
}

void get_system_time(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

float get_cpu_load() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return info.loads[0] / 65536.0;
    }
    return -1.0;
}

// Function to build and send a packet
void send_packet(int sockfd, struct sockaddr_in *dest, uint8_t msg_type,
                uint16_t trans_id, const char *payload, uint8_t payload_len) {
    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);

    // IP header
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

    // CLDP header
    cldp_header_t *cldp_hdr = (cldp_header_t *)(buffer + sizeof(struct iphdr));
    cldp_hdr->msg_type = msg_type;
    cldp_hdr->payload_len = payload_len;
    cldp_hdr->trans_id = htons(trans_id);
    cldp_hdr->reserved = 0;

    // payload for RESPONSE
    if (payload_len > 0 && payload != NULL) {
        memcpy(buffer + sizeof(struct iphdr) + sizeof(cldp_header_t), payload, payload_len);
    }

    if (sendto(sockfd, buffer, total_len, 0, (struct sockaddr *)dest, sizeof(struct sockaddr_in)) < 0) {
        perror("sendto failed");
    }
}

void process_query(int sockfd, struct sockaddr_in *src_addr, const char *buffer, cldp_header_t *cldp_hdr) {
    uint16_t trans_id = ntohs(cldp_hdr->trans_id);
    
    // The query payload is a bitmask of requested metadata
    uint8_t query_flags = 0;
    if (cldp_hdr->payload_len >= 1)
        query_flags = *(buffer + sizeof(struct iphdr) + sizeof(cldp_header_t));

    char response_payload[256];
    int offset = 0;
    if (query_flags & META_HOSTNAME) {
        char hostname[64];
        get_hostname(hostname, sizeof(hostname));
        offset += snprintf(response_payload + offset, sizeof(response_payload) - offset,
                        "Hostname: %s\n", hostname);
    }
    if (query_flags & META_TIME) {
        char timestr[64];
        get_system_time(timestr, sizeof(timestr));
        offset += snprintf(response_payload + offset, sizeof(response_payload) - offset,
                        "Time: %s\n", timestr);
    }
    if (query_flags & META_CPULOAD) {
        float load = get_cpu_load();
        offset += snprintf(response_payload + offset, sizeof(response_payload) - offset,
                        "CPU Load: %.2f\n", load);
    }

    // Send the RESPONSE packet back to the sender
    send_packet(sockfd, src_addr, MSG_RESPONSE, trans_id, response_payload, offset);
    printf("<-- Sent RESPONSE to %s (trans_id %d)\n", inet_ntoa(src_addr->sin_addr), trans_id);
}

int main() {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_RAW, PROTOCOL_NUM)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set IP_HDRINCL so we provide the complete IP header.
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt IP_HDRINCL error");
        exit(EXIT_FAILURE);
    }

    // Bind to any interface
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("+++ CLDP Server running...\n");

    // Multiplex between receiving packets and sending periodic HELLO messages.
    fd_set read_fds;
    struct timeval tv;
    time_t last_hello = time(NULL);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &tv);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
        }

        // Process incoming data if any.
        if (FD_ISSET(sockfd, &read_fds)) {
            char buffer[BUF_SIZE];
            struct sockaddr_in src_addr;
            socklen_t addrlen = sizeof(src_addr);
            int data_len = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&src_addr, &addrlen);
            // Check if the packet is a CLDP packet.
            if (data_len > 0) {
                struct iphdr *ip_hdr = (struct iphdr *)buffer;
                if (ip_hdr->protocol != PROTOCOL_NUM)
                    continue;
                cldp_header_t *cldp_hdr = (cldp_header_t *)(buffer + sizeof(struct iphdr));

                // Process QUERY messages: reply with a RESPONSE.
                if (cldp_hdr->msg_type == MSG_QUERY) {
                    process_query(sockfd, &src_addr, buffer, cldp_hdr);
                }
            }
            else {
                perror("recvfrom failed");
                continue;
            }
        }

        // Every 10 seconds, send a HELLO message (for node announcement).
        if (time(NULL) - last_hello >= 10) {
            struct sockaddr_in broadcast_addr;
            memset(&broadcast_addr, 0, sizeof(broadcast_addr));
            broadcast_addr.sin_family = AF_INET;
            broadcast_addr.sin_port = 0;
            
            inet_aton("127.0.0.1", &broadcast_addr.sin_addr); // change broadcast address as needed

            send_packet(sockfd, &broadcast_addr, MSG_HELLO, 0, NULL, 0);
            printf("<== Broadcast HELLO sent.\n");
            last_hello = time(NULL);
        }
    }

    close(sockfd);
    return 0;
}
