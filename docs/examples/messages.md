# messages

Coroutine-local state persists across a `yield` without sharing memory between coroutines.

## Source

`examples/messages.fg`

## Features

- `coroutine` with mutable `let` bindings
- State survives `yield` within the same coroutine

## Code

```forge
process main {
    coroutine worker(id: int) {
        let step: int = id;
        println("worker", step);
        yield;
        step = step + 10;
        println("done", step);
    }

    spawn worker(1);
    spawn worker(2);
}
```

## Run

```bash
cmake --build build --target messages
./build/bin/messages
```

## Expected output

```
worker 1
worker 2
done 11
done 12
```

Each coroutine keeps its own `step` variable; updating it after `yield` does not affect siblings.
