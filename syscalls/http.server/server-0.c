// name : server-0.c
// date : 2026-11-07
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

//--------------------------------------- main program ----------------------------------
int main(void) {

    // create a socket;
    int sock_fd;
    // assign and check the return value.
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed => sock_fd");
        exit(EXIT_FAILURE);
    }

    // print the socket file descriptor.
    printf("socket fd => %d\n", sock_fd);

    // fill the sockaddr_in struct;
    // zero intialise the struct.
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the socket, associate it with the network data.
    if (bind(sock_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind failed => sock_fd");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // start listening on the socket filedescriptor.
    // listens only 1 client in backlog
    if (listen(sock_fd, 1) == -1) {
        perror("listen failed => sock_fd");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // accept any client connections.
    // this will return a file descriptor.
    int client_fd;
    if ((client_fd = accept(sock_fd, NULL, NULL)) == -1) {
        perror("accept failed => client_fd");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // client fd
    printf("client fd => %d\n", client_fd);

    //--------------------------------------------------- the request from client -------------------------------------------------------
    // read the connection;
    char request[4096] = {0};
    size_t size_recv = sizeof(request) - 1;
    // currenlty we recieved none;
    size_t received = 0;
    // read until received < size of the buffer;
    while (received < size_recv) {
        ssize_t nbytes = read(client_fd, request + received, size_recv - received);
        if (nbytes == -1) {
            if (errno == EINTR) {
                perror("read");
                // we will continue for EINTR;
                continue;
            }
            perror("read");
            break;
        }

        if (nbytes == EOF) {
            // break on EOF;
            break;
        }

        received += (size_t)nbytes;
        request[received] = '\0';

        // at the end of the request body, there will be '\r\n\r\n'
        // strstr is to check if needle is in the haystack;
        if (strstr(request, "\r\n\r\n") != NULL)
            break;
    }
    printf("REQUEST : \n%s\n", request);

    //---------------------------------------------------- response to the client ---------------------------------------------------------------
    // the request buffer;
    const char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>Hello from Server</h1></body></html>";

    size_t size_send = sizeof(response) - 1;
    size_t sent = 0;
    while (sent < size_send) {
        ssize_t nbytes = write(client_fd, response + sent, size_send - sent);
        if (nbytes == -1) {
            if (errno == EINTR) {
                perror("write");
                // we will continue for EINTR;
                continue;
            }
            perror("write");
            break;
        }

        if (nbytes == 0) {
            fprintf(stderr, "write made no progress\n");
            break;
        }
        sent += (size_t)nbytes;
    }

    // close the connection.
    close(client_fd);
    close(sock_fd);

    return EXIT_SUCCESS;
}
