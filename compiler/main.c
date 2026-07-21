#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "optimize.h"
#include "module_loader.h"
#include "driver.h"
#include "symbols.h"

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
    fprintf(stderr, "Forge %s - AOT native compiler\n", FORGE_VERSION);
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <input.fg> -o <binary>          Compile directly to native executable\n", prog);
    fprintf(stderr, "  %s <input.fg> -o <output.c> --emit-c   Emit C source only\n", prog);
    fprintf(stderr, "  %s --lib <input.fg> -o <lib.a> --header <lib.h>\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --emit-c           Emit C instead of a native binary\n");
    fprintf(stderr, "  --forge-root PATH  Project root (include/, build/lib)\n");
    fprintf(stderr, "  --lib-dir PATH     Directory containing libforge_*.a\n");
    fprintf(stderr, "  -I PATH            Extra include directory (also searches for .fg modules)\n");
    fprintf(stderr, "  -l NAME             Link libforge_NAME.a (repeatable)\n");
    fprintf(stderr, "  --cc PATH          C compiler for native output (default: CC, clang, gcc, or cc)\n");
    fprintf(stderr, "  --check            Parse only; exit 0 on success (for LSP / CI)\n");
    fprintf(stderr, "  --symbols-json     Print document symbols as JSON to stdout\n");
    fprintf(stderr, "  --keep-temp        Keep intermediate object files\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    bool lib_mode = false;
    bool check_only = false;
    bool symbols_json = false;
    const char *input = NULL;
    const char *output = NULL;
    const char *header = NULL;

    ForgeDriverConfig cfg;
    forge_driver_config_init(&cfg);
    forge_driver_detect_paths(&cfg, argv[0]);

    const char *includes[32];
    const char *link_libs[32];
    size_t include_count = 0;
    size_t link_lib_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lib") == 0) {
            lib_mode = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "--header") == 0 && i + 1 < argc) {
            header = argv[++i];
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            cfg.emit_c_only = true;
        } else if (strcmp(argv[i], "--forge-root") == 0 && i + 1 < argc) {
            cfg.forge_root = argv[++i];
            forge_driver_detect_paths(&cfg, argv[0]);
        } else if (strcmp(argv[i], "--lib-dir") == 0 && i + 1 < argc) {
            cfg.lib_dir = argv[++i];
        } else if (strcmp(argv[i], "--cc") == 0 && i + 1 < argc) {
            cfg.cc = argv[++i];
        } else if (strcmp(argv[i], "--check") == 0) {
            check_only = true;
        } else if (strcmp(argv[i], "--symbols-json") == 0) {
            symbols_json = true;
        } else if (strcmp(argv[i], "--keep-temp") == 0) {
            cfg.keep_intermediate = true;
        } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            includes[include_count++] = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            link_libs[link_lib_count++] = argv[++i];
        } else if (argv[i][0] != '-') {
            input = argv[i];
        }
    }

    cfg.extra_includes = includes;
    cfg.extra_include_count = include_count;
    cfg.link_libs = link_libs;
    cfg.link_lib_count = link_lib_count;

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

    ForgeModuleConfig mcfg = {
        .entry_path = input,
        .lib_dir = cfg.lib_dir,
        .include_dirs = includes,
        .include_dir_count = include_count,
    };
    forge_load_modules(&prog, &mcfg);
    optimize_program(&prog);

    if (symbols_json) {
        forge_emit_symbols_json(&prog, stdout);
        program_free(&prog);
        free(src);
        return 0;
    }
    if (check_only) {
        program_free(&prog);
        free(src);
        return 0;
    }

    int rc = 0;
    if (lib_mode) {
        if (!output) {
            fprintf(stderr, "forge: library mode requires -o\n");
            rc = 1;
        } else {
            rc = forge_driver_compile_library(&prog, output, header, &cfg);
        }
    } else {
        rc = forge_driver_compile_program(&prog, output, &cfg);
    }

    program_free(&prog);
    free(src);
    return rc;
}
