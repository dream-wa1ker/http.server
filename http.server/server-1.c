
// name : server-1.c
// date : 2026-11-07
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define ADDR INADDR_ANY

//--------------------------------------- main program ----------------------------------
int main(void) {
    puts("A Simple HTTP Server");
    int sock_fd, client_fd;

    // ignore SIGPIPE to prevent termination when clients disconnect;
    signal(SIGPIPE, SIG_IGN);

    // create a network socket;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set the socket option to reuse the local address;
    // @level SOL_SOCKET,
    // @option SO_REUSEADDR,
    // see man 2 setsockopt
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // fill in the network structs;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(ADDR);

    // bind to the socket
    if (bind(sock_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // listen for any connections, with backlog 10;
    if (listen(sock_fd, 10) == -1) {
        perror("listen failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // read the client connections in loop, a sequential http server;
    while (true) {
        if ((client_fd = accept(sock_fd, NULL, NULL)) == -1) {
            if (errno == EINTR)
                continue;

            perror("accept failed");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        printf("Client Connected : %d\n", client_fd);

        // setup to read the request ------------------------------------------------------------------------------------
        char request[4096] = {0};
        size_t size_recv = sizeof(request) - 1; // account for the null terminator;
        size_t received = 0;

        // while till the received is less than size_recv;
        while (received < size_recv) {
            ssize_t nbytes = read(client_fd, request + received, size_recv - received);

            if (nbytes == -1) {
                if (errno == EINTR) {
                    fprintf(stderr, "received syscall interrupt, proceeding...\n");
                    continue;
                }

                perror("read failed");
                break;
            }

            // case EOF received, break;
            if (nbytes == 0) {
                break;
            }

            // increment the offset;
            received += (size_t)nbytes;
            request[received] = '\0';

            // if the request has its end, break the loop.
            if (strstr(request, "\r\n\r\n") != NULL) {
                break;
            }
        }

        // check if the complete request headers were received;
        if (strstr(request, "\r\n\r\n") == NULL) {
            fprintf(stderr, "incomplete HTTP request\n");
            close(client_fd);
            continue;
        }

        printf("REQUEST : \n%s\n", request);

        // setup to write the response ------------------------------------------------------------------------------------
        const char response[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>Hello from the server!</h1></body></html>";

        size_t size_send = sizeof(response) - 1; // account for the null terminator;
        size_t sent = 0;

        // while till the sent is less than size_send;
        while (sent < size_send) {
            ssize_t nbytes = write(client_fd, response + sent, size_send - sent);

            if (nbytes == -1) {
                if (errno == EINTR) {
                    fprintf(stderr, "received syscall interrupt, proceeding...\n");
                    continue;
                }

                perror("write failed");
                break;
            }

            // case nbytes is 0, then there is no progress in write, exit;
            if (nbytes == 0) {
                fprintf(stderr, "write has no progress...\n");
                break;
            }

            // increment the offset;
            sent += (size_t)nbytes;
        }

        close(client_fd);
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}
