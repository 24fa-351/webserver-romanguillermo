#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "request.h"

bool read_request_line(Request* req, int fd);
bool read_headers(Request* req, int fd);

Request* request_read_from_fd(int fd) {
    Request* req = malloc(sizeof(Request));
    if (read_request_line(req, fd) == false) {
        request_free(req);
        return NULL;
    }
    if (read_headers(req, fd) == false) {
        request_free(req);
        return NULL;
    }
    return req;
}

void request_print(Request* req) {
    printf("Request Method: %s\n", req->method);
    printf("Request Path: %s\n", req->path);
    printf("Request Version: %s\n", req->version);
    for (int i = 0; i < req->header_count; i++) {
        printf("Header %s: %s\n", req->headers[i].key, req->headers[i].value);
    }
}

void request_free(Request* req) {
    if (req == NULL) return;
    free(req->method);
    free(req->path);
    free(req->version);
    for (int i = 0; i < req->header_count; i++) {
        free(req->headers[i].key);
        free(req->headers[i].value);
    }
    free(req->headers);
    free(req);
}

char* read_line(int fd) {
    char* line = malloc(1000);
    int len = 0;
    while (1) {
        char ch;
        int n = read(fd, &ch, 1);
        if (n <= 0 || ch == '\n') break;
        if (ch != '\r') line[len++] = ch;
    }
    line[len] = '\0';
    return line;
}

bool read_request_line(Request* req, int fd) {
    char* line = read_line(fd);
    req->method = malloc(10);
    req->path = malloc(1000);
    req->version = malloc(10);
    if (sscanf(line, "%s %s %s", req->method, req->path, req->version) != 3) {
        free(line);
        return false;
    }
    free(line);
    return true;
}

bool read_headers(Request* req, int fd) {
    req->headers = malloc(sizeof(Header) * 100);
    req->header_count = 0;
    while (1) {
        char* line = read_line(fd);
        if (strlen(line) == 0) {
            free(line);
            break;
        }
        req->headers[req->header_count].key = malloc(1000);
        req->headers[req->header_count].value = malloc(1000);
        sscanf(line, "%[^:]: %s", req->headers[req->header_count].key, req->headers[req->header_count].value);
        req->header_count++;
        free(line);
    }
    return true;
}
