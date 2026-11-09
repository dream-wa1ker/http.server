// name : server-8.c
// date : 2026-11-08
// std  : C 23 standard

//---------------------------------------------------------------------------------------
//---------------------------------- include header files -------------------------------
//---------------------------------------------------------------------------------------

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//---------------------------------------------------------------------------------------
//--------------------------------- struct defintions and enums -------------------------
//---------------------------------------------------------------------------------------

#define PORT 8080
#define ADDR INADDR_ANY
#define BACKLOG 10
#define MAX_REQUEST_LEN 4096
#define CVE_IS_FIXED true

// define the mime struct, which is a match for extension and mimetype;
typedef struct {
    const char *ext;
    const char *mimetype;
} mime;

// define how the request line looks;
typedef struct {
    char path[256];
    char method[16];
    char version[16];
} request_line;

// define the http_status enum, which is a mapping of integer response code and names.
typedef enum {
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

// the http response code with the response message
typedef struct {
    char *status_msg;
    http_status status;
} http_response_code;

//---------------------------------------------------------------------------------------
//--------------------------------- function declarations ------------------------------
//---------------------------------------------------------------------------------------

void panic(int sockfd, const char *msg);
void build_socket(int *sockfd);
void handle_client(int sockfd);
int recv_request(int clientfd, char *request, size_t recv_size);
void parse_request_line(char *request, request_line *line);
int send_response(int clientfd, const char *response_header, size_t send_size);
int response_error(int clientfd, const char *msg, char *version, http_status status);
int read_into_buffer(int filefd, char *buffer, size_t size);
int read_file(const char *path, char *buffer, size_t size);
bool file_exists(const char *path);
bool file_is_regular(const char *path);
off_t get_size(const char *path);
const char *get_mime_type(char *filepath);

//---------------------------------------------------------------------------------------
//--------------------------------------- main program ----------------------------------
//---------------------------------------------------------------------------------------

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    int sockfd = 0;
    build_socket(&sockfd);
    printf("[server] : socket fd => %d\n", sockfd);
    handle_client(sockfd);
    close(sockfd);
    return EXIT_SUCCESS;
}

//---------------------------------------------------------------------------------------
//--------------------------------------- function defintions ---------------------------
//---------------------------------------------------------------------------------------

//============================================================================= handle_client;
void handle_client(int sockfd) {
    int clientfd = 0;
    while (true) {
        if ((clientfd = accept(sockfd, NULL, NULL)) == -1) {
            panic(sockfd, "accept failed");
        }

        // print client file descriptor;
        printf("[server] : client fd => %d\n", clientfd);

        // receive the request from the client.
        char request[MAX_REQUEST_LEN] = {0};
        if (recv_request(clientfd, request, sizeof(request) - 1) == -1) {
            fprintf(stderr, "[server] : invalid request format, closing conn...\n");
            response_error(clientfd, "Invalid Request Header", "HTTP/1.1", HTTP_BAD_REQUEST);
            close(clientfd);
            continue;
        }

        printf("%s\n", request);
        // print the request line after parsing the request;
        request_line line = {0};
        parse_request_line(request, &line);
        if (line.path[0] == '\0' || line.method[0] == '\0' || line.version[0] == '\0') {
            fprintf(stderr, "[server] : failed to parse the request header...\n");
            response_error(clientfd, "Invalid Request Header", "HTTP/1.1", HTTP_BAD_REQUEST);
            close(clientfd);
            continue;
        }

        // if the method is not equal to get, then put a response_error;
        if (strcmp(line.method, "GET") != 0) {
            response_error(clientfd, "The request method is not supported, try with GET request...", line.version, HTTP_METHOD_NOT_ALLOWED);
            close(clientfd);
            continue;
        }

        // if the version is not equal to HTTP/1.1, then put a response_error;
        if (strcmp(line.version, "HTTP/1.1") != 0) {
            response_error(clientfd, "Error : HTTP Version Not Supported, the server only supports HTTP/1.1", "HTTP/1.1", HTTP_VERSION_NOT_SUPPORTED);
            close(clientfd);
            continue;
        }

        // if the path does not start with /, put a response error.
        if (strncmp(&line.path[0], "/", 1) != 0) {
            response_error(clientfd, "Error : Invalid Filepath, the server requires path to start with /", line.version, HTTP_BAD_REQUEST);
            close(clientfd);
            continue;
        }

        // if the path requests for a file starting with ., put a forbidden response.
        if (strncmp(&line.path[1], ".", 1) == 0) {
            response_error(clientfd, "Error : Accessing Hidden Files is Forbidden...", line.version, HTTP_FORBIDDEN);
            close(clientfd);
            continue;
        }

        // this is a temporary fix for the path traversal cve that is mentioned in the cyber risk 1 below : only if CVE_IS_FIXED is true;
        // run the server from a server directory, and serve from the www directory.
        if (CVE_IS_FIXED) {
            if (strstr(line.path, "..") != NULL) {
                response_error(clientfd, "Error : Path traversal support will not be implemented...", line.version, HTTP_NOT_IMPLEMENTED);
                close(clientfd);
                continue;
            }
        }
        // check and normalise the file paths.
        if (strcmp(line.path, "/") == 0) {
            strcpy(line.path, "/index.html");
        }

        int path_length = snprintf(NULL, 0, "./www%s", line.path);
        char filepath[(size_t)path_length + 1];
        snprintf(filepath, sizeof(filepath), "./www%s", line.path);

        // check if the file exits.
        if (!file_exists(filepath) || !file_is_regular(filepath)) {
            fprintf(stderr, "file path not found : %s\n", filepath);
            response_error(clientfd, "Error : The requested file path is not found, provide a valid path.", line.version, HTTP_NOT_FOUND);
            close(clientfd);
            continue;
        }

        // determine the size of the requested file;
        off_t file_size = get_size(filepath);

        if (file_size < 0 || (uintmax_t)file_size > SIZE_MAX) {
            fprintf(stderr, "invalid file size : %s\n", filepath);
            response_error(clientfd, "Error : Bad File Request", line.version, HTTP_INTERNAL_SERVER_ERROR);
            close(clientfd);
            continue;
        }

        size_t filesize = (size_t)file_size;

        //--------------------------------------------- read the actual file to serve.
        // allocate a buffer for the raw file contents;
        char *body = malloc(filesize == 0 ? 1 : filesize);

        if (body == NULL) {
            perror("malloc failed");
            response_error(clientfd, "Error : Internal Server Error, server could not process the request", line.version, HTTP_INTERNAL_SERVER_ERROR);
            close(clientfd);
            continue;
        }

        // here you go cve, path traversal with arbitrary read-----------------------------------------------------------------[cyber risk 1]
        // since we are not currently doing any URL encoding, the '.' cannot be disguised as %2e in base 64; that will return file not found error;
        if (read_file(filepath, body, filesize) == -1) {
            fprintf(stderr, "error while reading the file : %s\n", filepath);
            response_error(clientfd, "Error : Internal Server Error, server could not process the request", line.version, HTTP_INTERNAL_SERVER_ERROR);
            free(body);
            close(clientfd);
            continue;
        }

        // determine the MIME type of the requested file;
        const char *mime_type = get_mime_type(filepath);
        char format[] =
            "%s %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: %s\r\n"
            "\r\n";

        int header_length = snprintf(NULL, 0, format, "HTTP/1.1", HTTP_OK, "OK", mime_type, filesize, "close");

        if (header_length < 0) {
            fprintf(stderr, "snprintf failed\n");
            free(body);
            close(clientfd);
            continue;
        }

        char response[(size_t)header_length + 1];
        snprintf(response, sizeof(response), format, "HTTP/1.1", HTTP_OK, "OK", mime_type, filesize, "close");

        // first send the HTTP headers;
        if (send_response(clientfd, response, (size_t)header_length) == -1) {
            free(body);
            close(clientfd);
            continue;
        }

        // then send the raw file bytes as the response body;
        if (send_response(clientfd, body, filesize) == -1) {
            fprintf(stderr, "failed to send response body\n");
        }

        // free the body and close the connection.
        free(body);
        close(clientfd);
    }
}

//====================================================================== read_into_buffer
int read_into_buffer(int filefd, char *buffer, size_t size) {
    size_t got = 0;

    while (got < size) {
        size_t count = size - got;

        if (count > MAX_REQUEST_LEN)
            count = MAX_REQUEST_LEN;

        ssize_t nbytes = read(filefd, buffer + got, count);

        if (nbytes == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read failed");
            return -1;
        }

        if (nbytes == 0) {
            fprintf(stderr, "unexpected EOF while reading file...\n");
            return -1;
        }

        got += (size_t)nbytes;
    }

    return EXIT_SUCCESS;
}

//======================================================================== panic message
void panic(int sockfd, const char *msg) {
    perror(msg);
    close(sockfd);
    exit(EXIT_FAILURE);
}

//======================================================================== build_socket : all pre accept setup.
void build_socket(int *sockfd) {
    // create socket.
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set the socket options.
    int opt = 1;
    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        panic(*sockfd, "setsockopt failed");
    }

    // fill in the structs;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(ADDR);

    // bind the socket to struct;
    if (bind(*sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        panic(*sockfd, "bind failed");
    }

    // listen to the socket for connections.
    if (listen(*sockfd, BACKLOG) == -1) {
        panic(*sockfd, "listen failed");
    }
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

//============================================================================================ response_error
int response_error(int clientfd, const char *msg, char *version, http_status status) {
    char response_header_format[] =
        "%s %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n";

    static const http_response_code response_array[] = {
        {"OK", HTTP_OK},
        {"Bad Request", HTTP_BAD_REQUEST},
        {"Unauthorized", HTTP_UNAUTHORIZED},
        {"Forbidden", HTTP_FORBIDDEN},
        {"Not Found", HTTP_NOT_FOUND},
        {"Method Not Allowed", HTTP_METHOD_NOT_ALLOWED},
        {"Request Timeout", HTTP_REQUEST_TIMEOUT},
        {"Internal Server Error", HTTP_INTERNAL_SERVER_ERROR},
        {"Not Implemented", HTTP_NOT_IMPLEMENTED},
        {"Service Unavailable", HTTP_SERVICE_UNAVAILABLE},
        {"HTTP Version Not Supported", HTTP_VERSION_NOT_SUPPORTED},
    };

    char *status_msg = "unknown";
    size_t elements = sizeof(response_array) / sizeof(response_array[0]);
    for (size_t i = 0; i < elements; i++) {
        if (response_array[i].status == status) {
            status_msg = response_array[i].status_msg;
            break;
        }
    }

    size_t content_length = strlen(msg);
    size_t header_length = snprintf(NULL, 0, response_header_format, version, status, status_msg, content_length);
    char *response_header = malloc(header_length + 1);
    snprintf(response_header, header_length + 1, response_header_format, version, status, status_msg, content_length);
    response_header[header_length] = '\0';

    // send response header;
    if (send_response(clientfd, response_header, header_length) == -1) {
        free(response_header);
        return -1;
    }
    free(response_header);

    // send response content
    if (send_response(clientfd, msg, content_length) == -1) {
        return -1;
    }
    return EXIT_SUCCESS;
}

//============================================================================= parse_request_line
void parse_request_line(char *request, request_line *line) {
    if (sscanf(request, "%15s %255s %15s", line->method, line->path, line->version) != 3) {
        memset(line->path, 0, sizeof(line->path));
        memset(line->method, 0, sizeof(line->method));
        memset(line->version, 0, sizeof(line->version));
    }
}

//========================================================================= read_file
int read_file(const char *path, char *buffer, size_t size) {
    int filefd = open(path, O_RDONLY);

    if (filefd == -1) {
        perror("open failed");
        return -1;
    }

    if (read_into_buffer(filefd, buffer, size) == -1) {
        fprintf(stderr, "failed to read into buffer...\n");
        close(filefd);
        return -1;
    }

    close(filefd);
    return EXIT_SUCCESS;
}

//======================================================================== file_exists
bool file_exists(const char *path) {
    struct stat st_buf = {0};

    if (stat(path, &st_buf) == -1) {
        perror("stat failed");
        return false;
    }

    return true;
}

//====================================================================== file_is_regular
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

//===================================================================== get_size
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

//=================================================================== get_mime_type
const char *get_mime_type(char *filepath) {
    // search the string from reverse;
    char *ptr = strrchr(filepath, '.');
    // check if . is found.
    if (ptr == NULL) {
        return "application/octet-stream";
    }
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;

    typedef const struct {
        const char *ext;
        const char *mimetype;
    } mime;
    mime mime_table[] = {
        // web and text files;
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "text/javascript"},
        {".mjs", "text/javascript"},
        {".txt", "text/plain"},
        {".csv", "text/csv"},
        {".md", "text/markdown"},
        {".xml", "application/xml"},
        {".json", "application/json"},
        {".jsonld", "application/ld+json"},
        {".webmanifest", "application/manifest+json"},
        {".xhtml", "application/xhtml+xml"},

        // images;
        {".png", "image/png"},
        {".apng", "image/apng"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".svg", "image/svg+xml"},
        {".ico", "image/vnd.microsoft.icon"},
        {".bmp", "image/bmp"},
        {".avif", "image/avif"},
        {".tif", "image/tiff"},
        {".tiff", "image/tiff"},

        // audio;
        {".mp3", "audio/mpeg"},
        {".wav", "audio/wav"},
        {".ogg", "audio/ogg"},
        {".oga", "audio/ogg"},
        {".opus", "audio/opus"},
        {".aac", "audio/aac"},
        {".m4a", "audio/mp4"},
        {".flac", "audio/flac"},
        {".mid", "audio/midi"},
        {".midi", "audio/midi"},

        // video;
        {".mp4", "video/mp4"},
        {".m4v", "video/mp4"},
        {".webm", "video/webm"},
        {".ogv", "video/ogg"},
        {".mpeg", "video/mpeg"},
        {".mpg", "video/mpeg"},
        {".avi", "video/x-msvideo"},
        {".mov", "video/quicktime"},
        {".mkv", "video/x-matroska"},

        // fonts;
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".eot", "application/vnd.ms-fontobject"},

        // documents;
        {".pdf", "application/pdf"},
        {".doc", "application/msword"},
        {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".xls", "application/vnd.ms-excel"},
        {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {".ppt", "application/vnd.ms-powerpoint"},
        {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {".epub", "application/epub+zip"},

        // compressed files and archives;
        {".zip", "application/zip"},
        {".gz", "application/gzip"},
        {".tar", "application/x-tar"},
        {".7z", "application/x-7z-compressed"},
        {".rar", "application/vnd.rar"},
        {".bz2", "application/x-bzip2"},
        {".xz", "application/x-xz"},

        // application and binary files;
        {".wasm", "application/wasm"},
        {".bin", "application/octet-stream"},
        {".exe", "application/octet-stream"},
        {".c", "text/plain"},
    };

    size_t elements = sizeof(mime_table) / sizeof(mime_table[0]);
    for (size_t i = 0; i < elements; i++) {
        if (strcmp(ptr, (mime_table[i]).ext) == 0) {
            return mime_table[i].mimetype;
        }
    }
    return "application/octet-stream";
}
