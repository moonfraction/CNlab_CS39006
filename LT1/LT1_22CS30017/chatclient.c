/*
22CS30017
Chandransh Singh
SET A
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 8888

int main(){
    int client_sock;
    if((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed\n");
        exit(1);
    }

    struct sockaddr_in serveraddr;

    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(PORT);
    serveraddr.sin_family = AF_INET;

    if(connect(client_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0){
        perror("Connec failed");
        exit(1);
    }


    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    if(getpeername(client_sock, (struct sockaddr*)&clientaddr, &addrlen) < 0){
        perror("getpeername error");
        exit(1);
    }

    int cp = clientaddr.sin_port;
    char *ip = inet_ntoa(clientaddr.sin_addr);
    printf("Conn success on %s:%d\n", ip, cp);

    int maxfds = STDIN_FILENO;
    fd_set masterfds, readfds;
    FD_ZERO(&masterfds);
    maxfds = client_sock;

    FD_SET(client_sock, &masterfds);
    FD_SET(STDIN_FILENO, &masterfds);

    while(1){
        readfds = masterfds;

        if(select(maxfds+1, &readfds, NULL, NULL, NULL) < 0){
            perror("Select failed");
            exit(1);
        }

        if(FD_ISSET(STDIN_FILENO, &readfds)){
            char buf[100];
            fgets(buf, 100, stdin);

            printf("read from terminal: %s\n", buf); // cmmt

            if(send(client_sock, buf, 100, 0) < 0){
                perror("Send failed");
                exit(0);
            }

            printf("Send succ\n"); // cmmt
            FD_SET(STDIN_FILENO, &masterfds);
        }

        if(FD_ISSET(client_sock, &readfds)){
            char msg[100];
            int n;
            printf("Recv()ing msg\n"); // cmmt
            if((n = recv(client_sock, msg, 100, 0)) < 0){
                perror("recv failed");
                exit(1);
            }
            msg[n] = '\0';
            printf("Client: %s\n", msg); // cmmnt

            FD_SET(client_sock, &masterfds);
        }
    }

    return 0;
}