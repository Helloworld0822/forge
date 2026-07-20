# coroutines

Lightweight coroutines inside a process: define with `coroutine`, start with `spawn`, suspend with `yield`.

## Source

`examples/coroutines.fg`

## Features

- `coroutine name(params) { ... }`
- `spawn name(args)`
- `yield` — suspend until scheduler resumes

## Code

```forge
process main {
    coroutine worker(id: int) {
        println("worker start", id);
        yield;
        println("worker done", id);
    }

    spawn worker(1);
    spawn worker(2);
    spawn worker(3);
}
```

## Run

```bash
cmake --build build --target coroutines
./build/bin/coroutines
```

## Expected output

```
worker start 1
worker start 2
worker start 3
worker done 1
worker done 2
worker done 3
```

Print order of the `done` lines may vary depending on scheduling.
