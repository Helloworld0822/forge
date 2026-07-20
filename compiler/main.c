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
        fprintf(stderr, "forge: cannot open '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) forge_die("out of memory");
    size_t n = fread(buf, 1, (size_t)sz, f);
    buf[n] = '\0';
    fclose(f);
    *out_len = n;
    return buf;
}

static void usage(const char *prog) {
    fprintf(stderr, "Forge %s - Hybrid Process + Coroutine language (AOT -> C)\n", FORGE_VERSION);
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <input.hy> [-o output.c] [-I include-dir]\n", prog);
    fprintf(stderr, "  %s --lib <input.hy> -o output.c --header output.h\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    bool lib_mode = false;
    const char *input = NULL;
    const char *output = NULL;
    const char *header = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lib") == 0) {
            lib_mode = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "--header") == 0 && i + 1 < argc) {
            header = argv[++i];
        } else if (strcmp(argv[i], "-I") == 0) {
            i++;
        } else if (argv[i][0] != '-') {
            input = argv[i];
        }
    }

    if (!input) {
        usage(argv[0]);
        return 1;
    }
    if (lib_mode && !header) {
        fprintf(stderr, "forge: library mode requires --header\n");
        return 1;
    }

    size_t len = 0;
    char *src = read_file(input, &len);

    Lexer lx;
    lexer_init(&lx, src, len);
    Program prog = parse_program(&lx);

    if (lib_mode) {
        if (!output) {
            fprintf(stderr, "forge: library mode requires -o\n");
            return 1;
        }
        FILE *out_c = fopen(output, "w");
        FILE *out_h = fopen(header, "w");
        if (!out_c || !out_h) {
            fprintf(stderr, "forge: cannot write library output\n");
            return 1;
        }
        codegen_emit_library(&prog, out_c, out_h, "forge_runtime.h");
        fclose(out_c);
        fclose(out_h);
    } else {
        FILE *out = stdout;
        if (output) {
            out = fopen(output, "w");
            if (!out) {
                fprintf(stderr, "forge: cannot write '%s'\n", output);
                free(src);
                return 1;
            }
        }
        codegen_emit(&prog, out, "forge_runtime.h");
        if (output) fclose(out);
    }

    program_free(&prog);
    free(src);
    return 0;
}
