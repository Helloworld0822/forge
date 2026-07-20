# hello

The smallest Forge program: a `process main` entry point that prints text and an integer.

## Source

`examples/hello.fg`

## Features

- `process main` — program entry point
- `println` — print values with a newline
- `let` with type annotations (`int`)

## Code

```forge
process main {
    println("Hello from Forge!");
    let x: int = 40 + 2;
    println(x);
}
```

## Run

```bash
cmake --build build --target hello
./build/bin/hello
```

## Expected output

```
Hello from Forge!
42
```
