# mathutil (source module)

Reusable integer math helpers intended to be imported by other Forge programs.

## Source

`examples/modules/mathutil.fg`

## Features

- Top-level `fn` exports
- Internal calls between module functions (`twice` calls `add`)

## Code

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

## Usage

Import from a program in the same tree:

```forge
import "modules/mathutil.fg";

process main {
    println(mathutil.add(1, 2));
}
```

See [use_module](use_module.md) for a full walkthrough.

## Notes

- Module files must not contain `process`, `library`, or `native` blocks.
- Functions are compiled into the same binary as the importer with `frmod_<module>_<fn>` symbols.
