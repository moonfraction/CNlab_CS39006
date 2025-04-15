#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#include <sys/signal.h>

// ctrl + C to normally close the client

// to build executable: make
// to run server: ./c

#define PORT 9090
#define BUF_SIZE 8192

int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        perror("server socket creation failed\n");
        return -1;
    }

    struct sockaddr_in srv_addr;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = PORT;
    srv_addr.sin_family = AF_INET;

    if(connect(sockfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0){
        perror("conn error\n");
        return 1;
    }

    printf("Client conn to server\n");
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    while(1){
        printf("input: ");
        fgets(buf, sizeof(buf), stdin);
        // scanf("%s", buf);
        // printf("buf = %s\n", buf);
        if(send(sockfd, buf, sizeof(buf), 0) < 0){
            perror("send failed\n");
        }
        // printf("after send buf = %s\n", buf);


        memset(buf, 0, BUF_SIZE); // clear the buf after sending

        int br = recv(sockfd, buf, sizeof(buf), 0);
        if(br <=0){
            printf("server disconnected \n");
            return 1;
        }       
        printf("--> msg echo from server = %s\n", buf);
    }

    return 0;
}