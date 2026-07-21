# Forge

> **Project home moved:** Development continues under the [forge-language](https://github.com/forge-language) organization:
> [forge](https://github.com/forge-language/forge) · [language-server](https://github.com/forge-language/language-server) · [vscode-extension](https://github.com/forge-language/vscode-extension)
>
> This repository is the original monorepo and may lag behind the split repositories.

A **Hybrid Lightweight Process + Coroutine** language — an AOT-compiled language that combines Elixir/Erlang-style lightweight processes with coroutines.

Forge source (`.fg`) is compiled directly to native binaries. The compiler streams generated code to `clang` in memory — no `.c` files are written unless you pass `--emit-c`.

## Features

- **Direct native compilation** — `forge app.fg -o app` produces an executable in one step
- **Light Process** — unit for state ownership, isolation, and fault recovery (`process`)
- **Coroutine** — lightweight execution flows inside a process (`coroutine`, `spawn`, `yield`)
- **AOT compilation** — `.fg` → native binary (C emitted only with `--emit-c`)
- **Functions** — top-level `fn` with recursion, forward declarations, and return types
- **Native programs** — `native main` for plain C entry points (bootstrap compiler)
- **Optimizer** — constant folding and algebraic simplification
- **Standard modules** — I/O, strings, math, files, TCP/UDP, HTTP, JSON
- **User libraries** — build static libraries with `library` / `export` / `import`
- **M:N scheduler** — multi-threaded worker pool with work-stealing queues (`fr_scheduler_create(0)` = auto CPU count)
- **Event loop** — epoll (Linux), kqueue (macOS), or select (Windows); `await fd` in coroutines yields until readable
- **Pipe operator** — `value |> fn()` passes value as the first argument (Elixir-style)
- **Pattern matching** — `match expr { pat => stmt, _ => default }` on integers
- **Comptime constants** — `const NAME = expr` folded at compile time
- **Arena allocator** — bump allocation for HTTP request bodies (per-request reset)
- **Preemptive scheduling** — 2000-reduction budget per coroutine (BEAM-style)
- **Ownership** — `own let` for heap strings, `move(x)` and `send proc, tag, move(msg)` for move semantics
- **Supervisor** — Elixir-style fault-recovery policies

## Requirements

- GCC or Clang (C11)
- CMake 3.16+
- **Linux** — epoll event loop (recommended for production I/O)
- **macOS** — kqueue event loop
- **Windows** — MSVC or MinGW; select-based event loop; Winsock networking

Supported platforms: Linux, macOS, Windows 10+.

- **Self-hosting bootstrap** — `bootstrap/compiler.fg` compiles Forge subset to C; stage2 recompiles itself

## Self-hosting bootstrap

```bash
cmake --build build --target forge-selfhost forge-selfhost-test forge-selfhost-verify
```

Pipeline:

| Stage | Binary | Compiles |
|-------|--------|----------|
| 0 | `build/bin/forge` (C) | `bootstrap/compiler.fg` → `forge-stage1` |
| 1 | `build/bin/forge-stage1` | `compiler.fg` → `forge-stage2.c` |
| 2 | `build/bin/forge-stage2` | self + `examples/match.fg` |

`forge-selfhost-verify` builds stage3 from stage2 output and checks that recompiling `compiler.fg` yields identical C (fixed point).

Verify stage2 compiles itself:

```bash
./build/bin/forge-stage2 bootstrap/compiler.fg -o /tmp/stage3.c
```

## Build

```bash
git clone https://github.com/Helloworld0822/forge.git
cd forge

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (MSYS2 / MinGW)

```powershell
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
./build/bin/hello
./build/bin/coroutines
./build/bin/stdlib_demo
./build/bin/use_library
./build/bin/use_module
./build/bin/http_server   # single-request demo
./build/bin/web_server    # curl http://127.0.0.1:8080
./build/bin/ownership     # own let + move demo
./build/bin/event_echo    # await + epoll TCP echo (port 19090)
./build/bin/pipe          # pipe operator demo
./build/bin/match         # pattern matching + const demo
```

Step-by-step tutorials for every example live in [docs/examples/](docs/examples/README.md).

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

**Phoenix** (Elixir, see `benchmark/phoenix/run_server.sh`):

```bash
./benchmark/phoenix/run_server.sh   # port 19082 (requires Erlang + Elixir)
curl http://127.0.0.1:19082/
```

**Rust Axum** (see `benchmark/axum/run_server.sh`):

```bash
./benchmark/axum/run_server.sh   # port 19083 (requires Rust/cargo)
curl http://127.0.0.1:19083/
```

## HTTP Benchmark (Forge vs Python vs Phoenix vs Rust Axum)

Both servers return `Hello, World` (13 bytes) with `Connection: close`.  
Benchmark uses **1,000,000 requests** at **1,000 concurrent** connections on Linux.

| Implementation | Port | Requests/sec | Notes |
|----------------|------|--------------|-------|
| **Forge** (AOT native, epoll + REUSEPORT + connection pool) | 19080 | **~12,600** | `benchmark/forge/bench_server.fg` |
| **Rust** (Axum, release) | 19083 | **~8,000** | `benchmark/axum/bench_server` |
| **Python 3** (stdlib socket) | 19081 | **~7,700** | `benchmark/python/server.py` |
| **Phoenix** (Bandit, minimal router) | 19082 | **~7,500** | `benchmark/phoenix/bench_server` |

Measured on: Linux 7.1.2-arch3-1 x86_64 (2026-07-20). Results vary ±5% run-to-run.

Forge exceeds raw Python socket performance in this micro-benchmark.  
The benchmark uses prebuilt responses, per-core epoll workers with `SO_REUSEPORT`, and a fast accept→respond→close path (no per-client epoll registration).

### Why Forge was slower (and what we fixed)

The original Forge benchmark path paid avoidable per-request cost:

| Bottleneck | Impact |
|------------|--------|
| `malloc`/`free` on every request to buffer HTTP headers | Heap churn |
| Two `send()` syscalls (header + body separately) | Extra syscall |
| `recv` loop until `\r\n\r\n` instead of one read | Extra syscalls |
| Per-request `snprintf` + `strlen` to build the response | CPU overhead |
| Path parsing + `str_eq` in the Forge handler loop | Unnecessary work vs Python's fixed 200 |
| `setsockopt(TCP_NODELAY)` on every accepted socket | Extra syscall per connection |

**Fixes applied:**

- Stack-buffered header read (no per-request `malloc`) for the general `http_accept` path
- Single-buffer `http_respond` (one `send` when response fits in 576 bytes)
- `http_prepare` / `http_serve_forever` — single-threaded epoll accept loop
- `http_serve_mt(server, n)` — `2×CPU` REUSEPORT accept workers (`n=0`) + `8×` sharded connection handler threads
- Linux fast path: `accept4(SOCK_NONBLOCK)` then immediate respond/close (no client epoll)
- Worker threads pinned to CPUs; listen backlog 4096 and 64 KiB socket buffers

### Run the benchmark yourself

```bash
cmake --build build --target bench_server
./benchmark/run_benchmark.sh
```

Ensure ports **19080**–**19083** are free before running (`fuser -k 19080/tcp 19081/tcp 19082/tcp 19083/tcp` if a prior run left servers behind).

Results are written to `benchmark/results.txt` (gitignored).

## Runtime: M:N Scheduler + Event Loop

Forge processes compile to a **multi-threaded M:N scheduler**:

```
CPU workers (pthread)
    └── work-stealing run queues
            └── light processes
                    └── coroutines (cooperative, switch-case codegen)
```

- `fr_scheduler_create(0)` — `0` means auto-detect CPU count (used by generated `main`)
- Process mailboxes are mutex-protected; `recv()` blocks on a condition variable
- The scheduler embeds an **epoll** event loop; `await` in coroutines registers the fd and yields

```forge
import tcp;

process echo {
    coroutine once(port: int) {
        let sock: int = tcp_listen(port);
        await sock;                        // yield until connection is ready
        let client: int = tcp_accept(sock);
        await client;
        let data: string = tcp_recv(client);
        tcp_send(client, data);
        tcp_close(client);
    }
    spawn once(19090);
}
```

See `examples/event_echo.fg`.

## Ownership (move semantics)

Process-local data is freely usable. Heap strings can be marked **owned** and **moved** across `send`:

```forge
process main {
    coroutine demo() {
        own let msg: string = "owned by coroutine";
        yield;
        println(msg);
    }
    spawn demo();
}
```

- `own let x: string = ...` — owned heap string (process scope)
- `move(x)` — transfer ownership (`fr_own_take`)
- `send target, tag, move(payload)` — move payload into the mailbox (`fr_send_move`)

The compiler rejects use-after-move. See `examples/ownership.fg` and `docs/first.md` §6.


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
| `io` | `print`, `print_int`, `read_line`, `read_char`, `read_stdin`, `prompt`, `flush`, `write_fd`, `read_fd`, `eprint`, `eprintln` |
| `strings` | `str_len`, `str_concat`, `str_eq`, … |
| `math` | `abs_i`, `min_i`, `max_i`, `pow_i`, … |
| `time` | `time_now_ms`, `sleep_ms` |
| `fs` | `fs_read`, `fs_write`, `fs_append`, `fs_exists`, `fs_remove`, `fs_size`, `fs_is_file`, `fs_is_dir`, `fs_mkdir`, `fs_rename`, `fs_copy`, `fs_list_dir` |
| `os` | `os_exit`, `os_getenv`, `os_argc`, `os_argv` |
| `tcp` | `tcp_listen`, `tcp_connect`, `tcp_send`, `tcp_recv` |
| `udp` | `udp_bind`, `udp_send`, `udp_recv` |
| `http` | `http_get`, `http_listen`, `http_prepare`, `http_serve_mt`, … |
| `event` | `event_poll`, `event_add_read` (epoll integration) |
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

## Language Server (LSP)

Editor support for `.fg` files — syntax highlighting, diagnostics, completion, hover, and document symbols.

See [docs/lsp.md](docs/lsp.md) for setup. Quick start:

```bash
cmake --build build
cd lsp && npm install && npm run build
cd ../editors/vscode && npm install && npm run build
```

Then install the extension from `editors/vscode/` in VS Code or Cursor.

## Project Structure

```
forge/
├── compiler/       # Forge compiler (lexer, parser, codegen)
├── runtime/        # M:N scheduler, epoll event loop, ownership, mailbox
├── stdlib/         # standard module C implementations
├── include/        # runtime and stdlib headers
├── libs/           # sample user libraries
├── examples/       # example programs
├── benchmark/      # Forge vs Python vs Phoenix vs Rust Axum HTTP benchmark
│   ├── forge/      # Forge benchmark server (.fg)
│   ├── python/     # Python benchmark server (.py)
│   ├── phoenix/    # Phoenix (Bandit) benchmark server
│   └── axum/       # Rust Axum benchmark server
├── cmake/          # CMake helpers
├── lsp/            # TypeScript language server
├── editors/vscode/ # VS Code / Cursor extension
└── docs/           # design documents and tutorials
```

## Compiler Usage

```bash
# Compile directly to a native executable
forge app.fg -o app --forge-root . --lib-dir build/lib

# Emit C source only (debugging)
forge app.fg -o app.c --emit-c

# Build a static library (.a + .h)
forge --lib lib.fg -o libforge_mylib.a --header mylib.h \
    --forge-root . --lib-dir build/lib
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
