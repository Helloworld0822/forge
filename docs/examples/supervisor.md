# supervisor

Elixir-style supervisors declare child processes and a restart policy for fault recovery.

## Source

`examples/supervisor.fg`

## Features

- `supervisor name { restart: policy; child; }`
- `restart: all` — restart all children on failure
- Nested `process` and `coroutine` definitions

## Code

```forge
process worker_proc {
    coroutine task(n: int) {
        println("task", n);
        yield;
        println("task done", n);
    }

    spawn task(1);
    spawn task(2);
}

supervisor app {
    restart: all;
    worker_proc;
}

process main {
    println("supervisor demo");
}
```

## Run

```bash
cmake --build build --target supervisor
./build/bin/supervisor
```

## Expected output

```
supervisor demo
task 1
task 2
task done 1
task done 2
```
