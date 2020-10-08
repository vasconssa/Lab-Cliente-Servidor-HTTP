#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef MAX_ADDR_SIZE
#define MAX_ADDR_SIZE 200
#endif

typedef struct UrlInfo {
    char addr[MAX_ADDR_SIZE];
    bool valid;
    char* port;
    char* file_path;
} UrlInfo;

UrlInfo parse_url(const char* url);

void destroy_url(UrlInfo* info);
