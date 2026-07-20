# match

Compile-time constants and integer pattern matching with a wildcard fallback.

## Source

`examples/match.fg`

## Features

- `const NAME = value` — folded at compile time
- `match expr { pat => stmt, _ => default }`
- `_` wildcard arm

## Code

```forge
const HTTP_OK = 200;
const HTTP_NOT_FOUND = 404;

process main {
    let code: int = HTTP_OK;
    match code {
        200 => { println("OK"); }
        404 => { println("Not Found"); }
        _ => { println("Other"); }
    }

    let n: int = 7;
    match n {
        1 => { println("one"); }
        7 => { println("lucky seven"); }
        _ => { println("other"); }
    }
}
```

## Run

```bash
cmake --build build --target match
./build/bin/match
```

## Expected output

```
OK
lucky seven
```
