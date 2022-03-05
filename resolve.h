#pragma once

#include "xcc.h"

struct FunctionResolution;
struct Typee;
typedef struct FunctionResolution {
    // TODO: return type
    const char *name;
    int num_arguments;
    struct FunctionResolution *next_func_resolution;

    struct Type **argument_types;
    struct Type *return_type;
} FunctionResolution;

struct VariableResolution;
typedef struct VariableResolution {
    const char *name;
    struct Type *var_type;
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

    FunctionResolution *current_func;
} Resolutions;

Resolutions *resolve(AST *program);
void resolve_free(Resolutions *res);