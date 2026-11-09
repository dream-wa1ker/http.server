First of all, a pure http GET server is a server that only implements a http get request, and it only supports that method;

- any other method will be rejected, with a error response.
- right now, we will be parsing only the first request line from the client;
- that is 

```text
GET /path HTTP/1.1
```

The first word in the first request line will always be the method, followed by the file path, followed by the http version string.
The remaining request lines are additional http request headers, like Content-Type it wants to get, the Content-Length; The Connection type it is requesting, etc, the User-Agent, and additional information providers.

What we are going to do is : if the method is not GET, put a response to the client, and if it is GET, we are going to 

- Check if the file exists
- Check if the file is readable regular file;
- if not, we return a 404 response : Error file not found.

- If the above conditions are satisfied, we are going to send the response, first we send the response header;
- then we send the content of the response.

A typical response header will have :

```text
HTTP/1.1 200 ok
Content-Type: text/html
Content-Length: 25
Connection: close

Content
```

Any HTTP client will consider whatever is after \r\n\r\n as the content. 
so, our header will be sending till the \r\n, and then we send the content;

We should be able to make these functions

`int send_response(int client_fd, char *response_header, size_t send_size);`


`int recv_request(int client_fd, char *request, size_t recv_size);`

We should have these functions to read the file path and process it;

`int read_into_buffer(int file_fd, char *buffer, size_t size);`

`int read_file(const char *filepath, char *buffer, size_t size); `

`bool file_exists(const char *path);`

`bool is_file_regular(const char *path);`

`off_t get_size(const char *path);`

`void error_response(int client_fd, const char *format, const char *version, const char *content, http_status status);`

Where `http_status` is a enum of the http response codes; 


```c
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

```

For setting up the server, we would need to create a socket, bind the socket and listen at the socket address;
For that we will need to create a function

`void build_socket(int *socket_fd);`

Any error inside this function should be able to terminate the program with a EXIT_FAILURE;

Then we need to create a function that will handle the client, a single client connection; 

`void handle_client(int socket_fd);`

if the accept inside this function fails, the program exits with EXIT_FAILURE;

