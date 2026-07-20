#ifndef HYLO_IO_H
#define HYLO_IO_H

#include <stdint.h>

void hy_print_int(int64_t value);
void hy_print_str(const char *value);
void hy_println(void);
void hy_print(const char *value);
void hy_eprint(const char *value);
void hy_eprintln(const char *value);

#endif
