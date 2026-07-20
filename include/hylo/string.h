#ifndef HYLO_STRING_H
#define HYLO_STRING_H

#include <stdint.h>

int64_t hy_str_len(const char *s);
char *hy_str_concat(const char *a, const char *b);
int hy_str_eq(const char *a, const char *b);
char *hy_str_sub(const char *s, int64_t start, int64_t len);
int hy_str_contains(const char *haystack, const char *needle);
char *hy_str_trim(const char *s);

#endif
