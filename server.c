#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "request.h"

#define DEFAULT_PORT 80
#define STATIC_DIR "./static/"
#define LISTEN_BACKLOG 5

int request_count = 0;
size_t bytes_received = 0;
size_t bytes_sent = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_response(int fd, int status_code, const char* status_text, const char* content_type, const char* body) {
    char header[1024];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n\r\n",
             status_code, status_text, content_type, strlen(body));
    write(fd, header, strlen(header));
    write(fd, body, strlen(body));
}

void handle_static(int fd, const char* path) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s%s", STATIC_DIR, path + 8); // Skip "/static/"

    FILE* file = fopen(file_path, "rb");
    if (!file) {
        send_response(fd, 404, "Not Found", "text/plain", "File not found");
        return;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* file_content = malloc(file_size);
    fread(file_content, 1, file_size, file);
    fclose(file);

    char header[1024];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: %zu\r\n\r\n",
             file_size);
    write(fd, header, strlen(header));
    write(fd, file_content, file_size);

    free(file_content);
}

void handle_stats(int fd) {
    pthread_mutex_lock(&stats_mutex);
    char body[1024];
    snprintf(body, sizeof(body),
             "<html><body>"
             "<h1>Server Stats</h1>"
             "<p>Requests: %d</p>"
             "<p>Bytes Received: %zu</p>"
             "<p>Bytes Sent: %zu</p>"
             "</body></html>",
             request_count, bytes_received, bytes_sent);
    pthread_mutex_unlock(&stats_mutex);

    send_response(fd, 200, "OK", "text/html", body);
}

void handle_calc(int fd, const char* path) {
    int a = 0, b = 0;
    sscanf(path, "/calc?a=%d&b=%d", &a, &b);
    char body[1024];
    snprintf(body, sizeof(body), "Result: %d", a + b);
    send_response(fd, 200, "OK", "text/plain", body);
}

void server_dispatch_request(Request* req, int fd) {
    pthread_mutex_lock(&stats_mutex);
    request_count++;
    bytes_received += strlen(req->method) + strlen(req->path) + strlen(req->version);
    pthread_mutex_unlock(&stats_mutex);

    if (strncmp(req->path, "/static", 7) == 0) {
        handle_static(fd, req->path);
    } else if (strcmp(req->path, "/stats") == 0) {
        handle_stats(fd);
    } else if (strncmp(req->path, "/calc", 5) == 0) {
        handle_calc(fd, req->path);
    } else {
        send_response(fd, 404, "Not Found", "text/plain", "404 Not Found");
    }
}

void* handle_connection(void* arg) {
    int fd = *(int*)arg;
    free(arg);

    Request* req = request_read_from_fd(fd);
    if (req) {
        request_print(req);
        server_dispatch_request(req, fd);
        request_free(req);
    }
    close(fd);
    return NULL;
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 2 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, LISTEN_BACKLOG);

    while (1) {
        int* client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        pthread_t thread;
        pthread_create(&thread, NULL, handle_connection, client_fd);
        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}
