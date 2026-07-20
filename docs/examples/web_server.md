# web_server

Multi-route HTTP server using `native main` and a top-level `fn` handler. Serves `/`, `/health`, and 404 for other paths.

## Source

`examples/web_server.fg`

## Features

- `native main` — plain C entry point (no process runtime wrapper)
- Top-level `fn handle_request`
- `str_eq` for path routing
- `http_listen` + accept loop

## Code

```forge
import strings;
import http;

fn handle_request(server: int) {
    let req: int = http_accept(server);
    let path: string = http_req_path(req);

    if (str_eq(path, "/")) {
        http_respond(req, 200, "Hello from Forge!");
    } else if (str_eq(path, "/health")) {
        http_respond(req, 200, "ok");
    } else {
        http_respond(req, 404, "Not Found");
    }
    http_close(req);
}

native main {
    let server: int = http_listen(8080);
    println("Forge web server on http://127.0.0.1:8080");

    let running: int = 1;
    while (running == 1) {
        handle_request(server);
    }
}
```

## Run

```bash
cmake --build build --target web_server
./build/bin/web_server
```

In another terminal:

```bash
curl http://127.0.0.1:8080/
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/missing
```

## Expected responses

| Path | Status | Body |
|------|--------|------|
| `/` | 200 | `Hello from Forge!` |
| `/health` | 200 | `ok` |
| other | 404 | `Not Found` |
