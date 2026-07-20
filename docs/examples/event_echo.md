# event_echo

Async TCP echo using `await` on sockets inside a coroutine. The runtime registers the fd with epoll and resumes the coroutine when data is ready.

## Source

`examples/event_echo.fg`

## Features

- `await fd` — yield until a file descriptor is readable
- `coroutine` + `spawn` for non-blocking I/O
- `import tcp`

## Code

```forge
import tcp;

process echo {
    coroutine once(port: int) {
        let sock: int = tcp_listen(port);
        println("event echo on port", port);
        await sock;
        let client: int = tcp_accept(sock);
        await client;
        let data: string = tcp_recv(client);
        tcp_send(client, data);
        tcp_close(client);
        println("served one connection");
    }

    spawn once(19090);
}
```

## Run

```bash
cmake --build build --target event_echo
./build/bin/event_echo &
echo "hello" | nc 127.0.0.1 19090
```

## Expected behavior

- Server listens on port **19090**
- Accepts one connection, echoes the payload, prints `served one connection`
- Requires `nc` (netcat) or similar TCP client
