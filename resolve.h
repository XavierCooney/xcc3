#pragma once

#include "xcc.h"

typedef struct FunctionResolution {
    // TODO: return type
    const char *name;
    AST *param_list;
    int num_arguments;
} FunctionResolution;

typedef struct {
    int num_func_resolutions;
    int num_func_resolutions_allocated;
    FunctionResolution *func_resolutions;
} Resolutions;

Resolutions *resolve(AST *program);
void resolve_free(Resolutions *res);