# control_flow

Conditional branches and all three loop forms, including `continue`, `break`, and compound assignment.

## Source

`examples/control_flow.fg`

## Features

- `if` / `else if` / `else`
- `while` and `for` loops
- `continue` and `break`
- Compound assignment (`+=`)

## Code

```forge
process main {
    let x: int = 5;
    if (x < 3) {
        println(1);
    } else if (x < 10) {
        println(2);
    } else {
        println(3);
    }

    let i: int = 0;
    while (i < 3) {
        println(i);
        i += 1;
    }

    for (let j: int = 0; j < 3; j += 1) {
        println(j);
    }

    let k: int = 0;
    while (k < 10) {
        k += 1;
        if (k == 5) {
            continue;
        }
        if (k > 7) {
            break;
        }
        println(k);
    }
}
```

## Run

```bash
cmake --build build --target control_flow
./build/bin/control_flow
```

## Expected output

```
2
0
1
2
0
1
2
1
2
3
4
6
7
```

The last loop skips `5` (`continue`) and stops after `7` (`break`).
