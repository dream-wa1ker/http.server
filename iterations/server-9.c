// name : server-9.c
// date : 2026-11-09
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define BACKLOG 10
#define ADDR INADDR_ANY
#define MAX_REQUEST_LEN 4096

typedef struct {
    char path[256];
    char method[16];
    char version[16];
} request_line;

void get_current_time(char *buffer, size_t size) {
    time_t raw_time;
    struct tm *time_info;
    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(buffer, size, "%d/%b/%Y %T", time_info);
}

//===================================================================================== recv_request
int recv_request(int clientfd, char *request, size_t recv_size) {
    int is_valid_request = 0;
    size_t received = 0;
    while (received < recv_size) {
        ssize_t nbytes = read(clientfd, request + received, recv_size - received);

        // if read fails then..
        if (nbytes == -1) {
            if (errno == EINTR) {
                // continue in case of error interupt received.
                continue;
            }
            perror("read failed");
            return -1; // also breaks the loop;
        }

        if (nbytes == 0) {
            // break if the nbytes is zero, as it is EOF.
            return -1;
        }

        received += (size_t)nbytes;
        request[received] = '\0';
        if (strstr(request, "\r\n\r\n") != NULL) {
            return EXIT_SUCCESS;
        } else {
            is_valid_request = -1;
        }
    }
    return is_valid_request;
}

//========================================================================================== send_response
int send_response(int clientfd, const char *response, size_t send_size) {
    size_t sent = 0;
    while (sent < send_size) {
        ssize_t nbytes = write(clientfd, response + sent, send_size - sent);
        if (nbytes == -1) {
            if (errno == EINTR) {
                // continue in case of interupt.
                continue;
            }
            perror("write failed");
            return -1;
        }
        if (nbytes == 0) {
            fprintf(stderr, "write could not proceed...\n");
            return -1;
        }

        sent += (size_t)nbytes;
    }
    return EXIT_SUCCESS;
}

void parse_request_line(char *request, request_line *line) {
    memset(line, 0, sizeof(*line));
    char *eol;
    if ((eol = strstr(request, "\r\n")) == NULL) {
        return;
    }
    size_t len = eol - request;
    char *req_line = malloc(len + 1);
    if (!req_line) {
        return;
    }
    memcpy(req_line, request, len);
    req_line[len] = '\0';

    int bytes_read = 0;
    // %n will store the number of characters processed till that point.
    // if there were some garbage at the req_line, we will consider it as invalid request header;
    if (sscanf(req_line, "%15s %255s %15s %n", line->method, line->path, line->version, &bytes_read) != 3 || req_line[bytes_read] != '\0') {
        free(req_line);
        memset(line, 0, sizeof(*line));
        return;
    }
    free(req_line);
}

//--------------------------------------- main program ----------------------------------
int main(void) {
    int clientfd, sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(ADDR);

    if (bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char ip_addr[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &addr.sin_addr, ip_addr, sizeof(ip_addr)) == NULL) {
        perror("inet_ntop failed");
        strcpy(ip_addr, "0.0.0.0");
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    size_t url_len = snprintf(NULL, 0, "http://%s:%d/", ip_addr, PORT);
    char url[url_len + 1];
    snprintf(url, sizeof(url), "http://%s:%d/", ip_addr, PORT);
    printf("Serving HTTP on %s port %d (%s) ...\n", ip_addr, PORT, url);

    while (true) {
        if ((clientfd = accept(sockfd, NULL, NULL)) == -1) {
            perror("accept failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        struct timeval tv;
        tv.tv_sec = 10000 / 1000;
        tv.tv_usec = (10000 % 1000) * 1000;
        // set receive timeout
        if (setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            perror("setsockopt SO_RCVTIMEO failed");
        }

        // optionally set send timeout as well
        if (setsockopt(clientfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            perror("setsockopt SO_SNDTIMEO failed");
        }

        struct sockaddr_in client_addr = {0};
        socklen_t client_addr_len = sizeof(client_addr);

        if (getpeername(clientfd, (struct sockaddr *)&client_addr, &client_addr_len) == -1) {
            perror("failed to get client_addr");
            close(clientfd);
            continue;
        }

        char client_ip_addr[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_addr, sizeof(client_ip_addr)) == NULL) {
            perror("inet_ntop failed");
            strcpy(client_ip_addr, "unknown");
        }

        char current_time[24] = {0};
        get_current_time(current_time, sizeof(current_time));

        char request[MAX_REQUEST_LEN] = {0};
        if (recv_request(clientfd, request, sizeof(request) - 1) == -1) {
            fprintf(stderr, "[server] : invalid request format, closing conn...\n");
            close(clientfd);
            continue;
        }

        request_line line = {0};
        parse_request_line(request, &line);
        if (line.path[0] == '\0' || line.method[0] == '\0' || line.version[0] == '\0') {
            fprintf(stderr, "[server] : failed to parse the request header...\n");
            close(clientfd);
            continue;
        }
        size_t status_line_len = snprintf(NULL, 0, "%s - - [%s] \"%s %s %s\" 200 -", client_ip_addr, current_time, line.method, line.path, line.version);
        char *status_line = malloc(status_line_len + 1);
        if (!status_line) {
            perror("malloc failed");
            continue;
        }
        snprintf(status_line, status_line_len + 1, "%s - - [%s] \"%s %s %s\" 200 -", client_ip_addr, current_time, line.method, line.path, line.version);
        status_line[status_line_len] = '\0';
        printf("%s\n", status_line);
        free(status_line);

        static const char response[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 3\r\n"
            "Connection: close\r\n"
            "\r\n"
            "OK\n";

        send_response(clientfd, response, sizeof(response) - 1);

        close(clientfd);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
