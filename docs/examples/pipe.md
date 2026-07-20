# pipe

Elixir-style pipe operator: pass the left-hand value as the first argument to the function on the right.

## Source

`examples/pipe.fg`

## Features

- `value |> fn()` — pipe operator
- User-defined `fn` helpers

## Code

```forge
fn double_it(x: int): int {
    return x * 2;
}

fn add_ten(x: int): int {
    return x + 10;
}

process main {
    let nums: int = 5;
    let result: int = nums |> double_it() |> add_ten();
    println("pipe result:", result);
}
```

`nums |> double_it() |> add_ten()` is equivalent to `add_ten(double_it(nums))`.

## Run

```bash
cmake --build build --target pipe
./build/bin/pipe
```

## Expected output

```
pipe result: 20
```
