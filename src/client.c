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
#include "url.h"

void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef struct thread_data {
    int id;
    int fd;
    char* path;
} thread_data;

bool read_response(int fd, Response* response) {
    bool valid_line = false;
    bool status_line_found = false;
    char* line;
    char temp_buffer[2048];
    memset(temp_buffer, 'a', 2048);

    int total_received = recv(fd, temp_buffer, 2048, 0);
    int num_bytes = total_received;
    if (num_bytes == -1) {
        perror("recv");
        return false;
    }

    if (read_line(temp_buffer, &num_bytes, &line)) {
        status_line_found = parse_resp_statusline(line, response);
        valid_line = true;
        free(line);
    }
    printf("num_bytes: %d\n", num_bytes);

    if (num_bytes > 0) {
        memmove(temp_buffer, temp_buffer + num_bytes, 2048 - num_bytes);
        int n = num_bytes;
        num_bytes = total_received - num_bytes;
        total_received -= n;
    } else {
        total_received = recv(fd, temp_buffer, 2048, 0);
        num_bytes = total_received;
        if (num_bytes == -1) {
            perror("recv");
            return false;
        }
    }

    bool resp_header_finished = false;
    bool found_length = false;
    while(!resp_header_finished) {
        read_line(temp_buffer, &num_bytes, &line);
        printf("num_bytes: %d\n", num_bytes);
        if (num_bytes == 2) {
            resp_header_finished = true;
        }
        char* token = strtok(line, ":");
        if (strcmp(line, "Content-Length") == 0) {
            token = strtok(NULL, "\r");
            printf("lent: %d\n", atoi(token));
            response->content_length = atoi(token);
            found_length = true;
        }

        if (!resp_header_finished) {
            if (num_bytes > 0) {
                memmove(temp_buffer, temp_buffer + num_bytes, 2048 - num_bytes);
                /*memset(temp_buffer + (total_received - num_bytes), 'a', num_bytes);*/
                int n = num_bytes;
                num_bytes = total_received - num_bytes;
                total_received -= n;
            } else {
                total_received = recv(fd, temp_buffer, 2048, 0);
                num_bytes = total_received;
                if (num_bytes == -1) {
                    perror("recv");
                    return -1;
                }
            }
        }
        printf("buf: %s\n", temp_buffer);
        free(line);
    }

    if (!found_length) {
        return false;
    }

    response->data = malloc(response->content_length);
    memcpy(response->data, temp_buffer, total_received - 2);
    int length = total_received - 2;

    while (length < response->content_length) {
        num_bytes = recv(fd, temp_buffer, 2048, 0);
        if (num_bytes == -1) {
            perror("recv");
            return -1;
        }
        memcpy(response->data + length, temp_buffer, num_bytes);
        length += num_bytes;
    }

    return valid_line && status_line_found && resp_header_finished;

}

void* communicate(void* td) {
    struct thread_data data = *((struct thread_data*)td);
    int n = data.id;
    int fd = data.fd;
    char* path = data.path;
    int rv = 0;
    Request request;
    request.method = GET;
    request.request_uri = path;
    request.version = make_version(1, 0);
    char* msg;

    int size = create_request(&request, &msg);
    printf("request: %s\n", msg);

    rv = sendall(fd, msg, &size);

    if (rv == -1) {
        perror("send");
    }

    Response response;
    bool valid = read_response(fd, &response);
    if (valid) {
        int path_len = strlen(path);
        bool found_slash = false;
        while(!found_slash && path_len >= 0) {
           if (path[path_len] == '/') {
               found_slash = true;
           }
           path_len--;
        }
        
        FILE* file = fopen(path + path_len + 2, "wb");
        printf("path: %s\n", path + path_len + 2);
        fwrite(response.data, sizeof(char), response.content_length, file);
        fclose(file);
    }

    shutdown(fd, 2);
    pthread_exit(NULL);
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ./client [URL] [URL] ...");
        exit(1);
    }

    struct addrinfo hints = { 
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM, 0};


    struct addrinfo* p;
    int sockfd;
    int num_paths = argc - 1;
    thread_data* td = malloc(argc - 1);
    char s[INET6_ADDRSTRLEN];
    for (int n = 0; n < num_paths; n++) {
        struct addrinfo *servinfo;
        UrlInfo url_info = parse_url(argv[n + 1]);
        int rv = getaddrinfo(url_info.addr, url_info.port, &hints, &servinfo);
        td[n].path = malloc(strlen(url_info.file_path) + 1);
        strcpy(td[n].path, url_info.file_path);

        if (rv != 0) {
            printf("getaddrinfo: %s\n", gai_strerror(rv));
            exit(1);
        }

        for (p = servinfo; p != NULL; p = p->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd == -1) {
                perror("server: socket");
                continue;
            }

            rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
            pthread_t tid;

            td[n].fd = sockfd;
            td[n].id = n;
            int rc = pthread_create(&tid, NULL, communicate, (void *)&td[n]);
            pthread_join(tid, NULL);
            printf("cho\n");
            fflush(stdout);
            if (rv == -1) {
                shutdown(sockfd, 2);
                perror("client: connect");
                continue;
            }

            break;
        }
        inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof(s));
        printf("client: connecting to %s\n", s);

        freeaddrinfo(servinfo);
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }




    return 0;
}
