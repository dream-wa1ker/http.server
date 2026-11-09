
// name : server-7.c
// date : 2026-11-07
// std  : C 23 standard

//---------------------------------- include header files -------------------------------
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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

//--------------------------------- function declarations -----------------------------------------
void panic(int fd, const char *msg);

int recv_request(int client_fd, char *request, size_t recv_size);

int send_response(int client_fd, const char *response, size_t send_size);

int read_into_buffer(int file_fd, char *buffer, size_t size);

int read_file(const char *path, char *buffer, size_t size);

bool file_exists(const char *path);

bool file_is_regular(const char *path);

off_t get_size(const char *path);

void error_response(int client_fd, const char *format, const char *version,
                    const char *content, http_status status);

const char *get_mime_type(const char *filepath);

//--------------------------------------- main program ----------------------------------
int main(void) {
    // define the socket;
    int socket_fd, client_fd;

    // ignore SIGPIPE when a client disconnects;
    signal(SIGPIPE, SIG_IGN);

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
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
            if (errno == EINTR)
                continue;

            panic(socket_fd, "accept failed");
        }

        char request[4096] = {0};

        if (recv_request(client_fd, request, sizeof(request) - 1) == -1) {
            fprintf(stderr, "invalid http header format...\n");
            close(client_fd);
            continue;
        }

        // print the request;
        // printf("%s\n", request);

        char method[16] = {0}, path[256] = {0}, version[16] = {0};

        // define the response format;
        // this format constructs only the HTTP headers;
        char format[] =
            "%s %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: %s\r\n"
            "\r\n";

        if (sscanf(request, "%15s %255s %15s", method, path, version) != 3) {
            fprintf(stderr, "sscanf request parsing failed\n");
            error_response(client_fd, format, "HTTP/1.1",
                           "Error : Invalid HTTP Request", HTTP_BAD_REQUEST);
            close(client_fd);
            continue;
        }

        if (strcmp(path, "/") == 0) {
            strcpy(path, "/index.html");
        }

        int file_path_length = snprintf(NULL, 0, "./www%s", path);

        if (file_path_length < 0) {
            close(client_fd);
            continue;
        }

        char filepath[(size_t)file_path_length + 1];
        snprintf(filepath, sizeof(filepath), "./www%s", path);

        if (strcmp(method, "GET") != 0) {
            fprintf(stderr, "unsupported method : %s\n", method);

            error_response(client_fd, format, "HTTP/1.1",
                           "Unsupported Method : Server only supports GET",
                           HTTP_METHOD_NOT_ALLOWED);

            close(client_fd);
            continue;
        }

        if (!file_exists(filepath) || !file_is_regular(filepath)) {
            fprintf(stderr, "file path not found : %s\n", filepath);

            error_response(client_fd, format, "HTTP/1.1",
                           "Error : File Not Found", HTTP_NOT_FOUND);

            close(client_fd);
            continue;
        }

        // determine the size of the requested file;
        off_t file_size = get_size(filepath);

        if (file_size < 0 || (uintmax_t)file_size > SIZE_MAX) {
            fprintf(stderr, "invalid file size : %s\n", filepath);

            error_response(client_fd, format, "HTTP/1.1",
                           "Error : Bad File Request",
                           HTTP_INTERNAL_SERVER_ERROR);

            close(client_fd);
            continue;
        }

        size_t filesize = (size_t)file_size;

        // allocate a buffer for the raw file contents;
        char *body = malloc(filesize == 0 ? 1 : filesize);

        if (body == NULL) {
            perror("malloc failed");

            error_response(client_fd, format, "HTTP/1.1",
                           "Error : Internal Server Error",
                           HTTP_INTERNAL_SERVER_ERROR);

            close(client_fd);
            continue;
        }

        // deliberately, this is a path traversal cve;
        // just keep it for the sake of keeping it...
        if (read_file(filepath, body, filesize) == -1) {
            fprintf(stderr, "error while reading the file : %s\n", filepath);

            free(body);

            error_response(client_fd, format, "HTTP/1.1",
                           "Error : Bad File Request",
                           HTTP_INTERNAL_SERVER_ERROR);

            close(client_fd);
            continue;
        }

        // determine the MIME type of the requested file;
        const char *mime_type = get_mime_type(filepath);

        // construct only the HTTP headers;
        int header_length = snprintf(
            NULL, 0, format,
            "HTTP/1.1",
            HTTP_OK,
            "OK",
            mime_type,
            filesize,
            "close");

        if (header_length < 0) {
            fprintf(stderr, "snprintf failed\n");
            free(body);
            close(client_fd);
            continue;
        }

        char response[(size_t)header_length + 1];

        snprintf(
            response, sizeof(response), format,
            "HTTP/1.1",
            HTTP_OK,
            "OK",
            mime_type,
            filesize,
            "close");

        // first send the HTTP headers;
        if (send_response(client_fd, response,
                          (size_t)header_length) == -1) {
            free(body);
            close(client_fd);
            continue;
        }

        // then send the raw file bytes as the response body;
        if (send_response(client_fd, body, filesize) == -1) {
            fprintf(stderr, "failed to send response body\n");
        }

        printf("method : %s\n", method);
        printf("path : %s\n", path);
        printf("http version : %s\n", version);
        printf("content type : %s\n", mime_type);
        printf("content length : %zu\n", filesize);

        // free the allocated file buffer;
        free(body);

        // parsing the request;
        close(client_fd);
    }

    close(socket_fd);
    return EXIT_SUCCESS;
}

//------------------------------------------- function definitions --------------------------------------------------------------------
//============================================= panic : exit with error code;
void panic(int fd, const char *msg) {
    perror(msg);
    close(fd);
    exit(EXIT_FAILURE);
}

//============================================= recv_request : recieve the request from client;
int recv_request(int client_fd, char *request, size_t recv_size) {
    size_t received = 0;

    while (received < recv_size) {
        ssize_t nbytes = read(client_fd, request + received, recv_size - received);

        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read failed");
            return -1;
        }

        if (nbytes == 0) {
            break;
        }

        received += (size_t)nbytes;
        request[received] = '\0';

        if (strstr(request, "\r\n\r\n") != NULL) {
            return 0;
        }
    }

    return -1;
}

//============================================= send_response : send a response to client;
int send_response(int client_fd, const char *response, size_t send_size) {
    size_t sent = 0;

    while (sent < send_size) {
        ssize_t nbytes = write(client_fd, response + sent, send_size - sent);

        if (nbytes == -1) {
            if (errno == EINTR) {
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

    return 0;
}

//============================================= read_into_buffer : read into a buffer from some open file descriptor.
int read_into_buffer(int file_fd, char *buffer, size_t size) {
    size_t got = 0;

    while (got < size) {
        size_t count = size - got;

        if (count > 4096)
            count = 4096;

        ssize_t nbytes = read(file_fd, buffer + got, count);

        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read failed");
            return -1;
        }

        // means, reached the end of file;
        // if expected bytes are not read, report failure.
        if (nbytes == 0) {
            fprintf(stderr, "unexpected EOF while reading file...\n");
            return -1;
        }

        got += (size_t)nbytes;
    }

    return EXIT_SUCCESS;
}

//============================================= read_file : read contents of file into a buffer;
int read_file(const char *path, char *buffer, size_t size) {
    int file_fd = open(path, O_RDONLY);

    if (file_fd == -1) {
        perror("open failed");
        return -1;
    }

    if (read_into_buffer(file_fd, buffer, size) == -1) {
        fprintf(stderr, "failed to read into buffer...\n");
        close(file_fd);
        return -1;
    }

    close(file_fd);
    return EXIT_SUCCESS;
}

//============================================= file_exists : check if the file exists.
bool file_exists(const char *path) {
    struct stat st_buf = {0};

    if (stat(path, &st_buf) == -1) {
        perror("stat failed");
        return false;
    }

    return true;
}

//============================================= file_is_regular : check if the file is a regular file
bool file_is_regular(const char *path) {
    struct stat st_buf = {0};

    if (stat(path, &st_buf) == -1) {
        return false;
    }

    if (!S_ISREG(st_buf.st_mode)) {
        return false;
    }

    return true;
}

//============================================= get_size : a function to get the full size of the path;
off_t get_size(const char *path) {
    struct stat st_buf = {0};

    if (stat(path, &st_buf) == -1) {
        perror("stat failed");
        return -1;
    }

    if (!S_ISREG(st_buf.st_mode) || st_buf.st_size < 0) {
        fprintf(stderr, "invalid regular file : %s\n", path);
        return -1;
    }

    // this is the actual size of the file;
    return st_buf.st_size;
}

//============================================= error_response : a response to send in case of error;
void error_response(int client_fd, const char *format, const char *version,
                    const char *content, http_status status) {

    const char *reason = "Error";

    if (status == HTTP_NOT_FOUND)
        reason = "Not Found";
    else if (status == HTTP_BAD_REQUEST)
        reason = "Bad Request";
    else if (status == HTTP_METHOD_NOT_ALLOWED)
        reason = "Method Not Allowed";
    else if (status == HTTP_INTERNAL_SERVER_ERROR)
        reason = "Internal Server Error";

    // determine the size of the HTTP headers;
    int length = snprintf(NULL, 0, format, version, status,
                          reason, "text/plain", strlen(content), "close");

    if (length < 0)
        return;

    char response[(size_t)length + 1];

    snprintf(response, sizeof(response), format, version, status,
             reason, "text/plain", strlen(content), "close");

    // first send the HTTP headers;
    if (send_response(client_fd, response, (size_t)length) == -1)
        return;

    // then send the response body;
    send_response(client_fd, content, strlen(content));
}

//============================================= get_mime_type : identify the mime type of the requested file
const char *get_mime_type(const char *filepath) {
    // find the extension of the requested file;
    const char *ext = strrchr(filepath, '.');

    // no extension found, return the default MIME type;
    if (ext == NULL)
        return "application/octet-stream";

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
