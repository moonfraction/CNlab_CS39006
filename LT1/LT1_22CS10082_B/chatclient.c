/*
Name : Gaurav Roy
Roll : 22CS10082
SET B
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

#define BUFFER_SZ 2048 // maximum allowed buffer size
#define PORT 2000

int main(){
    char buf[BUFFER_SZ];
    int cfd;
    struct sockaddr_in server_addr;
    socklen_t slen;
    slen = sizeof(server_addr);
    if((cfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error creating client socket");
    }

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_family = AF_INET;

    if(connect(cfd, (struct sockaddr*)&server_addr, slen) < 0){
        perror("Unable to connect to server");
    }

    fd_set readerfds, curfds;
    FD_ZERO(&readerfds);
    FD_ZERO(&curfds);
    FD_SET(STDIN_FILENO, &readerfds);
    FD_SET(cfd, &readerfds);
    int maxfd = cfd;

    while(1){
        curfds = readerfds;
        select(maxfd +1, &curfds, NULL, NULL, NULL);

        if(FD_ISSET(cfd, &curfds)){
            memset(buf, 0, sizeof(buf));
            recv(cfd, buf, BUFFER_SZ, 0);

            printf("Client : Received the following message text from the server : \n");
            printf("%s",buf);
        }

        if(FD_ISSET(STDIN_FILENO, &curfds)){
            // input received
            memset(buf, 0, sizeof(buf));
            int val;
            scanf("%d", &val);
            char temp[200];
            sprintf(temp, "%d", val);
            struct sockaddr_in temp_client;
            socklen_t tcl = sizeof(temp_client);
            getpeername(cfd, (struct sockaddr*)&temp_client, &tcl);

            printf("Client <%s> number %d sent to server\n", inet_ntoa(temp_client.sin_addr), val);
            send(cfd, temp, strlen(temp)+1, 0);
        }
    }

    return 0;
}