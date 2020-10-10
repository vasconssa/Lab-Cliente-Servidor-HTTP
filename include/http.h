#pragma once

#include <stdint.h>
#include <stdbool.h>

#define make_version(major, minor) \
    ((((int)(major)) << 22) | (((int)(minor)) << 12))

#define version_major(version) ((int)(version) >> 22)
#define version_minor(version) ((int)(version) >> 12 && 0x3ff)

#define VERSION_UNDEFINED UINT32_MAX

typedef enum StatusCode {
    OK = 200,
    BAD_REQUEST = 400,
    NOT_FOUND = 404,
    STATUS_INVALID
} StatusCode;

typedef enum Method {
    GET,
    METHOD_UNDEFINED
} Method;

typedef struct Request {
    Method method;
    char* request_addr;
    char* request_uri;
    int version;
} Request;

typedef struct Response {
    int version;
    StatusCode status;
    int content_length;
    char* data;
} Response;

int sendall(int fd, char* buf, int* len);

bool read_line(char* tcp_buffer, int* tcp_size, char** line);

bool parse_req_statusline(const char* msg, Request* request);

bool parse_resp_statusline(const char* msg, Response* response);

bool parse_response(const char* msg, Response* response);

int create_request(Request* request, char** msg);

int create_response(Response* response, char** msg);

void destroy_request(Request* request);

void destroy_response(Response* response);
