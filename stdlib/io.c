#include "hylo/io.h"
#include <stdio.h>

void hy_print_int(int64_t value) {
    printf("%lld", (long long)value);
}

void hy_print_str(const char *value) {
    if (value) fputs(value, stdout);
}

void hy_println(void) {
    putchar('\n');
}

void hy_print(const char *value) {
    if (value) fputs(value, stdout);
}

void hy_eprint(const char *value) {
    if (value) fputs(value, stderr);
}

void hy_eprintln(const char *value) {
    if (value) fputs(value, stderr);
    fputc('\n', stderr);
}
