#include "forge/fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *fr_fs_read(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

int fr_fs_write(const char *path, const char *content) {
    if (!path) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (content) {
        size_t n = strlen(content);
        if (fwrite(content, 1, n, f) != n) { fclose(f); return 0; }
    }
    fclose(f);
    return 1;
}

int fr_fs_exists(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int fr_fs_remove(const char *path) {
    if (!path) return 0;
    return remove(path) == 0;
}
