# device_io

Interactive stdin/stdout/stderr I/O and low-level file-descriptor writes.

## Source

`examples/device_io.fg`

## Features

- `import io`, `import strings`
- `print`, `println`, `flush`, `read_line`, `prompt`
- `eprint`, `flush_err`, `write_fd`, `stdout_fd`

## Code

```forge
import io;
import strings;

process main {
    print("Enter your name: ");
    flush();
    let name: string = read_line();
    print_str(str_concat("Hello, ", name));
    println();

    let echoed: string = prompt("Say something: ");
    print_str(echoed);
    println();

    eprint("logged to stderr\n");
    flush_err();

    let n: int = write_fd(stdout_fd(), "written via fd\n");
    print_int(n);
    println();
}
```

## Run

```bash
cmake --build build --target device_io
./build/bin/device_io
```

## Expected behavior

Prompts for your name and a line of input, echoes both to stdout, logs a line to stderr, then prints the byte count from `write_fd`. Output depends on what you type.
