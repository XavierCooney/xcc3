#pragma once

#include "xcc.h"

struct FunctionResolution;
typedef struct FunctionResolution {
    // TODO: return type
    const char *name;
    AST *param_list;
    int num_arguments;
    struct FunctionResolution *next_func_resolution;
} FunctionResolution;

struct VariableResolution;
typedef struct VariableResolution {
    // TODO: variable type
    const char *name;
    int stack_offset;
    struct VariableResolution *next_variable_resolution;
} VariableResolution;

typedef struct {
    FunctionResolution *func_resolutions_head;
    FunctionResolution *func_resolutions_tail;

    VariableResolution *all_var_resolutions_head;
    VariableResolution *all_var_resolutions_tail;

    int num_local_var_resolutions;
    int num_local_var_resolutions_allocated;
    VariableResolution **local_var_resolutions;
} Resolutions;

Resolutions *resolve(AST *program);
void resolve_free(Resolutions *res);