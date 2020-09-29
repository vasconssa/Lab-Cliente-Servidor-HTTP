#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "http.h"

void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef struct thread_data {
    int id;
    int fd;
} thread_data;


int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client server_addr port /path/to/file");
        exit(1);
    }

    struct addrinfo hints = { 
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM, 0};

    struct addrinfo *servinfo;
    int rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo);
    if (rv != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    struct addrinfo* p;
    int sockfd;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("server: socket");
            continue;
        }

        rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
        if (rv == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof(s));
    
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo);


    return 0;
}
