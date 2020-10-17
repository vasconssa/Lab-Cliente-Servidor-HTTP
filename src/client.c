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
#include "url.h"

#define MAX_THREAD_CREATE_TRIES 10
#define BUFFER_SIZE 2048

void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef struct thread_data {
    pthread_t tid;
    int fd;
    char* path;
    char* addr;
} thread_data;

bool read_response(int fd, Response* response) {
    bool status_line_found = false;
    char* line;
    char temp_buffer[BUFFER_SIZE];
    char* line_temp_buffer;
    memset(temp_buffer, 'a', BUFFER_SIZE);

    int total_received = recv(fd, temp_buffer, BUFFER_SIZE, 0);
    int num_bytes = total_received;
    if (num_bytes == -1 || total_received == 0) {
        perror("recv");
        shutdown(fd, 2);
        pthread_exit(NULL);
    }
    bool complete_line = false;

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
        bool line_read = read_line(line_temp_buffer, &num_bytes, &line);
        // try read one line, the first line should be the staus line
        if (line_read) {
            status_line_found = parse_resp_statusline(line, response);
            complete_line = true;
            free(line);
        }

        // If we could read a line, maybe buffer still have data, so we move unread data to begin of
        // persistent data buffer, in case a line was not read, it means the was a line in the current buffer,
        // so we must keep data in buffer intact and reallocate more space for incoming data, that way we can 
        // append received data in this space
        if (num_bytes > 0 && num_bytes < line_temp_buffer_size) {
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

    bool resp_header_finished = false;
    bool found_length = false;
    // Here we keep reading lines until we receive only \r\n after a read line
    while(!resp_header_finished) {
        complete_line = false;
        while (!complete_line) {
            complete_line = read_line(line_temp_buffer, &num_bytes, &line);

            // Check if response is finished 
            if (num_bytes == 2) {
                resp_header_finished = true;
            }

            // Check if read line has "Content-Length" header
            char* token = strtok(line, ":");
            if (complete_line && strcmp(line, "Content-Length") == 0) {
                token = strtok(NULL, "\r");
                response->content_length = atoi(token);
                found_length = true;
            }

            if (!resp_header_finished) {
                if (num_bytes > 0 && num_bytes < line_temp_buffer_size) {
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

    if (!found_length) {
        return false;
    }

    // Allocate space for data in response and then keep calling recv() until we 
    // receive all "Content-Length" bytes
    response->data = malloc(response->content_length);
    // move data to begin of buffer and copy to response->data
    memmove(line_temp_buffer, line_temp_buffer + num_bytes, line_temp_buffer_size - num_bytes);
    memcpy(response->data, line_temp_buffer, line_temp_buffer_size - 2);
    int length = line_temp_buffer_size - 2;
    while (length < response->content_length) {
        num_bytes = recv(fd, temp_buffer, BUFFER_SIZE, 0);
        if (total_received == -1) {
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
    pthread_t tid = pthread_self();
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
    rv = sendall(fd, msg, &size);
    if (rv == -1) {
        perror("send");
    }
    free(msg);

    printf("[%ld]: Sent request:\n", tid);
    printf("[%ld]: METHOD: %u, URI: %s, VERSION: %u.%u\n", tid, request.method, request.request_uri, 
                                                version_major(request.version), version_minor(request.version));

    Response response;
    bool valid = read_response(fd, &response);
    printf("[%ld]: Received response:\n", tid);
    printf("[%ld]: STATUS: %u, Content-Length: %d, VERSION: %u.%u\n", tid,  response.status, response.content_length, 
                                                version_major(response.version), version_minor(response.version));
    if (valid) {
        int path_len = strlen(path);
        bool found_slash = false;
        // Search for file name, we search from end to begin for a slash
        // when it is found, we get the filename
        while(!found_slash && path_len >= 0) {
           if (path[path_len] == '/') {
               found_slash = true;
           }
           path_len--;
        }

        // Case we don't have a filename in request uri
        char* file_path = path + path_len + 2;
        if (*(path + path_len + 2) == '\0') {
           file_path = "index.html"; 
        }
        
        // Save response data
        FILE* file = fopen(file_path, "wb");
        if (file != NULL) {
            printf("[%ld]: Saved file: %s\n", tid, file_path);
            fwrite(response.data, sizeof(char), response.content_length, file);
            fclose(file);
        }
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

    // Hints for getaddrinfo
    struct addrinfo hints = { 
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM, 0};

    struct addrinfo* p;
    int sockfd;
    int num_paths = argc - 1;
    // Allocate data for the number of URL receivde
    thread_data* td = malloc(num_paths * sizeof(*td));
    char s[INET6_ADDRSTRLEN];
    for (int n = 0; n < num_paths; n++) {
        struct addrinfo *servinfo;
        // Parse url
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
        // Fill servinfo struct
        int rv = getaddrinfo(url_info.addr, port, &hints, &servinfo);
        if (rv != 0) {
            printf("getaddrinfo: %s\n", gai_strerror(rv));
            destroy_url(&url_info);
            exit(1);
        }

        td[n].path = malloc(strlen(url_info.file_path) + 1);
        td[n].addr = malloc(strlen(url_info.addr) + 1);
        strcpy(td[n].path, url_info.file_path);
        strcpy(td[n].addr, url_info.addr);
        destroy_url(&url_info);

        // Create a thread to send request with first valid address in servinfo
        for (p = servinfo; p != NULL; p = p->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd == -1) {
                perror("server: socket");
                continue;
            }
            inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof(s));
            printf("client: connecting to %s\n", s);

            rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
            pthread_t tid = 0;

            td[n].fd = sockfd;
            // We try create a thread MAX_THREAD_CREATE_TRIES, if we can't, we shutdown socket
            // and wait all running threads to finish
            int rc = pthread_create(&tid, NULL, communicate, (void *)&td[n]);
            int max_tries = 0;
            while (rc != 0 && max_tries < MAX_THREAD_CREATE_TRIES) {
                rc = pthread_create(&tid, NULL, communicate, (void *)&td[n]);
                max_tries++;
            }
            if (rc != 0) {
                shutdown(sockfd, 2);
                break;
            }
            td[n].tid = tid;
            if (rv == -1) {
                shutdown(sockfd, 2);
                perror("client: connect");
                continue;
            }

            break;
        }

        freeaddrinfo(servinfo);
    }

    for (int n = 0; n < num_paths; n++) {
        pthread_join(td[n].tid, NULL);
    }
    free(td);

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }




    return 0;
}
