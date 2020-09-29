#include "url.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

UrlInfo parse_url(const char* url) {
    UrlInfo info = {0};
    info.valid = true;
    char* temp_url = malloc(strlen(url) + 1);
    strcpy(temp_url, url);
    //strip http:
    char* token = strtok(temp_url, "//");
    if (token == NULL || strcmp(token, "http:") != 0) {
        info.valid = false;
        return info;
    }
    printf("protocol: %s\n", token);

    token = strtok(NULL, "/");
    if (token != NULL) {
        printf("addr port: %s\n", token);
        strcpy(info.addr, token);
    } else {
        info.valid = false;
        return info;
    }
    token += strlen(token);
    if (url[token - temp_url] != '\0') {
        const char* u = &url[token - temp_url];
        printf("file path: %s\n", u);
        uint32_t path_len = strlen(u);
        info.file_path = malloc(path_len + 1);
        strcpy(info.file_path, u);
    } else {
        printf("file path: /\n");
        info.file_path = malloc(2);
        info.file_path[0] = '/';
        info.file_path[1] = '\0';
    }

    char temp[MAX_ADDR_SIZE];
    strcpy(temp, info.addr);
    token = strtok(temp, ":");
    printf("address: %s\n", token);
    strcpy(info.addr, token);
    if (token != NULL) {
        token = strtok(NULL, ":");
        printf("port: %s\n\n", token);
        info.port = atoi(token);
    } else {
        info.valid = false;
        return info;
    }

    free(temp_url);


    return info;
    
}

void destroy_url(UrlInfo* info) {
    free(info->file_path);
}
