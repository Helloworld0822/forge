#include "hylo_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hy_print_int(int64_t value) {
    printf("%lld", (long long)value);
}

void hy_print_str(const char *value) {
    fputs(value, stdout);
}

void hy_println(void) {
    putchar('\n');
}
