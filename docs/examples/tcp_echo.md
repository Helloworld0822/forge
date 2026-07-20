# tcp_echo

TCP server that accepts one client, prints the received message, echoes it back, and closes.

## Source

`examples/tcp_echo.fg`

## Features

- `import tcp`
- `tcp_listen`, `tcp_accept`, `tcp_recv`, `tcp_send`, `tcp_close`

## Code

```forge
import io;
import tcp;

process main {
    let server: int = tcp_listen(9000);
    println("tcp echo server on port 9000");

    let client: int = tcp_accept(server);
    let msg: string = tcp_recv(client);
    println("received:", msg);

    tcp_send(client, msg);
    tcp_close(client);
    tcp_close(server);
}
```

## Run

```bash
cmake --build build --target tcp_echo
./build/bin/tcp_echo &
echo "hello tcp" | nc 127.0.0.1 9000
```

## Expected behavior

- Listens on port **9000**
- Prints `received: hello tcp` (or whatever you send)
- Echoes the same payload back to the client
