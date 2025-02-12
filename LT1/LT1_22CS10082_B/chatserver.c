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
#include<limits.h>

// clients must enter a number greater than -1000;

#define MAX_CL 5
#define BUFFER_SZ 1024 // maximum allowed buffer size
#define PORT 2000

int main(){
    char buf[BUFFER_SZ];
    int server_fd, numclient = 0;
    struct sockaddr_in server_addr, client_addr;
    int clientsockfd[MAX_CL];
    socklen_t clilen;
    clilen = sizeof(client_addr);
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error creating server socket");
    }

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_family = AF_INET;

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        perror("Server Bind Error");
    }
    int maxfd;
    fd_set readerfds, curfds;
    FD_ZERO(&readerfds);
    FD_ZERO(&curfds);

    listen(server_fd, MAX_CL);

    FD_SET(server_fd, &readerfds);
    maxfd = server_fd;
    
    int round = 1;

    int max_received[MAX_CL];
    for(int i=0; i<MAX_CL;i++) max_received[i] = INT_MIN, clientsockfd[i] = -1;
    while(1){
        curfds = readerfds;
        select(maxfd+1, &curfds, NULL, NULL, NULL);

        if(FD_ISSET(server_fd, &curfds)){
            // connection request
            if(numclient == MAX_CL) continue; // dont accept anymore
            int newfd = accept(server_fd, (struct sockaddr*)&client_addr, &clilen);
            if(newfd < 0){
                perror("Could not accept client");
                continue;
            }
            if(newfd > 0 && newfd > maxfd) maxfd = newfd;
            numclient++;
            clientsockfd[numclient-1] = newfd;
            FD_SET(newfd, &readerfds);
            printf("Server : Received a new connection from <%s,%d>\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
            char temp[1024];
            sprintf(temp, "Server:Send your number for round %d\n", round);
            send(newfd, temp, strlen(temp)+1, 0);
        }

        for(int i=0; i<numclient; i++){
            int sfd = clientsockfd[i];
            if(FD_ISSET(sfd, &curfds)){
                memset(buf,0,sizeof(buf));
                recv(sfd, buf, BUFFER_SZ, 0);
                struct sockaddr_in temp_client;
                socklen_t tcl = sizeof(temp_client);
                getpeername(sfd, (struct sockaddr*)&temp_client, &tcl);
                // check for less clients
                if(numclient < 2){
                    char temp[2048];
                    sprintf(temp,"Server : Insufficient clients, %s from client <%s,%d> dropped\n",buf, inet_ntoa(temp_client.sin_addr), temp_client.sin_port);
                    send(sfd, temp, strlen(temp)+1, 0);
                }else if(max_received[i] != INT_MIN){
                    char temp[1024];
                    int nn;
                    sscanf(buf, "%d",&nn);
                    sprintf(temp,"Server : Duplicae message for round %d not allowed. Please wait for the results for round %d and call for the round %d\n", round, round, round+1);
                    send(sfd, temp, strlen(temp)+1, 0);
                }else{
                    int nn;
                    sscanf(buf, "%d",&nn);
                    max_received[i] = nn;
                }
            }
        }

        // check if all have sent
        int ok = 1;
        for(int i=0; i<numclient; i++){
            if(max_received[i] == INT_MIN) ok = 0;
        }

        if(ok){
            // round complete
            int ans = INT_MIN;
            int id = -1;
            for(int i=0; i<numclient; i++){
                if(max_received[i] > ans){
                    ans = max_received[i];
                    id = i;
                }
            }
            int sfd = clientsockfd[id];
            struct sockaddr_in temp_client;
            socklen_t tcl = sizeof(temp_client);
            getpeername(sfd, (struct sockaddr*)&temp_client, &tcl);
            printf("Maximum number received in round %d is : %d. The number has been received from client <%s,%d>\n", round, ans, inet_ntoa(temp_client.sin_addr), temp_client.sin_port);

            // go to next round
            round++;
            for(int i=0; i<MAX_CL; i++) max_received[i] = INT_MIN;
            for(int i=0; i<MAX_CL; i++){
                int fd = clientsockfd[i];
                char temp[1024];
                sprintf(temp,"Server : Enter number for round %d\n", round);
                send(fd, temp, strlen(temp)+1, 0);
            }
        }
    }

    return 0;
}