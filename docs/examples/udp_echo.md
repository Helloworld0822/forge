# udp_echo

UDP server that receives one datagram, logs the sender, and replies with `pong`.

## Source

`examples/udp_echo.fg`

## Features

- `import udp`
- `udp_bind`, `udp_recv`, `udp_peer`, `udp_send`, `udp_close`

## Code

```forge
import io;
import udp;

process main {
    let sock: int = udp_bind(9001);
    println("udp server on port 9001");

    let msg: string = udp_recv(sock);
    let peer: string = udp_peer(sock);
    println("from", peer, ":", msg);

    udp_send(sock, "127.0.0.1", 9001, "pong");
    udp_close(sock);
}
```

## Run

```bash
cmake --build build --target udp_echo
./build/bin/udp_echo &
echo "ping" | nc -u 127.0.0.1 9001
```

## Expected behavior

- Binds port **9001**
- Prints sender address and message
- Sends `pong` back to `127.0.0.1:9001`
