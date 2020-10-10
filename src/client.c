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
    char* addr;
} thread_data;

bool read_response(int fd, Response* response) {
    bool valid_line = false;
    bool status_line_found = false;
    char* line;
    char temp_buffer[2048];
    char* line_temp_buffer;
    memset(temp_buffer, 'a', 2048);

    int total_received = recv(fd, temp_buffer, 2048, 0);
    int num_bytes = total_received;
    if (num_bytes == -1 || total_received == 0) {
        perror("recv");
        shutdown(fd, 2);
        pthread_exit(NULL);
    }
    bool complete_line = false;
    bool req_finished = false;

    int line_temp_buffer_size = total_received;
    line_temp_buffer = malloc(line_temp_buffer_size);
    memcpy(line_temp_buffer, temp_buffer, total_received);

    while (!complete_line) {
        if (read_line(line_temp_buffer, &num_bytes, &line)) {
            status_line_found = parse_resp_statusline(line, response);
            complete_line = true;
            free(line);
        }
        printf("num_bytes: %d\n", num_bytes);

        if (num_bytes > 0 && num_bytes < line_temp_buffer_size) {
            /*memmove(temp_buffer, temp_buffer + num_bytes, 2048 - num_bytes);*/
            memmove(line_temp_buffer, line_temp_buffer + num_bytes, line_temp_buffer_size - num_bytes);
            /*memset(temp_buffer + (2048 - num_bytes), 'a', num_bytes);*/
            int n = num_bytes;
            num_bytes = line_temp_buffer_size - num_bytes;
            line_temp_buffer_size -= n;
        } else {
            total_received = recv(fd, temp_buffer, 2048, 0);
            line_temp_buffer = realloc(line_temp_buffer, line_temp_buffer_size + total_received);
            memcpy(line_temp_buffer + line_temp_buffer_size, temp_buffer, total_received);
            line_temp_buffer_size += total_received;
            num_bytes = line_temp_buffer_size;
            if (num_bytes == -1 || total_received == 0) {
                perror("recv");
                shutdown(fd, 2);
                pthread_exit(NULL);
            }
        }
    }

    bool resp_header_finished = false;
    bool found_length = false;
    while(!resp_header_finished) {
        complete_line = false;
        while (!complete_line) {
            complete_line = read_line(line_temp_buffer, &num_bytes, &line);
            printf("line: %s", line);
            printf("num_bytes: %d\n", num_bytes);
            if (num_bytes == 2) {
                resp_header_finished = true;
            }
            char* token = strtok(line, ":");
            if (complete_line && strcmp(line, "Content-Length") == 0) {
                token = strtok(NULL, "\r");
                printf("lent: %d\n", atoi(token));
                response->content_length = atoi(token);
                found_length = true;
            }

            if (!resp_header_finished) {
                if (num_bytes > 0 && num_bytes < line_temp_buffer_size) {
                   /*memmove(temp_buffer, temp_buffer + num_bytes, 2048 - num_bytes);*/
                    memmove(line_temp_buffer, line_temp_buffer + num_bytes, line_temp_buffer_size - num_bytes);
                    /*memset(temp_buffer + (2048 - num_bytes), 'a', num_bytes);*/
                    int n = num_bytes;
                    num_bytes = line_temp_buffer_size - num_bytes;
                    line_temp_buffer_size -= n;
                } else {
                    total_received = recv(fd, temp_buffer, 2048, 0);
                    line_temp_buffer = realloc(line_temp_buffer, line_temp_buffer_size + total_received);
                    memcpy(line_temp_buffer + line_temp_buffer_size, temp_buffer, total_received);
                    line_temp_buffer_size += total_received;
                    num_bytes = line_temp_buffer_size;
                    if (num_bytes == -1 || total_received == 0) {
                        perror("recv");
                        shutdown(fd, 2);
                        pthread_exit(NULL);
                    }
                }
            }
        }
        /*printf("buf: %s\n", temp_buffer);*/
        free(line);
    }

    if (!found_length) {
        return false;
    }

    response->data = malloc(response->content_length);
    memmove(line_temp_buffer, line_temp_buffer + num_bytes, line_temp_buffer_size - num_bytes);
    memcpy(response->data, line_temp_buffer, line_temp_buffer_size - 2);
    int length = line_temp_buffer_size - 2;

    while (length < response->content_length) {
        num_bytes = recv(fd, temp_buffer, 2048, 0);
        if (num_bytes == -1 || total_received == 0) {
            perror("recv");
            shutdown(fd, 2);
            pthread_exit(NULL);
        }
        memcpy(response->data + length, temp_buffer, num_bytes);
        length += num_bytes;
    }
    free(line_temp_buffer);

    return complete_line && status_line_found && resp_header_finished;

}

void* communicate(void* td) {
    struct thread_data data = *((struct thread_data*)td);
    int n = data.id;
    int fd = data.fd;
    char* path = data.path;
    char* addr = data.addr;
    int rv = 0;
    Request request;
    request.method = GET;
    request.request_uri = path;
    request.request_addr = addr;
    request.version = make_version(1, 1);
    char* msg;

    int size = create_request(&request, &msg);
    printf("request: %s\n", msg);

    rv = sendall(fd, msg, &size);
    free(msg);

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

        char* file_path = path + path_len + 2;
        printf("path fopen: %s\n", file_path);
        if (*(path + path_len + 2) == '\0') {
           file_path = "index.html"; 
        }
        
        FILE* file = fopen(file_path, "wb");
        printf("path fopen: %s\n", file_path);
        fwrite(response.data, sizeof(char), response.content_length, file);
        fclose(file);
    }

    destroy_response(&response);
    destroy_request(&request);

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
    thread_data* td = malloc(num_paths * sizeof(*td));
    char s[INET6_ADDRSTRLEN];
    for (int n = 0; n < num_paths; n++) {
        struct addrinfo *servinfo;
        UrlInfo url_info = parse_url(argv[n + 1]);
        if (!url_info.valid) {
            printf("not valid\n");
            destroy_url(&url_info);
            continue;
        }
        char* port = url_info.port;
        if (port == NULL) {
            port = "80";
        }
        int rv = getaddrinfo(url_info.addr, port, &hints, &servinfo);
        td[n].path = malloc(strlen(url_info.file_path) + 1);
        td[n].addr = malloc(strlen(url_info.addr) + 1);
        strcpy(td[n].path, url_info.file_path);
        strcpy(td[n].addr, url_info.addr);
        destroy_url(&url_info);

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
    free(td);

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }




    return 0;
}
