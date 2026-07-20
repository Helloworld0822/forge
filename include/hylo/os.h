#ifndef HYLO_OS_H
#define HYLO_OS_H

#include <stdint.h>

void hy_os_set_args(int argc, char **argv);
void hy_os_exit(int64_t code);
const char *hy_os_getenv(const char *name);
int64_t hy_os_argc(void);
const char *hy_os_argv(int64_t index);

#endif
