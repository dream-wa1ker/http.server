
// name : server-4.c
// date : 2026-11-07
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT 8080
#define ADDR INADDR_ANY
#define BACKLOG 10

//---------------------------------- function declarations -----------------------------
void panic(int fd, const char *msg);
void recv_request(int client_fd, char *request, size_t recv_size);
void send_response(int client_fd, char *response, size_t send_size);
int read_file(int file_fd, char *buffer, size_t file_size);
ssize_t get_size(const char *filepath);

//--------------------------------------- main program ----------------------------------
int main(void) {
    // initalise the file descriptors for socket, the client and the file to serve.
    int sock_fd, client_fd, file_fd;
    int opt = 1;

    // prevent termination when writing to a disconnected client;
    signal(SIGPIPE, SIG_IGN);

    // create a new network socket;
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // now set the socket options to reuse the local address;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        panic(sock_fd, "setsockopt failed");
    }

    // fill in the strucs to associate the socket with the network connection.
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(ADDR);

    // now bind;
    if (bind(sock_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        panic(sock_fd, "bind failed");
    }

    // we need to listen in the port, with a backlog of defined value;
    // backlog is the number of clients that will wait during an active connection.
    if (listen(sock_fd, BACKLOG) == -1) {
        panic(sock_fd, "listen failed");
    }

    // while loop and accept connections from the clients.
    while (true) {
        if ((client_fd = accept(sock_fd, NULL, NULL)) == -1) {
            if (errno == EINTR)
                continue;

            panic(sock_fd, "accept failed");
        }

        // if the accept succeeds, then we need to first receive the request;
        // a request buffer of size 4096 bytes; account for the null terminator.
        char request[4096] = {0};
        size_t recv_size = sizeof(request) - 1;

        recv_request(client_fd, request, recv_size);

        // the HTTP request headers end with \r\n\r\n;
        if (strstr(request, "\r\n\r\n") == NULL) {
            fprintf(stderr, "invalid http request format, closing conn...\n");
            close(client_fd);
            continue;
        }

        printf("REQUEST : \n%s\n", request);

        // this is the format of a http response.
        const char format[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s";

        // read the file : www/index.html;
        ssize_t filesize = get_size("www/index.html");

        if (filesize == -1) {
            fprintf(stderr, "failed to get the size of file...\n");
            close(client_fd);
            continue;
        }

        // open and read the file;
        if ((file_fd = open("www/index.html", O_RDONLY)) == -1) {
            perror("failed to open www/index.html");
            close(client_fd);
            continue;
        }

        char buffer[(size_t)filesize + 1];

        if (read_file(file_fd, buffer, (size_t)filesize) == -1) {
            close(file_fd);
            close(client_fd);
            continue;
        }

        close(file_fd);
        buffer[filesize] = '\0';

        // use snprintf to constuct the body;
        int body_len = snprintf(NULL, 0, format, buffer);

        if (body_len < 0) {
            fprintf(stderr, "snprintf failed\n");
            close(client_fd);
            continue;
        }

        size_t body_size = (size_t)body_len + 1;
        char response[body_size];

        snprintf(response, sizeof(response), format, buffer);

        send_response(client_fd, response, (size_t)body_len);
        close(client_fd);
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}

//--------------------------------------------- function definitions------------------------------------------
void panic(int fd, const char *msg) {
    perror(msg);
    close(fd);
    exit(EXIT_FAILURE);
}

void recv_request(int client_fd, char *request, size_t recv_size) {
    size_t received = 0;

    while (received < recv_size) {
        ssize_t nbytes = read(client_fd, request + received, recv_size - received);

        // if the nbytes is -1, then there was some error, and you need to mitigate it;
        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("read failed");
            // break the loop and stop reading if read failed;
            break;
        }

        // if nbytes is 0, then the read reached the end of stream;
        if (nbytes == 0) {
            break;
        }

        received += (size_t)nbytes;
        request[received] = '\0';

        // check if the HTTP headers have ended;
        if (strstr(request, "\r\n\r\n") != NULL) {
            break;
        }
    }
}

void send_response(int client_fd, char *response, size_t send_size) {
    size_t sent = 0;

    while (sent < send_size) {
        ssize_t nbytes = write(client_fd, response + sent, send_size - sent);

        // if the nbytes is -1, then there was some error, and you need to mitigate it;
        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("write failed");
            // break the loop and stop writing if write failed;
            break;
        }

        // nbytes == 0 for write does not mean end of file;
        if (nbytes == 0) {
            fprintf(stderr, "write could not proceed...\n");
            break;
        }

        sent += (size_t)nbytes;
    }
}

int read_file(int file_fd, char *buffer, size_t size) {
    size_t got = 0;

    while (got < size) {
        ssize_t nbytes = read(file_fd, buffer + got, size - got);

        // if the nbytes is -1, then there was some error;
        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("read failed");
            return -1;
        }

        // unexpected EOF before reading the complete file;
        if (nbytes == 0) {
            fprintf(stderr, "unexpected EOF while reading file\n");
            return -1;
        }

        got += (size_t)nbytes;
    }

    return 0;
}

ssize_t get_size(const char *filepath) {
    int fd = open(filepath, O_RDONLY);

    if (fd == -1) {
        perror("open failed");
        return -1;
    }

    struct stat stat_buf;

    if (fstat(fd, &stat_buf) == -1) {
        perror("fstat failed");
        close(fd);
        return -1;
    }

    close(fd);

    // check if the requested file path is a regular file;
    if (!S_ISREG(stat_buf.st_mode)) {
        fprintf(stderr, "the file : %s is not a regular file\n", filepath);
        return -1;
    }

    // check for invalid file size;
    if (stat_buf.st_size < 0) {
        fprintf(stderr, "invalid file size\n");
        return -1;
    }

    // this is the size of the entire file;
    return (ssize_t)stat_buf.st_size;
}
