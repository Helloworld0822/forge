# Forge

A **Hybrid Lightweight Process + Coroutine** language — an AOT-compiled language that combines Elixir/Erlang-style lightweight processes with coroutines.

Forge source (`.hy`) is compiled to C and then built into native binaries. It targets a predictable execution model without a garbage collector.

## Features

- **Light Process** — unit for state ownership, isolation, and fault recovery (`process`)
- **Coroutine** — lightweight execution flows inside a process (`coroutine`, `spawn`, `yield`)
- **AOT compilation** — `.hy` → `.c` → native binary
- **Standard modules** — I/O, strings, math, files, TCP/UDP, HTTP, JSON
- **User libraries** — build static libraries with `library` / `export` / `import`
- **Supervisor** — Elixir-style fault-recovery policies

## Requirements

- GCC or Clang (C11)
- CMake 3.16+
- Linux (networking modules use POSIX sockets)

## Build

```bash
git clone https://github.com/Helloworld0822/forge.git
cd forge

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
./build/bin/hello
./build/bin/coroutines
./build/bin/stdlib_demo
./build/bin/use_library
./build/bin/http_server   # curl http://127.0.0.1:8080
./build/bin/tcp_echo      # port 9000
```

## Language Example

```forge
process main {
    coroutine worker(id: int) {
        println("start", id);
        yield;
        println("done", id);
    }

    spawn worker(1);
    spawn worker(2);
}
```

## Standard Modules

Import modules with `import`. Standard modules are included in `libforge_std`.

| Module | Description |
|--------|-------------|
| `io` | `print`, `eprint`, `eprintln` |
| `strings` | `str_len`, `str_concat`, `str_eq`, … |
| `math` | `abs_i`, `min_i`, `max_i`, `pow_i`, … |
| `time` | `time_now_ms`, `sleep_ms` |
| `fs` | `fs_read`, `fs_write`, `fs_exists` |
| `os` | `os_exit`, `os_getenv`, `os_argc`, `os_argv` |
| `tcp` | `tcp_listen`, `tcp_connect`, `tcp_send`, `tcp_recv` |
| `udp` | `udp_bind`, `udp_send`, `udp_recv` |
| `http` | `http_get`, `http_post`, `http_listen`, … |
| `json` | `json_get_string`, `json_get_int`, … |

```forge
import io;
import tcp;

process main {
    let server: int = tcp_listen(8080);
    println("listening");
}
```

## User Libraries

### Define a Library

```forge
library greeting {
    import strings;

    export fn hello(name: string): string {
        return str_concat("Hello, ", name);
    }
}
```

### Compile

```bash
./build/bin/forge --lib libs/greeting/greeting.hy \
    -o greeting.c --header greeting.h
```

### Use

```forge
import greeting;

process main {
    println(greeting.hello("Forge"));
}
```

With CMake, use `forge_add_library()` from `cmake/ForgeLibrary.cmake`.

## Project Structure

```
forge/
├── compiler/       # Forge compiler (lexer, parser, codegen)
├── runtime/        # processes, coroutines, scheduler
├── stdlib/         # standard module C implementations
├── include/        # runtime and stdlib headers
├── libs/           # sample user libraries
├── examples/       # example programs
├── cmake/          # CMake helpers
└── docs/           # design documents
```

## Compiler Usage

```bash
# Generate C for an executable
forge app.hy -o app.c

# Generate C + header for a library
forge --lib lib.hy -o lib.c --header lib.h
```

## Contributing

Issues and pull requests are welcome.

1. Fork the repository and create a branch
2. Commit your changes
3. Open a pull request

## License

[MIT License](LICENSE) — free to use, modify, and distribute.

## Design Docs

For the execution model and design goals, see [docs/first.md](docs/first.md).
