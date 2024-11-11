#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "http_message.h"

#define DEFAULT_PORT 8080
#define LISTEN_BACKLOG 5
#define STATIC_DIR "./static"

int total_requests = 0;
int total_received_bytes = 0;
int total_sent_bytes = 0;

// Helper function to send an HTTP response
void send_response(int sock_fd, const char* status, const char* content_type, const char* body) {
    char response[4096];
    int content_length = body ? strlen(body) : 0;
     snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, content_type, content_length, body ? body : "");

    write(sock_fd, response, strlen(response));
    close(sock_fd);
}

// Handle the /static route
void handle_static(int sock_fd, const char* path) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", STATIC_DIR, path + 7); // Skip "/static"
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        send_response(sock_fd, "404 Not Found", "text/plain", "File not found");
        return;
    }

    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* file_content = malloc(file_size);
    fread(file_content, 1, file_size, file);
    fclose(file);

    char headers[256];
    snprintf(headers, sizeof(headers),
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: %d\r\n"
             "Content-Type: text/plain\r\n"
             "Connection: close\r\n"
             "\r\n",
             file_size);
    write(sock_fd, headers, strlen(headers));
    write(sock_fd, file_content, file_size);

    total_sent_bytes += strlen(headers) + file_size;
    free(file_content);
}

// Handle the /stats route
void handle_stats(int sock_fd) {
    char body[1024];
    snprintf(body, sizeof(body),
             "<html><body><h1>Server Stats</h1>"
             "<p>Total Requests: %d</p>"
             "<p>Total Received Bytes: %d</p>"
             "<p>Total Sent Bytes: %d</p></body></html>",
             total_requests, total_received_bytes, total_sent_bytes);
    send_response(sock_fd, "200 OK", "text/html", body);
}

// Handle the /calc route
void handle_calc(int sock_fd, const char* query) {
    int a = 0, b = 0;
    sscanf(query, "a=%d&b=%d", &a, &b);
    char body[256];
    snprintf(body, sizeof(body), "<html><body><h1>Calculation Result</h1><p>%d + %d = %d</p></body></html>", a, b, a + b);
    send_response(sock_fd, "200 OK", "text/html", body);
}

// Process HTTP request
void process_request(int sock_fd, http_client_message_t* http_msg) {
    total_requests++;
    total_received_bytes += strlen(http_msg->method) + strlen(http_msg->path);

    if (strncmp(http_msg->path, "/static", 7) == 0) {
        handle_static(sock_fd, http_msg->path);
    } else if (strncmp(http_msg->path, "/stats", 6) == 0) {
        handle_stats(sock_fd);
    } else if (strncmp(http_msg->path, "/calc", 5) == 0) {
        char* query = strchr(http_msg->path, '?');
        if (query) {
            handle_calc(sock_fd, query + 1);
        } else {
            send_response(sock_fd, "400 Bad Request", "text/plain", "Missing query parameters");
        }
    } else {
        send_response(sock_fd, "404 Not Found", "text/plain", "Route not found");
    }
}

// Thread function to handle a connection
void handleConnection(int* sock_fd_ptr) {
    int sock_fd = *sock_fd_ptr;
    free(sock_fd_ptr);

    while (1) {
        http_client_message_t* http_msg;
        http_read_result_t result;

        read_http_client_message(sock_fd, &http_msg, &result);
        if (result == BAD_REQUEST) {
            printf("Bad request\\n");
            close(sock_fd);
            break;
        } else if (result == CLOSED_CONNECTION) {
            printf("Closed connection\\n");
            close(sock_fd);
            break;
        }

        process_request(sock_fd, http_msg);
        http_client_message_free(http_msg);
    }
}

// Main function
int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1 && strcmp(argv[1], "-p") == 0 && argc > 2) {
        port = atoi(argv[2]);
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in socket_address;
    memset(&socket_address, '\0', sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socket_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr*)&socket_address, sizeof(socket_address)) < 0) {
        perror("bind");
        exit(1);
    }
    if (listen(socket_fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Server is listening on port %d\\n", port);

    while (1) {
        pthread_t thread;
        int* client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = accept(socket_fd, NULL, NULL);
        if (*client_fd_ptr < 0) {
            perror("accept");
            free(client_fd_ptr);
            continue;
        }
        pthread_create(&thread, NULL, (void* (*)(void*))handleConnection, client_fd_ptr);
        pthread_detach(thread);
    }

    return 0;
}