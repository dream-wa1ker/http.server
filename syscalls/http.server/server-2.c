// name : server-2.c
// date : 2026-11-07
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define ADDR INADDR_ANY
#define BACKLOG 10

//--------------------------------------- declarations ---------------------------------
// exit with failure code;
void panic(int fd, const char *msg);
void recv_request(int client_fd, char *request, size_t recv_size);
void send_response(int client_fd, const char *response, size_t send_size);
ssize_t get_filesize(const char *filepath);

//--------------------------------------- main program ----------------------------------
int main(void) {
    int sock_fd, client_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        panic(sock_fd, "setsockopt failed");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(ADDR);

    if (bind(sock_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        panic(sock_fd, "bind failed");
    }

    if (listen(sock_fd, BACKLOG) == -1) {
        panic(sock_fd, "listen failed");
    }

    while (true) {
        if ((client_fd = accept(sock_fd, NULL, NULL)) == -1) {
            panic(sock_fd, "accept failed");
        }
        char request[4096] = {0};
        size_t recv_size = sizeof(request) - 1;
        recv_request(client_fd, request, recv_size);
        // check if the complete request headers were received;
        if (strstr(request, "\r\n\r\n") == NULL) {
            fprintf(stderr, "incomplete HTTP request\n");
            close(client_fd);
            continue;
        }

        printf("REQUEST : \n%s\n", request);
        ssize_t filesize = get_filesize("www/index.html");

        if (filesize == -1) {
            close(client_fd);
            continue;
        }
        char body[filesize + 1];
        FILE *file = fopen("www/index.html", "r");
        if (!file) {
            perror("failed to open file");
            close(client_fd);
            continue;
        }
        if (fread(body, 1, (size_t)filesize, file) != (size_t)filesize) {
            perror("fread failed");
            fclose(file);
            close(client_fd);
            continue;
        }
        fclose(file);
        body[filesize] = '\0';

        const char *format =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s";

        // determine the length of the complete response;
        int length = snprintf(NULL, 0, format, body);

        if (length < 0) {
            fprintf(stderr, "snprintf failed\n");
            close(client_fd);
            continue;
        }

        // allocate the response buffer;
        char response[(size_t)length + 1];

        // construct the HTTP response;
        snprintf(response, sizeof(response), format, body);

        // send the response;
        send_response(client_fd, response, (size_t)length);
        close(client_fd);
    }

    return EXIT_SUCCESS;
}

//----------------------------------------- function definitions -------------------------------------------
void panic(int fd, const char *msg) {
    perror(msg);
    close(fd);
    exit(EXIT_FAILURE);
}

void recv_request(int client_fd, char *request, size_t recv_size) {
    size_t received = 0;
    while (received < recv_size) {
        ssize_t nbytes = read(client_fd, request + received, recv_size - received);
        if (nbytes == -1) {
            if (errno == EINTR) {
                fprintf(stderr, "got syscall interupt, proceeding...");
                continue;
            }
            perror("read failed");
            break;
        }

        if (nbytes == 0) {
            break;
        }
        received += (size_t)nbytes;
        request[received] = '\0';

        if (strstr(request, "\r\n\r\n") != NULL) {
            break;
        }
    }
}

void send_response(int client_fd, const char *response, size_t send_size) {
    size_t sent = 0;
    while (sent < send_size) {
        ssize_t nbytes = write(client_fd, response + sent, send_size - sent);
        if (nbytes == -1) {
            if (errno == EINTR) {
                fprintf(stderr, "got syscall interupt, proceeding...\n");
                continue;
            }
            perror("write failed");
            break;
        }

        if (nbytes == 0) {
            fprintf(stderr, "write did not make any progress, closing connection...\n");
            break;
        }

        sent += (size_t)nbytes;
    }
}

ssize_t get_filesize(const char *filepath) {
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) {
        perror("open failed");
        return -1;
    }
    struct stat st;
    if (fstat(file_fd, &st) == -1) {
        perror("stat failed");
        close(file_fd);
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "requested file is not a regular file : %s\n", filepath);
        close(file_fd);
        return -1;
    }

    close(file_fd);
    return (ssize_t)st.st_size;
}
