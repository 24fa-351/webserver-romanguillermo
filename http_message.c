#include <stdlib.h>
#include "http_message.h"
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

bool is_complete_http_message(char* buffer) {
    if (strstr(buffer, "\r\n\r\n") != NULL) {
        return true;
    }
    return false;
}

void read_http_client_message(int client_sock,
    http_client_message_t** msg,
    http_read_result_t* result) {
    
    *msg = malloc(sizeof(http_client_message_t));
    char buffer[4096] = {0};
    int total_bytes_read = 0;

    // Read data from the client until a complete HTTP message is received
    while (!is_complete_http_message(buffer)) {
        int bytes_read = read(client_sock, buffer + total_bytes_read, sizeof(buffer) - total_bytes_read - 1);
        if (bytes_read == 0) {
            *result = CLOSED_CONNECTION;
            free(*msg);
            return;
        }
        if (bytes_read < 0) {
            *result = BAD_REQUEST;
            free(*msg);
            return;
        }
        total_bytes_read += bytes_read;
        buffer[total_bytes_read] = '\0';
    }

    // Extract the request line (method, path, HTTP version)
    char* request_line = strtok(buffer, "\r\n");
    if (!request_line) {
        *result = BAD_REQUEST;
        free(*msg);
        return;
    }

    char* method = strtok(request_line, " ");
    char* path = strtok(NULL, " ");
    char* http_version = strtok(NULL, " ");

    if (!method || !path || !http_version) {
        *result = BAD_REQUEST;
        free(*msg);
        return;
    }

    // Validate HTTP version
    if (strcmp(http_version, "HTTP/1.1") != 0) {
        *result = BAD_REQUEST;
        free(*msg);
        return;
    }

    (*msg)->method = strdup(method);
    (*msg)->path = strdup(path);
    (*msg)->http_version = strdup(http_version);

    // Skip headers
    char* headers_end = strstr(buffer, "\r\n\r\n");
    if (headers_end) {
        (*msg)->headers = strdup(buffer + (headers_end - buffer) + 4);
    } else {
        (*msg)->headers = NULL;
    }

    (*msg)->body = NULL;
    (*msg)->body_length = 0;

    *result = MESSAGE;
}



void http_client_message_free(http_client_message_t* msg) { 
    if (msg->method) free(msg->method);
    if (msg->path) free(msg->path);
    if (msg->http_version) free(msg->http_version);
    if (msg->headers) free(msg->headers);
    if (msg->body) free(msg->body);
    free(msg);
 }