# functions

Top-level function definitions with parameters, return types, recursion, and calls from `process main`.

## Source

`examples/functions.fg`

## Features

- `fn name(params): return_type { ... }`
- `return` statements
- Recursion (`factorial`)
- `import strings` and `str_concat`

## Code

```forge
import strings;

fn add(a: int, b: int): int {
    return a + b;
}

fn greet(name: string): string {
    return str_concat("Hello, ", name);
}

fn factorial(n: int): int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

process main {
    let x: int = add(10, 32);
    println(x);

    let msg: string = greet("Forge");
    println(msg);

    let f: int = factorial(6);
    println(f);
}
```

## Run

```bash
cmake --build build --target functions
./build/bin/functions
```

## Expected output

```
42
Hello, Forge
720
```

## Notes

Functions are compiled to static C helpers in the same translation unit. They can be called from `process main`, `coroutine` bodies, and other top-level functions.
