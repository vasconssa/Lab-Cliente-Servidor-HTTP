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
    /*char* http = strstr(temp_url, "http://");*/
    char* token = strtok(temp_url, "//");
    if (token == NULL || strcmp(token, "http:") != 0) {
        info.valid = false;
        return info;
    }

    token = strtok(NULL, "/");
    if (token != NULL) {
        strcpy(info.addr, token);
        token += strlen(token);
    } else {
        info.valid = false;
        return info;
    }
    if (url[token - temp_url] != '\0') {
        const char* u = &url[token - temp_url];
        uint32_t path_len = strlen(u);
        info.file_path = malloc(path_len + 1);
        strcpy(info.file_path, u);
    } else {
        info.file_path = malloc(2);
        info.file_path[0] = '/';
        info.file_path[1] = '\0';
    }

    char* temp = malloc(strlen(info.addr) + 1);
    strcpy(temp, info.addr);
    token = strtok(temp, ":");
    if (token != NULL & strcmp(temp, info.addr) != 0) {
        strcpy(info.addr, token);
        if (token != NULL) {
            /*token = strtok(NULL, ":");*/
            if (token != NULL) {
                token = token + strlen(token) + 1;
                info.port = malloc(strlen(token) + 1);

                strcpy(info.port, token);
            } else {
                info.valid = false;
                free(temp);
                return info;
            }
        }
    } else {
        token = strtok(temp, "/");
        if (token != NULL) {
            strcpy(info.addr, token);
        } else {
            info.valid = false;
            free(temp);
            return info;
        }
    }
    free(temp);

    free(temp_url);


    return info;
    
}

void destroy_url(UrlInfo* info) {
    if (info->file_path) {
        free(info->file_path);
    }
    if (info->port) {
        free(info->port);
    }
}
