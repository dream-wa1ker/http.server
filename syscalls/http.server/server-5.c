// name : server-5.c
// date : 2026-11-07
// std  : C 23 standard
// desc : http request parsing...

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

//--------------------------------------- main program ----------------------------------
int main(void) {
    int sock_fd, client_fd;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(sock_fd, 10) == -1) {
        perror("listen failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server Listening @ %s in %d\n", "0.0.0.0", 8080);

    while (true) {
        if ((client_fd = accept(sock_fd, NULL, NULL)) == -1) {
            perror("accept failed");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        char request[4096] = {0};
        size_t recv_size = sizeof(request) - 1;
        size_t received = 0;
        while (received < recv_size) {
            ssize_t nbytes = read(client_fd, request + received, recv_size - received);
            if (nbytes == -1) {
                if (errno == EINTR) {
                    perror("signal interrupt, proceeding...");
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

        if (strstr(request, "\r\n\r\n") == NULL) {
            fprintf(stderr, "invalid HTTP request, closing conn..\n");
            close(client_fd);
            continue;
        }

        printf("REQUEST : \n%s\n", request);
        // now that we got the request from the server; here is how to parse it;
        char method[16] = {0}, path[256] = {0}, version[64] = {0}, host[64] = {0};
        if (sscanf(request, "%15s %255s %63s %63s %63s", method, path, version, host, host) != 5) {
            fprintf(stderr, "unable to parse the request...\n");
            close(client_fd);
            continue;
        }

        printf("Method : %s\n", method);
        printf("Path : %s\n", path);
        printf("HTTP Version : %s\n", version);
        printf("Host : %s\n", host);
        close(client_fd);
    }
    close(sock_fd);
    return EXIT_SUCCESS;
}
