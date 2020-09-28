#include "http.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    Request request;
    char msg[] = "GET /index.html HTTP/1.1\r\n\r\n";
    bool res = parse_request(msg, &request);

    if (!res) {
        printf("failed\n");
    }

    printf("METHOD: %u, URI: %s, VERSION: %u.%u\n", request.method, request.request_uri, 
                                                    version_major(request.version), version_minor(request.version));
    free(request.request_uri);

    Response response;
    char resp[] = "HTTP/1.1 200 OK\r\nContent-Length:20\r\n";
    bool re = parse_response(resp, &response);
    printf("VERSION: %u.%u, STATUS: %d, LENGTH: %d\n", version_major(response.version), version_minor(response.version),
                                                      response.status, response.content_length);

    return 0;
}
