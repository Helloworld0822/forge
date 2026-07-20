#ifndef FORGE_STRING_H
#define FORGE_STRING_H

#include <stdint.h>

int64_t fr_str_len(const char *s);
char *fr_str_concat(const char *a, const char *b);
int fr_str_eq(const char *a, const char *b);
char *fr_str_sub(const char *s, int64_t start, int64_t len);
int fr_str_contains(const char *haystack, const char *needle);
char *fr_str_trim(const char *s);

#endif
