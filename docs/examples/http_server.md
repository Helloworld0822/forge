# http_server

Minimal HTTP server that accepts a single request, logs method and path, and responds with a fixed body.

## Source

`examples/http_server.fg`

## Features

- `import http`
- `http_listen`, `http_accept`, `http_req_method`, `http_req_path`
- `http_respond`, `http_close`, `http_server_close`

## Code

```forge
import io;
import http;

process main {
    let server: int = http_listen(8080);
    println("http server on port 8080");

    let req: int = http_accept(server);
    let path: string = http_req_path(req);
    let method: string = http_req_method(req);
    println(method, path);

    let body: string = "Hello from Forge HTTP server";
    http_respond(req, 200, body);
    http_close(req);
    http_server_close(server);
}
```

## Run

```bash
cmake --build build --target http_server
./build/bin/http_server &
curl http://127.0.0.1:8080/
```

## Expected behavior

- Listens on port **8080**
- Handles **one** request then exits
- Responds with `Hello from Forge HTTP server`
