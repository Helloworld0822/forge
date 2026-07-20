#include "forge/arena.h"
#include <stdlib.h>
#include <string.h>

struct fr_arena {
    char *buf;
    size_t cap;
    size_t pos;
};

static _Thread_local fr_arena_t *tls_arena = NULL;

fr_arena_t *fr_arena_create(size_t initial_cap) {
    fr_arena_t *a = (fr_arena_t *)calloc(1, sizeof(fr_arena_t));
    if (!a) return NULL;
    if (initial_cap < 4096) initial_cap = 4096;
    a->buf = (char *)malloc(initial_cap);
    if (!a->buf) {
        free(a);
        return NULL;
    }
    a->cap = initial_cap;
    return a;
}

void fr_arena_destroy(fr_arena_t *a) {
    if (!a) return;
    free(a->buf);
    free(a);
}

static int arena_grow(fr_arena_t *a, size_t need) {
    if (a->pos + need <= a->cap) return 0;
    size_t ncap = a->cap * 2;
    while (ncap < a->pos + need) ncap *= 2;
    char *nbuf = (char *)realloc(a->buf, ncap);
    if (!nbuf) return -1;
    a->buf = nbuf;
    a->cap = ncap;
    return 0;
}

void *fr_arena_alloc(fr_arena_t *a, size_t size, size_t align) {
    if (!a || size == 0) return NULL;
    if (align < sizeof(void *)) align = sizeof(void *);
    size_t off = (a->pos + align - 1) & ~(align - 1);
    if (arena_grow(a, (off - a->pos) + size) != 0) return NULL;
    a->pos = off + size;
    return a->buf + off;
}

char *fr_arena_strdup(fr_arena_t *a, const char *s) {
    if (!a || !s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = (char *)fr_arena_alloc(a, n, 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    return out;
}

void fr_arena_reset(fr_arena_t *a) {
    if (a) a->pos = 0;
}

fr_arena_t *fr_arena_tls(void) {
    if (!tls_arena) tls_arena = fr_arena_create(64 * 1024);
    return tls_arena;
}

void fr_arena_tls_reset(void) {
    if (tls_arena) fr_arena_reset(tls_arena);
}
