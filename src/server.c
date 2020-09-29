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

#define BACKLOG 10

void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct thread_data {
    int id;
    int fd;
};

void* hello_server(void* fd) {
    struct thread_data data = *((struct thread_data*)fd);
    int n = data.id;
    int new_fd = data.fd;
    int rv = 0;
    char buffer[200000];
    /*sprintf(buffer, "Hello, world from thread %d!\n", n);*/
    Request request;
    uint32_t num_bytes = recv(new_fd, buffer, 1000, 0);
    bool res = parse_request(buffer, &request);
    printf("METHOD: %u, URI: %s, VERSION: %u.%u\n", request.method, request.request_uri, 
                                                    version_major(request.version), version_minor(request.version));

    FILE* hello = fopen(request.request_uri + 1, "rb");
    uint32_t size = 0;
    if (hello) {
        fseek(hello, 0, SEEK_END);
        size = ftell(hello);
        printf("size: %d\n", size);
        rewind(hello);
        fread(buffer, sizeof(char), size, hello);
        fclose(hello);
    }
    char* resp = NULL;
    Response response;
    response.version = request.version;
    response.status = OK;
    response.content_length = size;
    response.data = buffer;
    size = create_response(&response, &resp);
    resp[size - 1] = '\0';
    printf("size: %d\n", size);
    printf("%s\n", resp);

    rv = send(new_fd, resp, size, 0);
    if (rv == -1) {
        perror("send");
    }
    shutdown(new_fd, 2);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client server_addr port /path/to/file\n");
        exit(1);
    }
    struct addrinfo hints = { 
        .ai_flags = AI_PASSIVE,
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

        int yes = 1;
        int r = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (r == -1) {
            perror("setsockopt");
            return 1;
        }

        r = bind(sockfd, p->ai_addr, p->ai_addrlen);
        if (r == -1) {
            shutdown(sockfd, 2);
            perror("server: bind");
            continue;
        }

        break;
    }
    freeaddrinfo(servinfo);

    rv = listen(sockfd, BACKLOG);
    if (rv == -1) {
        perror("listen");
        return 1;
    }

    printf("server: waiting for connections...\n");

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    int count = 0;
    while (1) {
        sin_size = sizeof(their_addr);
        int new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        count++;
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr*)&their_addr),
                  s, sizeof(s));
        printf("server: got connection from %s\n", s);

        pthread_t tid;

        struct thread_data data = {count, new_fd};
        int rc = pthread_create(&tid, NULL, hello_server, (void *)&data);
        
    }

    return 0;
}

