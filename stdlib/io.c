#include "forge/io.h"
#include <stdio.h>

void fr_print_int(int64_t value) {
    printf("%lld", (long long)value);
}

void fr_print_str(const char *value) {
    if (value) fputs(value, stdout);
}

void fr_println(void) {
    putchar('\n');
}

void fr_print(const char *value) {
    if (value) fputs(value, stdout);
}

void fr_eprint(const char *value) {
    if (value) fputs(value, stderr);
}

void fr_eprintln(const char *value) {
    if (value) fputs(value, stderr);
    fputc('\n', stderr);
}
