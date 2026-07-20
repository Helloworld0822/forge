#ifndef FORGE_MOD_REGISTRY_H
#define FORGE_MOD_REGISTRY_H

#include "common.h"
#include <stddef.h>
#include <stdio.h>

typedef struct {
    const char *fr_name;
    const char *c_name;
} ForgeStdFn;

typedef struct {
    ForgeStr name;
    const char *header;
    const ForgeStdFn *fns;
    size_t fn_count;
} ForgeModule;

const ForgeModule *forge_std_module(ForgeStr name);
const char *forge_std_c_name(ForgeStr fr_name, ForgeStr *imports, size_t import_count);
const char *forge_std_header(ForgeStr module);
int forge_import_is_stdlib(ForgeStr name);
void forge_lib_mangle(char *out, size_t cap, ForgeStr lib, ForgeStr fn);

#endif
