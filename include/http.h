#pragma once

#include <stdint.h>
#include <stdbool.h>

#define make_version(major, minor) \
    ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12))

#define version_major(version) ((uint32_t)(version) >> 22)
#define version_minor(version) ((uint32_t)(version) >> 12 && 0x3ff)

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
    char* request_uri;
    uint32_t version;
} Request;

typedef struct Response {
    uint32_t version;
    StatusCode status;
    uint32_t content_length;
} Response;


bool parse_request(const char* msg, Request* request);

bool parse_response(const char* msg, Response* response);

bool create_request(Request* request, char* msg);

bool create_response(Response* response, char* msg);
