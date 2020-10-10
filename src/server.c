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
// Max file size: 1 MB
#define MAX_FILE_SIZE 1048576 

char* main_dir = "./";

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



// Parse Status Line : Method SP Request-URI SP HTTP-Version CRLF
bool read_request(int fd, Request* request) {
    bool valid_line = false;
    bool status_line_found = false;
    char* line;
    char temp_buffer[2048];
    char* line_temp_buffer;
    memset(temp_buffer, 'a', 2048);

    int total_received = recv(fd, temp_buffer, 2048, 0);
    int num_bytes = total_received;
    if (num_bytes == -1) {
        perror("recv");
        return -1;
    }

    bool complete_line = false;
    bool req_finished = false;

    int line_temp_buffer_size = total_received;
    line_temp_buffer = malloc(line_temp_buffer_size);
    memcpy(line_temp_buffer, temp_buffer, total_received);
    while (!complete_line) {
        if (read_line(line_temp_buffer, &num_bytes, &line)) {
            status_line_found = parse_req_statusline(line, request);
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
            if (num_bytes == -1) {
                perror("recv");
                return -1;
            }
        }
    }
    /*num_bytes = line_temp_buffer_size - num_bytes;*/
    /*free(line_temp_buffer);*/

    /*printf("buf: %s\n", line_temp_buffer);*/
    while (!req_finished) {
        printf("num_bytes aki: %d\n", num_bytes);
        complete_line = false;
        while (!complete_line) {
            complete_line = read_line(line_temp_buffer, &num_bytes, &line);
            if (num_bytes == 2) {
                req_finished = true;
            }

            if (!req_finished) {
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
                    if (num_bytes == -1) {
                        perror("recv");
                        return -1;
                    }
                }
            }
        }
        free(line);
    }
    free(line_temp_buffer);
    printf("aki\n");

    return status_line_found && req_finished;

}

void* communicate(void* fd) {
    struct thread_data data = *((struct thread_data*)fd);
    int n = data.id;
    int new_fd = data.fd;
    int rv = 0;
    Request request;

    bool res = read_request(new_fd, &request);
    if (res) {
        printf("valid\n");
    } else {
        printf("error\n");
    }
    printf("METHOD: %u, URI: %s, VERSION: %u.%u\n", request.method, request.request_uri, 
                                                    version_major(request.version), version_minor(request.version));
    printf("URI size: %lu\n", strlen(request.request_uri));
    if(strcmp(request.request_uri, "/") == 0) {
        request.request_uri = realloc(request.request_uri, strlen("index.html") + 1);
        strcpy(request.request_uri,  "/index.html");
    }

    char* file_path = malloc(strlen(main_dir) + strlen(request.request_uri) + 2);
    strcpy(file_path, main_dir);
    strcat(file_path, request.request_uri);

    FILE* file = fopen(file_path, "rb");
    printf("file: %s\n", file_path);
    int size = 0;
    char* file_buf = NULL;
    if (file != NULL) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        if (size > 0) {
            file_buf = malloc(size);
            printf("size file: %d\n", size);
            rewind(file);
            fread(file_buf, sizeof(char), size, file);
        } else {
            size = 0;
        }
        fclose(file);
    }
    char* resp_msg = NULL;
    Response response;
    response.version = request.version;
    response.status = OK;
    response.content_length = size;
    response.data = file_buf;
    size = create_response(&response, &resp_msg);
    if (file_buf != NULL) {
        free(file_buf);
    }
    /*resp[size - 1] = '\0';*/
    /*resp[size] = '\0';*/
    printf("size: %d\n", size);
    /*printf("%s\n", resp);*/

    rv = sendall(new_fd, resp_msg, &size);
    if (rv == -1) {
        perror("send");
    }
    destroy_request(&request);
    destroy_response(&response);
    free(resp_msg);
    free(file_path);

    shutdown(new_fd, 2);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./client server_addr port /path/to/file\n");
        exit(1);
    }
    main_dir = argv[3];
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
    int num_slot = 0;
    struct thread_data** slot = malloc(1 * sizeof(struct thread_data*));
    slot[num_slot] = malloc(10 * sizeof(struct thread_data));
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

        if (count == 10) {
            count = 0;
            num_slot++;
            slot = realloc(slot, (num_slot + 1) * sizeof(struct thread_data*));
            slot[num_slot] = malloc(10 * sizeof(struct thread_data));
        }

        slot[num_slot][count].fd = new_fd;
        slot[num_slot][count].id = count;
        int rc = pthread_create(&tid, NULL, communicate, (void *)&slot[num_slot][count]);
        pthread_join(tid, NULL);
        count++;
        
    }

    for (int i = 0; i < num_slot + 1; i++) {
        free(slot[i]);
    }
    free(slot);

    return 0;
}

