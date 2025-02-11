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
#define MAX_CLIENT 5

int clientsockfd[MAX_CLIENT + 1];
int numclient = 0;

void print_newconn(struct sockaddr_in clientaddr){
    int cp = clientaddr.sin_port;
    char *ip = inet_ntoa(clientaddr.sin_addr);
    printf("Server: Received a new connection from client %s: %d\n", ip, cp);
}

void print_insuff(int i, char *msg){
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    if(getpeername(clientsockfd[i], (struct sockaddr*)&clientaddr, &addrlen) < 0){
        perror("getpeername error");
        exit(1);
    }

    int cp = clientaddr.sin_port;
    char *ip = inet_ntoa(clientaddr.sin_addr);

    printf("Server: Insufficient clients, %s from client %s: %d\n", msg, ip, cp);

}

void sendtoall(int i, char* msg){

    struct sockaddr_in ogc;
    socklen_t ogal = sizeof(ogc);
    if(getpeername(clientsockfd[i], (struct sockaddr*)&ogc, &ogal) < 0){
        perror("getpeername error");
        exit(1);
    }

    int ogp = ogc.sin_port;
    char *ogip = inet_ntoa(ogc.sin_addr);


    for(int j = 0; j<numclient; ++j){
        if(i==j) continue;

        if(send(clientsockfd[j], msg, sizeof(msg), 0) < 0){
            perror("Send failed");
            exit(0);
        }
            
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);
        if(getpeername(clientsockfd[j], (struct sockaddr*)&clientaddr, &addrlen) < 0){
            perror("getpeername error");
            exit(1);
        }

        int cp = clientaddr.sin_port;
        char *ip = inet_ntoa(clientaddr.sin_addr);

        printf("Server: Send message %s from client %s:%d to %s:%d\n",msg, ogip, ogp, ip, cp);

    }
}

int main(){
    int server_sock;
    if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed\n");
        exit(1);
    }

    struct sockaddr_in serveraddr, clientaddr;
    socklen_t addrlen = sizeof(clientaddr);

    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(PORT);
    serveraddr.sin_family = AF_INET;

    if((bind(server_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) < 0){
        perror("Bind failed");
        exit(0);
    }

    if((listen(server_sock, MAX_CLIENT) < 0)){
        perror("Listen failed");
        exit(0);
    }

    printf("Server is running on %d...\n", PORT);

    fd_set masterfds, readfds;
    int maxfd;
    FD_ZERO(&masterfds);
    maxfd = server_sock;
    FD_SET(server_sock, &masterfds);

    while(1){
        // printf("total cli = %d\n", numclient);
        readfds = masterfds;

        if(select(maxfd+1, &readfds, NULL, NULL, NULL) < 0){
            perror("Select failed");
            exit(1);
        }

        int newclientfd;
        if(FD_ISSET(server_sock, &readfds)){
            if((newclientfd = accept(server_sock, (struct sockaddr*)&clientaddr, &addrlen)) < 0){
                perror("Accept failed");
                exit(1);
            }

            // printf("new conn --> "); // comment
            clientsockfd[numclient] = newclientfd;
            numclient++;
            FD_SET(clientsockfd[numclient-1], &masterfds);

            if(getpeername(newclientfd, (struct sockaddr*)&clientaddr, &addrlen) < 0){
                perror("getpeername error");
                exit(1);
            }
            print_newconn(clientaddr);

            FD_SET(newclientfd, &masterfds);
            if(newclientfd > maxfd) maxfd = newclientfd;

            FD_SET(server_sock, &masterfds);
        }

        for(int i = 0; i<numclient; ++i){
            // printf("i = %d\n", i);
            if(FD_ISSET(clientsockfd[i], &readfds)){
                char msg[100];
                int n;
                printf("Recv()ing msg\n"); // cmmt
                if((n = recv(clientsockfd[i], msg, 100, 0)) < 0){
                    perror("recv failed");
                    exit(1);
                }
                msg[n] = '\0';


                struct sockaddr_in clientaddr;
                socklen_t addrlen = sizeof(clientaddr);
                if(getpeername(clientsockfd[i], (struct sockaddr*)&clientaddr, &addrlen) < 0){
                    perror("getpeername error");
                    exit(1);
                }

                int cp = clientaddr.sin_port;
                char *ip = inet_ntoa(clientaddr.sin_addr);

                printf("Server: Received message %s from %s:%d\n", msg, ip, cp); // cmmnt

                if(numclient < 2){
                    print_insuff(i, msg);
                }

                sendtoall(i, msg);

                FD_SET(clientsockfd[i], &masterfds);
            }
        }




    }




    return 0;
}