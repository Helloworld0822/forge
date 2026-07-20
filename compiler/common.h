#ifndef HYLO_COMMON_H
#define HYLO_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define HYLO_VERSION "0.1.0"

typedef struct HyloStr {
    char *data;
    size_t len;
} HyloStr;

static inline HyloStr hylo_str(const char *s) {
    HyloStr r = { (char *)s, s ? strlen(s) : 0 };
    return r;
}

static inline bool hylo_str_eq(HyloStr a, HyloStr b) {
    return a.len == b.len && strncmp(a.data, b.data, a.len) == 0;
}

static inline char *hylo_strdup(HyloStr s) {
    char *out = (char *)malloc(s.len + 1);
    if (!out) return NULL;
    memcpy(out, s.data, s.len);
    out[s.len] = '\0';
    return out;
}

[[noreturn]] static inline void hylo_die(const char *msg) {
    fprintf(stderr, "hylo: fatal: %s\n", msg);
    exit(1);
}

#endif
