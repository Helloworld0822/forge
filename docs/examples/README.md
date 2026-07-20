# Forge Examples

Runnable tutorials for the example programs under `examples/`. Each page links to the `.fg` source, explains the language features used, and shows how to build and run the binary.

## Basics

| Example | Description |
|---------|-------------|
| [hello](hello.md) | Minimal entry point and `println` |
| [control_flow](control_flow.md) | `if` / `while` / `for`, `break`, `continue`, `+=` |
| [functions](functions.md) | Top-level `fn`, recursion, `return` |
| [match](match.md) | `const` and integer `match` |
| [pipe](pipe.md) | Pipe operator `\|>` |

## Concurrency

| Example | Description |
|---------|-------------|
| [coroutines](coroutines.md) | `coroutine`, `spawn`, `yield` |
| [messages](messages.md) | Coroutine-local mutable state |
| [supervisor](supervisor.md) | Supervisor restart policies |
| [ownership](ownership.md) | `own let` and move semantics |
| [event_echo](event_echo.md) | `await` with epoll TCP echo |

## I/O and Standard Library

| Example | Description |
|---------|-------------|
| [file_io](file_io.md) | Filesystem module (`fs`) |
| [device_io](device_io.md) | stdin/stdout/stderr and file descriptors |
| [stdlib_demo](stdlib_demo.md) | strings, math, time, json, fs |

## Modules and Libraries

| Example | Description |
|---------|-------------|
| [use_module](use_module.md) | Import a `.fg` source module |
| [mathutil](modules/mathutil.md) | Reusable source module |
| [use_library](use_library.md) | Prebuilt static libraries |

## Networking

| Example | Description |
|---------|-------------|
| [http_server](http_server.md) | Single-request HTTP server |
| [web_server](web_server.md) | Multi-route HTTP server |
| [tcp_echo](tcp_echo.md) | TCP echo server |
| [udp_echo](udp_echo.md) | UDP request/response |

## Quick build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run any example after building:

```bash
./build/bin/hello
```
