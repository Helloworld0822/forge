#ifndef HYLO_FS_H
#define HYLO_FS_H

#include <stdint.h>

char *hy_fs_read(const char *path);
int hy_fs_write(const char *path, const char *content);
int hy_fs_exists(const char *path);
int hy_fs_remove(const char *path);

#endif
