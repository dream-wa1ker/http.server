# http.server

A simple re-implementation of a HTTP GET server.
Migrated from [init87](https://github.com/dream-wa1ker/init87), to this github repo.

### What this does

Currenly this server is not very advanced enough to serve a directory like in 

```bash
python3 -m http.server
```

And is capable of only serving static files, from a serving root directory that is `#defined` in the C file. (`www`);

- setting the `IS_CVE_FIXED` to `false` will enable a path traversal CVE, allowing the client to request file outside of the root directory.

```bash
curl -v --path-as-is 'http://localhost:8080/../../../../../../../../../../../../etc/passwd'
```

- from the server side, you can see that the requested file was served

```text
GET /../../../../../../../etc/passwd HTTP/1.1
Host: localhost:8080
User-Agent: curl/8.23.0
Accept: */*
```

- with the CVE disabled, you will get an error in the client side.

```text
λ curl --path-as-is http://localhost:8080/../../../../../../../etc/passwd

# output
Error : Path traversal support will not be implemented...⏎
```


#### Steps for creation of a connection.

- create a network socket in `AF_INET` domain and the socket should be of type `SOCK_STREAM`
- use the default protocol for that combination - `0`, which is TCP by default.
- set the socket option to reuse the address immediately `SO_REUSEADDR` set to be true;
- fill in the network structures, the server uses `IPv4` internet protocol implementation. 
- listen from all clients in the LAN - `INADDR_ANY`, in port 8080; Now any host in the local network can access this server.
- bind the structure to the socket we just created. 
- start listening for the clients, use a backlog of 10 clients - queued.
- define a function for handling the clients, in which the we set the client option for recv and send timeouts so that a single client will not stall our server if client did not request for anything for 10000ms.
- validates the paths, the request line, the supported methods, and mimetypes, and serves accordingly. 
- deliberately avoids serving any files starting with `'.'`, i.e, the hidden files inside the root directory are not served.
- validate and close the client connection after the request is received | response is send.

>[!NOTE]
> It is the responsibility of the host running the server not to serve any symlinks inside the root directory, that will cause a escape out of the root directory.



### Running the Server

Make sure that you put your index.html inside the root directory (`www`).

```bash
gcc server.c -o server && ./server
```


Note the IP address of the host running the server.

```bash
ip addr show wlan0 | grep "inet" | awk '{printf $2}' | awk -F/ '{printf $1 "\n"}'
```

Browse / curl the server @

```text
http://<server-ip-address>:8080/
```


