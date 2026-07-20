#!/usr/bin/env python3
"""Minimal HTTP server for Forge vs Python benchmark."""

import socket

HOST = "127.0.0.1"
PORT = 19081
BODY = b"Hello, World"


def main() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((HOST, PORT))
    sock.listen(128)
    print(f"Python benchmark server on port {PORT}", flush=True)

    while True:
        conn, _ = sock.accept()
        try:
            conn.recv(4096)
            header = (
                b"HTTP/1.1 200 OK\r\n"
                b"Content-Length: "
                + str(len(BODY)).encode()
                + b"\r\nConnection: close\r\n\r\n"
            )
            conn.sendall(header + BODY)
        finally:
            conn.close()


if __name__ == "__main__":
    main()
