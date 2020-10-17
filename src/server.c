#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "http.h"

#define BACKLOG 10
#define MAX_THREADS 150
#define MAX_THREAD_CREATE_TRIES 10
#define BUFFER_SIZE 2048

char* main_dir = "./";

void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct thread_data {
    pthread_t tid;
    int fd;
};



// Parse Status Line : Method SP Request-URI SP HTTP-Version CRLF
bool read_request(int fd, Request* request) {
    bool status_line_found = false;
    char* line;
    char temp_buffer[BUFFER_SIZE];
    char* line_temp_buffer;
    memset(temp_buffer, 'a', BUFFER_SIZE);

    int total_received = recv(fd, temp_buffer, BUFFER_SIZE, 0);
    int num_bytes = total_received;
    if (num_bytes == -1) {
        perror("recv");
        shutdown(fd, 2);
        pthread_exit(NULL);
    }

    bool complete_line = false;
    bool req_finished = false;

    // recv () does not guarantee that all bytes will be received, nor do we know if the entire 
    // request will be received in just one call from recv ()., recv () does not guarantee 
    // that all the bytes will be received, or as we all know if the request will be received in one recv () call.
    //  Although in most cases it doesn't happen, we may have to use two or more recv () to receive 
    //  a complete line, so we cannot discard the data from the previous call, until we read a complete 
    //  line, Although in most cases does not happen, we have to use two or more recv () to get a complete 
    //  line, then we can not discard data from the previous chamanda while not read a complete line

    int line_temp_buffer_size = total_received;
    line_temp_buffer = malloc(line_temp_buffer_size);
    // Copy received data to persistent data buffer(line_temp_buffer)
    memcpy(line_temp_buffer, temp_buffer, total_received);
    while (!complete_line) {
        bool line_read = false;
        // try read one line, the first line should be the staus line
        line_read = read_line(line_temp_buffer, &num_bytes, &line);
        if (line_read) {
            status_line_found = parse_req_statusline(line, request);
            complete_line = true;
            free(line);
        }

        // If we could read a line, maybe buffer still have data, so we move unread data to begin of
        // persistent data buffer, in case a line was not read, it means the was a line in the current buffer,
        // so we must keep data in buffer intact and reallocate more space for incoming data, that way we can 
        // append received data in this space
        if (num_bytes > 0 && line_read) {
            memmove(line_temp_buffer, line_temp_buffer + num_bytes, line_temp_buffer_size - num_bytes);
            int n = num_bytes;
            num_bytes = line_temp_buffer_size - num_bytes;
            line_temp_buffer_size -= n;
        } else {
            total_received = recv(fd, temp_buffer, BUFFER_SIZE, 0);
            line_temp_buffer = realloc(line_temp_buffer, line_temp_buffer_size + total_received);
            memcpy(line_temp_buffer + line_temp_buffer_size, temp_buffer, total_received);
            line_temp_buffer_size += total_received;
            num_bytes = line_temp_buffer_size;
            if (total_received == -1) {
                perror("recv");
                shutdown(fd, 2);
                pthread_exit(NULL);
            }
        }
    }

    // Here we keep reading lines until we receive only \r\n after a read line
    while (!req_finished) {
        complete_line = false;
        while (!complete_line) {
            complete_line = read_line(line_temp_buffer, &num_bytes, &line);
            if (num_bytes == 2) {
                req_finished = true;
            }

            if (!req_finished) {
                if (num_bytes > 0 && complete_line) {
                    memmove(line_temp_buffer, line_temp_buffer + num_bytes, line_temp_buffer_size - num_bytes);
                    int n = num_bytes;
                    num_bytes = line_temp_buffer_size - num_bytes;
                    line_temp_buffer_size -= n;
                } else {
                    total_received = recv(fd, temp_buffer, BUFFER_SIZE, 0);
                    line_temp_buffer = realloc(line_temp_buffer, line_temp_buffer_size + total_received);
                    memcpy(line_temp_buffer + line_temp_buffer_size, temp_buffer, total_received);
                    line_temp_buffer_size += total_received;
                    num_bytes = line_temp_buffer_size;
                    if (total_received == -1) {
                        perror("recv");
                        shutdown(fd, 2);
                        pthread_exit(NULL);
                    }
                }
            }
        }
        free(line);
    }
    free(line_temp_buffer);

    return status_line_found && req_finished;

}

void* communicate(void* fd) {
    struct thread_data data = *((struct thread_data*)fd);
    pthread_t tid = pthread_self();
    int new_fd = data.fd;
    int rv = 0;
    Request request;
    request.request_addr = NULL;

    bool res = read_request(new_fd, &request);
    if (res) {
        printf("[%lu]: Received Request: \n", tid);
        printf("[%lu]: METHOD: %u, URI: %s, VERSION: %u.%u\n", tid, request.method, request.request_uri, 
                                                    version_major(request.version), version_minor(request.version));
    }

    // set filename to index.html case request uri is only /
    if(strcmp(request.request_uri, "/") == 0) {
        request.request_uri = realloc(request.request_uri, strlen("index.html") + 1);
        strcpy(request.request_uri,  "/index.html");
    }

    // Concatenate main directory with request uri
    char* file_path = malloc(strlen(main_dir) + strlen(request.request_uri) + 2);
    strcpy(file_path, main_dir);
    strcat(file_path, request.request_uri);

    FILE* file = fopen(file_path, "rb+");
    int size = 0;
    char* file_buf = NULL;
    if (errno == 0) {
        if (file != NULL) {
            fseek(file, 0, SEEK_END);
            size = ftell(file);
            file_buf = malloc(size);
            rewind(file);
            fread(file_buf, sizeof(char), size, file);
            fclose(file);
        }
    } else {
        perror("Error: file: ");
    }
    char* resp_msg = NULL;
    Response response;
    response.version = request.version;
    // Analyze request and choose return code, we send a custom html in case
    // of BAD_REQUEST of NOT_FOUND file
    if (!res) {
        response.status = BAD_REQUEST;
        char* bad_req = "<html>400 Bad Request</html>";
        response.content_length = strlen(bad_req);
        response.data = malloc(strlen(bad_req) + 1);
        strcpy(response.data, bad_req);
    } else {
        if (size >= 0 && file != NULL) {
            response.status = OK;
            response.content_length = size;
            response.data = file_buf;
        } else {
            response.status = NOT_FOUND;
            char* not_found = "<html>404 Not Found</html>";
            response.content_length = strlen(not_found);
            response.data = malloc(strlen(not_found) + 1);
            strcpy(response.data, not_found);
        }
    }
    size = create_response(&response, &resp_msg);
    printf("[%lu]: Sent response:\n", tid);
    printf("[%lu]: STATUS: %u, Content-Length: %d, VERSION: %u.%u\n", tid, response.status, response.content_length, 
                                                version_major(response.version), version_minor(response.version));

    rv = sendall(new_fd, resp_msg, &size);
    if (rv == -1) {
        perror("send");
    }

    // Clean up memory
    destroy_request(&request);
    destroy_response(&response);
    free(resp_msg);
    free(file_path);

    shutdown(new_fd, 2);
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./server server_addr port /path/to/main/directory\n");
        exit(1);
    }
    main_dir = argv[3];
    // Hints for getaddrinfo
    struct addrinfo hints = { 
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM, 0};

    struct addrinfo *servinfo;

    // fill servinfo struct
    int rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo);
    if (rv != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    struct addrinfo* p;
    int sockfd;
    int r;
    // Stop on first valid address and bind it to socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("server: socket");
            continue;
        }

        int yes = 1;
        r = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
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

    // It's possible that any bind works, so we finishes server
    if (r == -1) {
        printf("Was not possible bind any valid address, please check addres and port for valid range\n");
        return 1;
    }

    rv = listen(sockfd, BACKLOG);
    if (rv == -1) {
        perror("listen");
        return 1;
    }

    printf("server: waiting for connections...\n");

    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    // Threads slots: thread data has to persist while thread is running, and data pointer must continue valid,
    // so we create a array of slots, we can only have MAX_THREADS concurrent threads running, so we can reuse
    // this slots, when threads terminates
    struct thread_data slot[MAX_THREADS];
    while (true) {
        for (int count = 0; count < MAX_THREADS; count++) {
            sin_size = sizeof(their_addr);
            int new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
            if (new_fd == -1) {
                perror("accept");
                continue;
            }
            inet_ntop(their_addr.ss_family,
                      get_in_addr((struct sockaddr*)&their_addr),
                      s, sizeof(s));
            printf("server: got connection from %s\n", s);

            pthread_t tid = 0;

            slot[count].fd = new_fd;
            slot[count].tid = tid;
            // We try create a thread MAX_THREAD_CREATE_TRIES, if we can't, we shutdown socket
            // and wait all running threads to finish
            int rc = pthread_create(&tid, NULL, communicate, (void *)&slot[count]);
            int max_tries = 0;
            while (rc != 0 && max_tries < MAX_THREAD_CREATE_TRIES) {
                rc = pthread_create(&tid, NULL, communicate, (void *)&slot[count]);
                max_tries++;
            }
            if (rc != 0) {
                shutdown(new_fd, 2);
                break;
            }
            slot[count].tid = tid;
        }

        for (int count = 0; count < MAX_THREADS; count++) {
            pthread_join(slot[count].tid, NULL);
        }
    }


    return 0;
}

