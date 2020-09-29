#include "url.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
    char url[] = "http://www.google.com:12345/file";
    char url1[] = "http://www.google.com:12345/";
    char url2[] = "http://127.0.0.1:12345/";
    char url3[] = "http://127.0.0.1:12345/path/to/file";
    char url4[] = "http://127.0.0.1:12345/file";
    char url5[] = "http://localhost:12345/";
    char url6[] = "http://localhost:12345/path/to/file";
    char url7[] = "http://localhost:12345/file";
    char url8[] = "localhost/";

    UrlInfo info = parse_url(url);
    assert(info.valid);
    assert(strcmp(info.addr, "www.google.com") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/file") == 0);
    destroy_url(&info);

    info = parse_url(url1);
    assert(info.valid);
    assert(strcmp(info.addr, "www.google.com") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/") == 0);
    destroy_url(&info);
        
    info = parse_url(url2);
    assert(info.valid);
    assert(strcmp(info.addr, "127.0.0.1") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/") == 0);
    destroy_url(&info);
        
    info = parse_url(url3);
    assert(info.valid);
    assert(strcmp(info.addr, "127.0.0.1") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/path/to/file") == 0);
    destroy_url(&info);
        
    info = parse_url(url4);
    assert(info.valid);
    assert(strcmp(info.addr, "127.0.0.1") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/file") == 0);
    destroy_url(&info);
        
    info = parse_url(url5);
    assert(info.valid);
    assert(strcmp(info.addr, "localhost") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/") == 0);
    destroy_url(&info);
        
    info = parse_url(url6);
    assert(info.valid);
    assert(strcmp(info.addr, "localhost") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/path/to/file") == 0);
    destroy_url(&info);
        
    info = parse_url(url7);
    assert(info.valid);
    assert(strcmp(info.addr, "localhost") == 0);
    assert(info.port == 12345);
    assert(strcmp(info.file_path, "/file") == 0);
    destroy_url(&info);
        
    info = parse_url(url8);
    assert(!info.valid);
    destroy_url(&info);
        
}
