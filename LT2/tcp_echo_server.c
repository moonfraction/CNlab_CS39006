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

#define PORT 9090

#define BUF_SIZE 8192


// to build executable: make
// to run server: ./s


void set_socket_options(int sockfd){
    int opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("setsockopt\n");
        exit(1);
    }
}


int create_server_socket(int port){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
        perror("server socket creation failed\n");
        return -1;
    }

    set_socket_options(sockfd);

    struct sockaddr_in srv_addr;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = port;
    srv_addr.sin_family = AF_INET;

    if(bind(sockfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0){
        perror("bind failed\n");
        return -1;
    }
    return sockfd;
}

void handle_client(int client_fd){
    struct sockaddr_in cliaddr;
    socklen_t addr_len = sizeof(cliaddr);
    if(getpeername(client_fd, (struct sockaddr*)&cliaddr, &addr_len)<0){
        perror("getpeername failed\n");
    }

    printf("Client %s:%d connected\n",
                inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));


    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, O_NONBLOCK | flags);

    char buf[BUF_SIZE];
    while(1){
        int br = recv(client_fd, buf, sizeof(buf), 0); // bytes recv()ed
        if(br == -1){
            if(errno == EWOULDBLOCK || errno == EAGAIN){
                continue;
            }
            else{
                printf("Client from %s:%d has disconnected abruptly\n",
                    inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                exit(1);
            }
        }

        if(send(client_fd, buf, sizeof(buf), 0) < 0){
            perror("send echo\n");
        }
        memset(buf, 0, BUF_SIZE);

    }

    printf("Client from %s:%d has disconnected normally\n",
                inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
    exit(0);
}


void sighandler(int sig){
    printf("Client has disconnected normally\n");
    signal(SIGCHLD, sighandler);
}

int main(){
    int port = PORT;
    int srv_fd = create_server_socket(port);
    if(srv_fd==-1) return 1;

    printf("server is running on port %d...\n", port);

    signal(SIGCHLD, sighandler);

    // int flags = fcntl(srv_fd, F_GETFL, 0);
    // fcntl(srv_fd, F_SETFL, O_NONBLOCK | flags);

    listen(srv_fd, 5);

    struct sockaddr_in cli_addr;
    socklen_t addr_len = sizeof(cli_addr);

    while(1){
        int cli_fd = accept(srv_fd, (struct sockaddr*)&cli_addr, &addr_len);
        printf("new client conn\n");
        if(!fork()){
            handle_client(cli_fd);
        }
    }

    close(srv_fd);

    return 0;
}