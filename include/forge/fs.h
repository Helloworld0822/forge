#ifndef FORGE_FS_H
#define FORGE_FS_H

#include <stdint.h>

char *fr_fs_read(const char *path);
int fr_fs_write(const char *path, const char *content);
int fr_fs_exists(const char *path);
int fr_fs_remove(const char *path);

#endif
