# stdlib_demo

A tour of commonly used standard modules: strings, math, time, json, and fs.

## Source

`examples/stdlib_demo.fg`

## Features

| Module | Functions used |
|--------|----------------|
| `strings` | `str_concat`, `str_len` |
| `math` | `pow_i`, `clamp_i` |
| `time` | `time_now_ms` |
| `json` | `json_stringify_str`, `json_get_string` |
| `fs` | `fs_write`, `fs_read`, `fs_size`, `fs_remove` |

## Code

```forge
import io;
import strings;
import math;
import time;
import json;
import fs;

process main {
    let greeting: string = str_concat("Hello, ", "Forge!");
    println(greeting);
    println("len:", str_len(greeting));
    println("pow:", pow_i(2, 10));
    println("clamp:", clamp_i(150, 0, 100));

    let now: int = time_now_ms();
    println("time:", now);

    let payload: string = json_stringify_str("lang", "forge");
    println(payload);
    println("json lang:", json_get_string(payload, "lang"));

    fs_write("/tmp/forge_demo.txt", "forge stdlib");
    let content: string = fs_read("/tmp/forge_demo.txt");
    println("fs:", content);
    println("fs size:", fs_size("/tmp/forge_demo.txt"));
    fs_remove("/tmp/forge_demo.txt");
}
```

## Run

```bash
cmake --build build --target stdlib_demo
./build/bin/stdlib_demo
```

## Expected output

```
Hello, Forge!
len: 13
pow: 1024
clamp: 100
time: <milliseconds since epoch>
{"lang":"forge"}
json lang: forge
fs: forge stdlib
fs size: 12
```

The timestamp line changes on every run.
