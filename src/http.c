#include "http.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Method get_method(const char* msg) {
    if (strcmp(msg, "GET") == 0) {
        return GET;
    }
    return METHOD_UNDEFINED;
}

uint32_t get_version(const char* msg) {

    if (strcmp(msg, "HTTP/0.9") == 0) {
        return make_version(0, 9);
    } else if (strcmp(msg, "HTTP/1.0") == 0) {
        return make_version(1, 0);
    } else if (strcmp(msg, "HTTP/1.1") == 0) {
        return make_version(1, 1);
    }

    return VERSION_UNDEFINED;
}

StatusCode get_status_code(const char* msg) {
    if (strcmp(msg, "200") == 0) {
        return OK;
    } else if (strcmp(msg, "400") == 0) {
        return BAD_REQUEST;
    } else if (strcmp(msg, "404") == 0) {
        return NOT_FOUND;
    }

    return STATUS_INVALID;
}

bool parse_request(const char* msg, Request* request) {
    bool valid = true;

    char* message = malloc(strlen(msg));
    strcpy(message, msg);
    char* line = strtok(message, "\r\n");

    // Parse Status Line : Method SP Request-URI SP HTTP-Version CRLF
    char* temp_msg = malloc(strlen(line));
    strcpy(temp_msg, line);
    // Method
    char* token = strtok(temp_msg, " ");
    request->method = get_method(token);
    if (request->method == METHOD_UNDEFINED) {
        valid = false;
    }

    // Request-Uri
    token = strtok(NULL, " ");
    uint32_t uri_len = strlen(token);
    request->request_uri = malloc(uri_len);
    strcpy(request->request_uri, token);

    // HTTP-Version
    token = strtok(NULL, " ");
    char ver[9];
    memcpy(ver, token, sizeof(ver) - 1);
    ver[8] = '\0';
    request->version = get_version(ver);

    if (request->version == VERSION_UNDEFINED) {
        valid = false;
    }

    free(temp_msg);
    free(message);

    return valid;
}

bool parse_response(const char* msg, Response* response) {
    bool valid = true;

    char* message = malloc(strlen(msg));
    strcpy(message, msg);
    char* line = strtok(message, "\r\n");
    uint32_t line_len = strlen(line);

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
        printf("len: %d\n",strlen(line));
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

bool create_request(Request* request, char* msg) {
}

bool create_response(Response* response, char* msg) {
}
