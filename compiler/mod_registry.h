#ifndef HYLO_MOD_REGISTRY_H
#define HYLO_MOD_REGISTRY_H

#include "common.h"
#include <stddef.h>
#include <stdio.h>

typedef struct {
    const char *hy_name;
    const char *c_name;
} HyloStdFn;

typedef struct {
    HyloStr name;
    const char *header;
    const HyloStdFn *fns;
    size_t fn_count;
} HyloModule;

const HyloModule *hylo_std_module(HyloStr name);
const char *hylo_std_c_name(HyloStr hy_name, HyloStr *imports, size_t import_count);
const char *hylo_std_header(HyloStr module);
int hylo_import_is_stdlib(HyloStr name);
void hylo_lib_mangle(char *out, size_t cap, HyloStr lib, HyloStr fn);

#endif
