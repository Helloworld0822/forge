#include "hylo/string.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int64_t hy_str_len(const char *s) {
    return s ? (int64_t)strlen(s) : 0;
}

char *hy_str_concat(const char *a, const char *b) {
    size_t la = a ? strlen(a) : 0;
    size_t lb = b ? strlen(b) : 0;
    char *out = (char *)malloc(la + lb + 1);
    if (!out) return NULL;
    if (la) memcpy(out, a, la);
    if (lb) memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return out;
}

int hy_str_eq(const char *a, const char *b) {
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

char *hy_str_sub(const char *s, int64_t start, int64_t len) {
    if (!s || start < 0 || len < 0) return NULL;
    size_t slen = strlen(s);
    if ((size_t)start >= slen) {
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    if ((size_t)(start + len) > slen) len = (int64_t)(slen - (size_t)start);
    char *out = (char *)malloc((size_t)len + 1);
    if (!out) return NULL;
    memcpy(out, s + start, (size_t)len);
    out[len] = '\0';
    return out;
}

int hy_str_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    return strstr(haystack, needle) != NULL;
}

char *hy_str_trim(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) {
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    size_t len = (size_t)(end - s + 1);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
