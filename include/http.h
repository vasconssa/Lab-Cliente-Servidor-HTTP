#pragma once

#include <stdint.h>
#include <stdbool.h>

#define make_version(major, minor) \
    ((((uint32_t)(major)) << 22) | (((uint32_t)(minor)) << 12))

#define version_major(version) ((uint32_t)(version) >> 22)
#define version_minor(version) ((uint32_t)(version) >> 12 && 0x3ff)

#define VERSION_UNDEFINED UINT32_MAX

typedef enum Method {
    GET,
    METHOD_UNDEFINED
} Method;

typedef struct Request {
    Method method;
    char* request_uri;
    uint32_t version;
} Request;

bool parse_request(const char* msg, Request* request);
