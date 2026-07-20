#ifndef FORGE_IO_H
#define FORGE_IO_H

#include <stdint.h>

void fr_print_int(int64_t value);
void fr_print_str(const char *value);
void fr_println(void);
void fr_print(const char *value);
void fr_eprint(const char *value);
void fr_eprintln(const char *value);

#endif
