// name : server-6.c
// date : 2026-11-07
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef enum http_status {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_REQUEST_TIMEOUT = 408,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_NOT_IMPLEMENTED = 501,
    HTTP_SERVICE_UNAVAILABLE = 503,
    HTTP_VERSION_NOT_SUPPORTED = 505
} http_status;

//--------------------------------- definitions -----------------------------------------
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

void send_response(int client_fd, char *response, size_t send_size) {
    size_t sent = 0;
    while (sent < send_size) {
        ssize_t nbytes = write(client_fd, response + sent, send_size - sent);
        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("write failed");
            break;
        }

        if (nbytes == 0) {
            fprintf(stderr, "write could not proceed...\n");
            break;
        }

        sent += (size_t)nbytes;
    }
}

int read_into_buffer(int file_fd, char *buffer, size_t size) {
    size_t got = 0;
    while (got < size) {
        ssize_t nbytes = read(file_fd, buffer + got, size - got);
        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read failed");
            return -1;
        }

        // means, reached the end of file, and is a success.
        if (nbytes == 0) {
            break;
        }

        got += (size_t)nbytes;
        buffer[got] = '\0';
    }

    return EXIT_SUCCESS;
}

int read_file(const char *path, char *buffer, size_t size) {
    int file_fd = open(path, O_RDONLY);
    if (file_fd == -1) {
        perror("open failed");
        buffer = NULL;
        return -1;
    }

    if (read_into_buffer(file_fd, buffer, size) == -1) {
        fprintf(stderr, "failed to read into buffer...\n");
        buffer = NULL;
        close(file_fd);
        return -1;
    }

    close(file_fd);
    return EXIT_SUCCESS;
}

bool file_exists(const char *path) {
    struct stat st_buf = {0};
    if (stat(path, &st_buf) == -1) {
        perror("stat failed");
        return false;
    }
    return true;
}

bool file_is_regular(const char *path) {
    if (file_exists(path)) {
        struct stat st_buf = {0};
        stat(path, &st_buf);
        if (!S_ISREG(st_buf.st_mode)) {
            return false;
        }
        return true;
    }
    return false;
}

size_t get_size(const char *path) {
    if (!file_exists(path))
        return 0;
    struct stat st_buf = {0};
    stat(path, &st_buf);
    // if size_t is 0, we are not gonna proceed;
    return st_buf.st_size;
}

void error_response(int client_fd, const char *format, const char *version, const char *content, http_status status) {
    size_t length = snprintf(NULL, 0, format, version, status, "error", "text/plain", strlen(content), "close", content);
    char response[length + 1];
    snprintf(response, sizeof(response), format, version, status, "error", "text/plain", strlen(content), "close", content);
    response[length] = '\0';
    send_response(client_fd, response, sizeof(response) - 1);
    close(client_fd);
}

char *get_mime_type(const char *filepath) {
    // find the extension of the requested file;
    const char *ext = strrchr(filepath, '.');

    // no extension found, return the default MIME type;
    if (ext == NULL)
        return "text/plain";

    // compare the extensions and return the respective MIME type;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";

    if (strcmp(ext, ".css") == 0)
        return "text/css";

    if (strcmp(ext, ".js") == 0)
        return "text/javascript";

    if (strcmp(ext, ".txt") == 0)
        return "text/plain";

    if (strcmp(ext, ".json") == 0)
        return "application/json";

    if (strcmp(ext, ".png") == 0)
        return "image/png";

    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";

    if (strcmp(ext, ".svg") == 0)
        return "image/svg+xml";

    if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";

    if (strcmp(ext, ".ico") == 0)
        return "image/x-icon";

    // unknown extension;
    return "application/octet-stream";
}

//--------------------------------------- main program ----------------------------------
int main(void) {
    // define the socket;
    int socket_fd, client_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        panic(socket_fd, "socket failed");
    }

    int opt = 1;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        panic(socket_fd, "setsockopt failed");
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        panic(socket_fd, "bind failed");
    }

    if (listen(socket_fd, 10) == -1) {
        panic(socket_fd, "listen failed");
    }

    while (true) {
        if ((client_fd = accept(socket_fd, NULL, NULL)) == -1) {
            panic(socket_fd, "accept failed");
        }

        char request[4096];
        recv_request(client_fd, request, sizeof(request) - 1);
        if (strstr(request, "\r\n\r\n") == NULL) {
            fprintf(stderr, "invalid http header format...\n");
            close(client_fd);
            continue;
        }
        // print the request;
        // printf("%s\n", request);
        char method[16] = {0}, path[256] = {0}, version[16] = {0};
        if (sscanf(request, "%15s %255s %15s", method, path, version) != 3) {
            perror("sscanf request parsing failed");
            continue;
        }

        if (strcmp(path, "/") == 0) {
            strcpy(path, "/index.html");
        }
        int file_path_length = snprintf(NULL, 0, "./www%s", path);
        char filepath[file_path_length + 1];
        snprintf(filepath, sizeof(filepath), "./www%s", path);

        // define the response format;
        char format[] =
            "%s %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: %s\r\n"
            "\r\n"
            "%s";

        if (strcmp(method, "GET") != 0) {
            fprintf(stderr, "unsupported method : %s\n", method);
            error_response(client_fd, format, version, "Unsupported Method : Server only supports GET", HTTP_METHOD_NOT_ALLOWED);
            continue;
        }

        if (!file_exists(filepath)) {
            fprintf(stderr, "file path not found : %s\n", filepath);
            error_response(client_fd, format, version, "Error : File Not Found", HTTP_NOT_FOUND);
            continue;
        }

        size_t filesize = 0;
        if ((filesize = get_size(filepath)) == 0) {
            fprintf(stderr, "file buffer size is zero : %s\n", filepath);
            error_response(client_fd, format, version, "Error : File Not Found", HTTP_NOT_FOUND);
            continue;
        }

        char body[filesize];
        // deliberately, this is a path traversal cve; just keep it for the sake of keeping it...
        if (read_file(filepath, body, sizeof(body)) == -1) {
            fprintf(stderr, "error while reading the file : %s\n", filepath);
            error_response(client_fd, format, version, "Error : Bad File Request", HTTP_BAD_REQUEST);
            continue;
        }
        body[filesize] = '\0';
        char *mime_type = get_mime_type(filepath);
        size_t response_size = snprintf(NULL, 0, format, version, HTTP_OK, "ok", mime_type, strlen(body), "close", body);
        char response[response_size + 1];
        snprintf(response, sizeof(response), format, version, HTTP_OK, "ok", mime_type, strlen(body), "close", body);
        send_response(client_fd, response, sizeof(response) - 1);

        printf("method : %s\n", method);
        printf("path : %s\n", path);
        printf("http version : %s\n", version);

        // parsing the request;
        close(client_fd);
    }

    close(socket_fd);
    return EXIT_SUCCESS;
}

// NOTE:
// Currently, this is not a binary safe implementation; The null termination in the send_response and things, will not allow binary files to traverse..
// Also, this is a long implementation of a simple http server.
