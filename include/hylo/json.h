#ifndef HYLO_JSON_H
#define HYLO_JSON_H

#include <stdint.h>

const char *hy_json_get_string(const char *json, const char *key);
int64_t hy_json_get_int(const char *json, const char *key);
char *hy_json_stringify_str(const char *key, const char *value);
char *hy_json_stringify_int(const char *key, int64_t value);

#endif
