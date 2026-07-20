# Forge

A **Hybrid Lightweight Process + Coroutine** language — an AOT-compiled language that combines Elixir/Erlang-style lightweight processes with coroutines.

Forge source (`.fg`) is compiled to C and then built into native binaries. It targets a predictable execution model without a garbage collector.

## Features

- **Light Process** — unit for state ownership, isolation, and fault recovery (`process`)
- **Coroutine** — lightweight execution flows inside a process (`coroutine`, `spawn`, `yield`)
- **AOT compilation** — `.fg` → `.c` → native binary
- **Functions** — top-level `fn` with recursion, forward declarations, and return types
- **Native programs** — `native main` for plain C entry points (bootstrap compiler)
- **Optimizer** — constant folding and algebraic simplification
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
./build/bin/http_server   # single-request demo
./build/bin/web_server    # curl http://127.0.0.1:8080
./build/bin/tcp_echo      # port 9000
```

### Simple Web Servers

**Forge** (`examples/web_server.fg` → `web_server` binary):

```bash
./build/bin/web_server
curl http://127.0.0.1:8080/
curl http://127.0.0.1:8080/health
```

**Python** (for comparison, see `benchmark/python/server.py`):

```bash
python3 benchmark/python/server.py   # port 19081
curl http://127.0.0.1:19081/
```

## HTTP Benchmark (Forge vs Python)

Both servers return `Hello, World` (13 bytes) with `Connection: close`.  
Benchmark uses **2,000 requests** at **50 concurrent** connections on Linux.

| Implementation | Port | Requests/sec | Notes |
|----------------|------|--------------|-------|
| **Forge** (AOT native) | 19080 | **3,916** | `benchmark/forge/bench_server.fg` |
| **Python 3** (stdlib socket) | 19081 | **4,090** | `benchmark/python/server.py` |

Measured on: Linux 7.1.2-arch3-1 x86_64 (2026-07-20).

Forge is within ~4% of raw Python socket performance in this micro-benchmark.  
Python's slight edge comes from the interpreter's mature socket path; Forge pays no GC cost and compiles to native code.

### Run the benchmark yourself

```bash
cmake --build build --target bench_server
./benchmark/run_benchmark.sh
```

Results are written to `benchmark/results.txt` (gitignored).


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
./build/bin/forge --lib libs/greeting/greeting.fg \
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
├── benchmark/      # Forge vs Python HTTP benchmark
│   ├── forge/      # Forge benchmark server (.fg)
│   └── python/     # Python benchmark server (.py)
├── cmake/          # CMake helpers
└── docs/           # design documents
```

## Compiler Usage

```bash
# Generate C for an executable
forge app.fg -o app.c

# Generate C + header for a library
forge --lib lib.fg -o lib.c --header lib.h
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
