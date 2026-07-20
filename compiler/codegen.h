#ifndef HYLO_CODEGEN_H
#define HYLO_CODEGEN_H

#include "ast.h"
#include <stdio.h>

void codegen_emit(Program *prog, FILE *out, const char *runtime_include);

#endif
