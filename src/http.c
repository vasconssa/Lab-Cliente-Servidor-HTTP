#include "http.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

Method get_method(const char* msg) {
    if (msg != NULL) {
        if (strcmp(msg, "GET") == 0) {
            return GET;
        }
    }
    return METHOD_UNDEFINED;
}

int get_version(const char* msg) {
    if (msg != NULL) {
        if (strcmp(msg, "HTTP/0.9") == 0) {
            return make_version(0, 9);
        } else if (strcmp(msg, "HTTP/1.0") == 0) {
            return make_version(1, 0);
        } else if (strcmp(msg, "HTTP/1.1") == 0) {
            return make_version(1, 1);
        }
    }

    return VERSION_UNDEFINED;
}

StatusCode get_status_code(const char* msg) {
    if (msg != NULL) {
        if (strcmp(msg, "200") == 0) {
            return OK;
        } else if (strcmp(msg, "400") == 0) {
            return BAD_REQUEST;
        } else if (strcmp(msg, "404") == 0) {
            return NOT_FOUND;
        }
    }

    return STATUS_INVALID;
}

bool parse_req_statusline(const char* msg, Request* request) {
    bool valid = true;

    char* message = malloc(strlen(msg) + 1);
    strcpy(message, msg);
    char* line = strtok(message, "\r\n");

    // Parse Status Line : Method SP Request-URI SP HTTP-Version CRLF
    char* temp_msg = malloc(strlen(line) + 1);
    strcpy(temp_msg, line);
    // Method
    char* token = strtok(temp_msg, " ");
    request->method = get_method(token);
    if (request->method == METHOD_UNDEFINED) {
        return false;
    }

    // Request-Uri
    token = strtok(NULL, " ");
    valid = token != NULL;
    if (valid) {
        int uri_len = strlen(token);
        request->request_uri = malloc(uri_len + 1);
        strcpy(request->request_uri, token);
    }

    // HTTP-Version
    token = strtok(NULL, " ");
    valid = token != NULL;
    if (valid) {
        char ver[9];
        memcpy(ver, token, sizeof(ver) - 1);
        ver[8] = '\0';
        request->version = get_version(ver);

        if (request->version == VERSION_UNDEFINED) {
            valid = false;
        }
    }

    free(temp_msg);
    free(message);

    return valid;
}

bool parse_resp_statusline(const char* msg, Response* response) {
    bool valid = true;

    char* message = malloc(strlen(msg) + 1);
    strcpy(message, msg);
    char* line = strtok(message, "\r\n");

    // Parse Status Line : HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    char* temp_msg = malloc(strlen(line) + 1);
    strcpy(temp_msg, line);
    // HTTP-Version
    char* token = strtok(temp_msg, " ");
    response->version = get_version(token);
    if (response->version == VERSION_UNDEFINED) {
        valid = false;
    }

    // Status Code
    token = strtok(NULL, " ");
    response->status = get_status_code(token);
    free(temp_msg);
    free(message);

    return valid;
}

bool parse_response(const char* msg, Response* response) {
    bool valid = true;

    char* message = malloc(strlen(msg));
    strcpy(message, msg);
    char* line = strtok(message, "\r\n");
    int line_len = strlen(line);

    // Parse Status Line : HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    char* temp_msg = malloc(strlen(line));
    strcpy(temp_msg, line);
    // HTTP-Version
    char* token = strtok(temp_msg, " ");
    response->version = get_version(token);
    if (response->version == VERSION_UNDEFINED) {
        valid = false;
    }

    // Status Code
    token = strtok(NULL, " ");
    response->status = get_status_code(token);
    free(temp_msg);

    strcpy(message, msg + line_len);
    line = strtok(message, "\r\n");
    while (line) {
        line_len += strlen(line);
        printf("len: %lu\n", strlen(line));
        char* temp_msg = malloc(strlen(line));
        strcpy(temp_msg, line);
        char* token = strtok(temp_msg, ":");
        if (strcmp(temp_msg, "Content-Length") == 0) {
            token = strtok(NULL, "\r");
            printf("lent: %d\n", atoi(token));
            response->content_length = atoi(token);
        }
        strcpy(message, msg + line_len);
        line = strtok(message, "\r\n");
        printf("msg: %s\n", line);
        free(temp_msg);
    }

    return valid;
}

bool read_line(char* tcp_buffer, int* tcp_size, char** line) {

    bool valid = false;
    *line = NULL;

    char* temp_line = malloc(*tcp_size + 1);
    /*printf("tcp_size: %d\n", *tcp_size);*/
    memcpy(temp_line, tcp_buffer, *tcp_size);
    temp_line[*tcp_size] = '\0';
    char* token = strstr(temp_line, "\r\n");
    if (token != NULL) {
        *(token + 2) = '\0';
        *tcp_size = strlen(temp_line);
        *line = malloc(*tcp_size + 1);
        strcpy(*line, temp_line);
        /*printf("head: %s\n", *line);*/
        valid = true;
        /**tcp_size += 2;*/
    }
    free(temp_line);

    return valid;
}

int create_request(Request* request, char** msg) {
    char* method;
    int size = 0;
    if (request->method == GET) {
        method = "GET";
        size = strlen(method);
    } else {
        return 0;
    }

    char* version;
    if (request->version == make_version(0, 9)) {
        version = "HTTP/0.9";
    } else if (request->version == make_version(1, 0)) {
        version = "HTTP/1.0";
    } else if (request->version == make_version(1, 1)) {
        version = "HTTP/1.1";
    } else {
        return 0;
    }
    size += strlen(version);
    if (request->request_uri != NULL) {
        size += strlen(request->request_uri);
    } else {
        return 0;
    }
    if (request->request_addr != NULL) {
        /*size += strlen(request->request_addr);*/
    } else {
        return 0;
    }
    char* www = strstr(request->request_addr, "www");
    char* addr = request->request_addr;
    if (www != NULL) {
        addr += 4;
    }
    size += strlen(addr);

    size += 14;
    *msg = malloc(size);
    sprintf(*msg, "%s %s %s\r\nHost: %s\r\n\r\n", method, request->request_uri, version, addr);
    printf("size msg: %lu\n", strlen(*msg));

    return size;
}

int sendall(int fd, char* buf, int* len) {
    int total = 0;
    int bytesleft = *len;
    int n;

    while(total < *len) {
        n = send(fd, buf + total, bytesleft, 0);
        printf("send %d\n", n);
        if (n == -1) {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total;

    return  n == -1? -1 : 0;
}

int create_response(Response* response, char** msg) {
   char header[1000]; 
   char* reason_phrase = "OK";
   if (response->status == BAD_REQUEST) {
       reason_phrase = "Bad Request";
   } else if (response->status == NOT_FOUND) {
       reason_phrase = "Not Found";
   }
   sprintf(header, "HTTP/%u.%u %3d %s\r\nContent-Length:%d\r\n\r\n", version_major(response->version), version_minor(response->version),
                                                 response->status, reason_phrase, response->content_length);
   int size = strlen(header) + response->content_length;
   *msg = malloc(size);
   if (*msg != NULL) {
       memcpy(*msg, header, strlen(header));
       memcpy(*msg + strlen(header), response->data, response->content_length);
   }


   return size;
}

void destroy_request(Request* request) {
    if (request->request_uri) {
        free(request->request_uri);
    }
    if (request->request_addr) {
        free(request->request_addr);
    }
}

void destroy_response(Response* response) {
    if (response->content_length > 0 && response->data != NULL) {
        free(response->data);
    }
}
