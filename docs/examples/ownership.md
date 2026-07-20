# ownership

Heap-owned strings inside a coroutine using `own let`. Ownership prevents use-after-move at compile time.

## Source

`examples/ownership.fg`

## Features

- `own let` — heap-allocated string owned by the coroutine frame
- `yield` with owned state preserved across suspension

## Code

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

## Run

```bash
cmake --build build --target ownership
./build/bin/ownership
```

## Expected output

```
owned by coroutine
```

## Related

See `docs/first.md` §6 for move semantics (`move(x)`, `send proc, tag, move(msg)`).
