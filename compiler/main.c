#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "hylo: cannot open '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) hylo_die("out of memory");
    size_t n = fread(buf, 1, (size_t)sz, f);
  buf[n] = '\0';
    fclose(f);
    *out_len = n;
    return buf;
}

static void usage(const char *prog) {
    fprintf(stderr, "Hylo %s - Hybrid Process + Coroutine language (AOT -> C)\n", HYLO_VERSION);
    fprintf(stderr, "Usage: %s <input.hy> [-o output.c]\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *input = argv[1];
    const char *output = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        }
    }

    size_t len = 0;
    char *src = read_file(input, &len);

    Lexer lx;
    lexer_init(&lx, src, len);
    Program prog = parse_program(&lx);

    FILE *out = stdout;
    if (output) {
        out = fopen(output, "w");
        if (!out) {
            fprintf(stderr, "hylo: cannot write '%s'\n", output);
            free(src);
            return 1;
        }
    }

    codegen_emit(&prog, out, "hylo_runtime.h");
    if (output) fclose(out);

    program_free(&prog);
    free(src);
    return 0;
}
