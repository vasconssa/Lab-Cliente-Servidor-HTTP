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
    char ver[9];
    memcpy(ver, msg, sizeof(ver) - 1);
    ver[8] = '\0';

    if (strcmp(ver, "HTTP/0.9") == 0) {
        return make_version(0, 9);
    } else if (strcmp(ver, "HTTP/1.0") == 0) {
        return make_version(1, 0);
    } else if (strcmp(ver, "HTTP/1.1") == 0) {
        return make_version(1, 1);
    }

    return VERSION_UNDEFINED;
}

bool parse_request(const char* msg, Request* request) {
    bool valid = true;
    char* temp_msg = malloc(strlen(msg));
    strcpy(temp_msg, msg);
    // Parse HEADER : Method SP Request-URI SP HTTP-Version CRLF
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
    uint32_t ver_len = strlen(token);
    request->version = get_version(token);

    if (request->version == VERSION_UNDEFINED) {
        valid = false;
    }

    return valid;
}
