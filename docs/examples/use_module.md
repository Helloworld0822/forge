# use_module

Import another `.fg` source file and call its functions through a module namespace — no separate library build step.

## Source

- `examples/use_module.fg`
- `examples/modules/mathutil.fg`

## Features

- `import "path/to/module.fg"` — source module import
- `module.fn(args)` — qualified calls (`mathutil.add`, etc.)
- Module files contain top-level `fn` definitions only (no `process main`)

## Module code

`examples/modules/mathutil.fg`:

```forge
fn add(a: int, b: int): int {
    return a + b;
}

fn twice(n: int): int {
    return add(n, n);
}

fn square(n: int): int {
    return n * n;
}
```

## Main program

```forge
import "modules/mathutil.fg";

process main {
    println(mathutil.add(10, 32));
    println(mathutil.twice(21));
    println(mathutil.square(8));
}
```

## Run

```bash
cmake --build build --target use_module
./build/bin/use_module
```

## Expected output

```
42
42
64
```

## Search paths

The compiler looks for modules relative to the entry file directory and any `-I` paths:

```bash
forge examples/use_module.fg -o app -I examples
```

You can also write `import mathutil;` when `mathutil.fg` is on the search path.
