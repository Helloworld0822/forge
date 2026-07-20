# use_library

Call functions from prebuilt static Forge libraries (`library` / `export` / `import`).

## Source

- `examples/use_library.fg`
- `libs/greeting/greeting.fg`
- `libs/mathutil/mathutil.fg`

## Features

- `import greeting` / `import mathutil` — link against `libforge_*.a`
- `module.fn(args)` — qualified library calls
- Contrasts with [use_module](use_module.md) (source `.fg` import)

## Code

```forge
import io;
import greeting;
import mathutil;

process main {
    let msg: string = greeting.hello("Forge");
    println(msg);
    println(greeting.shout("library"));

    let a: int = mathutil.add(10, 32);
    println("add:", a);
    println("twice:", mathutil.twice(21));
    println("square:", mathutil.square(8));
}
```

## Build

Libraries are built automatically by CMake:

```bash
cmake --build build --target use_library
./build/bin/use_library
```

Manual library build:

```bash
./build/bin/forge --lib libs/greeting/greeting.fg \
    -o greeting.a --header greeting.h \
    --forge-root . --lib-dir build/lib
```

## Expected output

```
Hello, Forge
Hey library!
add: 42
twice: 42
square: 64
```

## When to use libraries vs source modules

| Approach | Build step | Best for |
|----------|------------|----------|
| Source module (`import "x.fg"`) | None | App-local helpers, fast iteration |
| Static library (`library` + `-l`) | Produces `.a` + `.h` | Shared code across projects, CMake integration |
