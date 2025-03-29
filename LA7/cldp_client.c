/* cldp_client.c
* A CLDP client that:
* - Sends a QUERY message for metadata.
* - Waits for a RESPONSE with a matching transaction ID.
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

#define PROTOCOL_NUM 253
#define BUF_SIZE 1024

// CLDP message types
#define MSG_HELLO    0x01
#define MSG_QUERY    0x02
#define MSG_RESPONSE 0x03

// Metadata flags (bitmask)
#define META_HOSTNAME 0x01
#define META_TIME     0x02
#define META_CPULOAD  0x04

#pragma pack(push, 1)
typedef struct {
    uint8_t  msg_type;
    uint8_t  payload_len;
    uint16_t trans_id;
    uint32_t reserved;
} cldp_header_t;
#pragma pack(pop)

unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (int i = 0; i < nwords; i++) {
        sum += buf[i];
    }
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)(~sum);
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

    // Destination: use broadcast or a specific server IP.
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_aton("127.0.0.1", &dest.sin_addr);

    srand(time(NULL));

    while (1) {
        // Generate a unique transaction id for this query.
        uint16_t trans_id = rand() % 65535;
        // Query bitmask: request hostname, system time, and CPU load.
        uint8_t query_flags = META_HOSTNAME | META_TIME | META_CPULOAD;

        // Send QUERY message (payload is one byte: query_flags)
        send_packet(sockfd, &dest, MSG_QUERY, trans_id, (char *)&query_flags, 1);
        printf("Sent QUERY (trans_id %d) with flags 0x%02x\n", trans_id, query_flags);

        // Wait for a RESPONSE matching our transaction ID.
        int found = 0;
        while (!found) {
            char buffer[BUF_SIZE];
            struct sockaddr_in src_addr;
            socklen_t addrlen = sizeof(src_addr);
            int data_len = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&src_addr, &addrlen);
            if (data_len > 0) {
                struct iphdr *ip_hdr = (struct iphdr *)buffer;
                if (ip_hdr->protocol != PROTOCOL_NUM)
                    continue;
                cldp_header_t *cldp_hdr = (cldp_header_t *)(buffer + sizeof(struct iphdr));
                if (cldp_hdr->msg_type == MSG_RESPONSE) {
                    if (ntohs(cldp_hdr->trans_id) == trans_id) {
                        char response[512];
                        int payload_len = cldp_hdr->payload_len;
                        if (payload_len > 0 && payload_len < sizeof(response)) {
                            memcpy(response, buffer + sizeof(struct iphdr) + sizeof(cldp_header_t), payload_len);
                            response[payload_len] = '\0';
                            printf("Received RESPONSE:\n%s", response);
                        }
                        found = 1;
                    }
                }
            }
        }

        // Ask user to continue or exit
        printf("\nPress Enter to send another query or type 'exit' to quit: ");
        char input[10];
        if (fgets(input, sizeof(input), stdin) != NULL) {
            if (strncmp(input, "exit", 4) == 0)
                break;
        }
    }

    close(sockfd);
    return 0;
}
