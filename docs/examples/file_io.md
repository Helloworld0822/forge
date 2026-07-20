# file_io

Filesystem operations through the `fs` standard module.

## Source

`examples/file_io.fg`

## Features

- `import fs`
- `fs_write`, `fs_append`, `fs_read`, `fs_exists`, `fs_size`
- `fs_mkdir`, `fs_is_file`, `fs_is_dir`, `fs_list_dir`
- `fs_copy`, `fs_rename`, `fs_remove`

## Code

```forge
import fs;

process main {
    let path: string = "/tmp/forge_file_io.txt";
    let dir: string = "/tmp/forge_file_io_dir";

    fs_write(path, "hello");
    fs_append(path, " forge");
    println(fs_exists(path));
    println(fs_is_file(path));
    println(fs_size(path));
    println(fs_read(path));

    fs_mkdir(dir);
    println(fs_is_dir(dir));
    println(fs_list_dir("/tmp/forge_file_io_dir"));

    let copy_path: string = "/tmp/forge_file_io_copy.txt";
    fs_copy(path, copy_path);
    println(fs_read(copy_path));

    let renamed: string = "/tmp/forge_file_io_renamed.txt";
    fs_rename(copy_path, renamed);
    println(fs_read(renamed));

    fs_remove(renamed);
    fs_remove(path);
    fs_remove(dir);
    println(1);
}
```

## Run

```bash
cmake --build build --target file_io
./build/bin/file_io
```

## Expected output

```
1
1
11
hello forge
1
(empty or directory listing)
hello forge
hello forge
1
```

Writes and removes files under `/tmp/`.
