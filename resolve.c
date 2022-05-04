// Name resolution
#include "xcc.h"

static FunctionResolution *append_func_resolution(
    Resolutions *res, const char *name, AST *param_ast
) {
    xcc_assert(param_ast->type == AST_FUNC_DECL_PARAM_LIST);

    FunctionResolution *new_res = xcc_malloc(sizeof(FunctionResolution));
    new_res->name = name;
    new_res->next_func_resolution = NULL;
    new_res->argument_types = NULL;
    new_res->return_type = NULL;

    if(!res->func_resolutions_head) {
        res->func_resolutions_head = new_res;
    }

    if(res->func_resolutions_tail) {
        res->func_resolutions_tail->next_func_resolution = new_res;
    }
    res->func_resolutions_tail = new_res;

    return new_res;
}

static VariableResolution *append_var_resolution_all(
    Resolutions *res, const char *name
) {
    VariableResolution *new_res = xcc_malloc(sizeof(VariableResolution));
    new_res->name = name;
    new_res->next_variable_resolution = NULL;
    new_res->stack_offset = -1;
    new_res->var_type = NULL;

    if(!res->all_var_resolutions_head) {
        res->all_var_resolutions_head = new_res;
    }

    if(res->all_var_resolutions_tail) {
        res->all_var_resolutions_tail->next_variable_resolution = new_res;
    }
    res->all_var_resolutions_tail = new_res;

    return new_res;
}

static void append_var_resolution_local(
    Resolutions *res, VariableResolution *new_var_res
) {
    VariableResolution **new_var_resolution_slot;
    LIST_STRUCT_APPEND_FUNC(
        VariableResolution*, res, num_local_var_resolutions,
        num_local_var_resolutions_allocated,
        local_var_resolutions, new_var_resolution_slot
    );
    *new_var_resolution_slot = new_var_res;
}

static void resolve_func_declaration(AST *ast, Resolutions *res) {
    const char *name = ast->identifier_string;
    // Check if it's been seen previously
    for (FunctionResolution *old_res = res->func_resolutions_head;
            old_res; old_res = old_res->next_func_resolution) {
        if (strcmp(old_res->name, name)) continue;

        // TODO: redefinition
        prog_error_ast("function redefinition", ast);
    }

    FunctionResolution *new_res = append_func_resolution(res, name, ast->nodes[1]);
    new_res->num_arguments = ast->nodes[1]->num_nodes;
    ast->function_res = new_res;

    res->current_func = new_res;
}

static void resolve_function_call(AST *ast, Resolutions *resolutions) {
    const char *name = ast->identifier_string;
    for(FunctionResolution *func_res = resolutions->func_resolutions_head; func_res;
            func_res = func_res->next_func_resolution) {
        if(strcmp(func_res->name, name)) continue;

        if(func_res->num_arguments != ast->num_nodes) {
            // TODO: more detailed error
            prog_error_ast("mismatch in argument number", ast);
        }

        ast->function_res = func_res;

        return;
    }

    prog_error_ast("Function name not found", ast);
}

static void resolve_var_declare(AST *ast, Resolutions *resolutions) {
    xcc_assert(ast->type == AST_VAR_DECLARE);

    const char *name = ast->identifier_string;

    // TODO: have an identifier_in_use(Resolutions *resolutions) function
    for(int i = 0; i < resolutions->num_local_var_resolutions; ++i) {
        if(!strcmp(resolutions->local_var_resolutions[i]->name, name)) {
            prog_error_ast("Redeclaration/shadowing of variable", ast);
        }
    }

    VariableResolution *new_var_res = append_var_resolution_all(resolutions, name);
    new_var_res->stack_offset = -1;
    append_var_resolution_local(resolutions, new_var_res);

    ast->var_res = new_var_res;
}

static void resolve_var_usage(AST *ast, Resolutions *resolutions) {
    const char *name = ast->identifier_string;

    for(int i = 0; i < resolutions->num_local_var_resolutions; ++i) {
        if(!strcmp(resolutions->local_var_resolutions[i]->name, name)) {
            ast->var_res = resolutions->local_var_resolutions[i];
            return;
        }
    }

    prog_error_ast("variable not found", ast);
}

static void resolve_parameter(AST *ast, Resolutions *resolutions) {
    xcc_assert(ast->type == AST_PARAMETER);

    const char *name = ast->identifier_string;

    VariableResolution *new_var_res = append_var_resolution_all(resolutions, name);
    new_var_res->stack_offset = -1;
    append_var_resolution_local(resolutions, new_var_res);

    ast->var_res = new_var_res;
}

static void resolve_recursive(AST *ast, Resolutions *res) {
    if (ast->type == AST_FUNCTION_PROTOTYPE || ast->type == AST_FUNCTION) {
        resolve_func_declaration(ast, res);
    }

    if (ast->type == AST_CALL) {
        resolve_function_call(ast, res);
    }

    if (ast->type == AST_VAR_DECLARE) {
        resolve_var_declare(ast, res);
    }

    if (ast->type == AST_VAR_USE) {
        resolve_var_usage(ast, res);
    }

    if (ast->type == AST_RETURN_STMT) {
        xcc_assert(res->current_func);
        ast->function_res = res->current_func;
    }

    if (ast->type == AST_FUNCTION) {
        // this could also just be moved to the AST_FUNC_DECL_PARAM_LIST param stuff.
        xcc_assert(ast->nodes[1]->type == AST_FUNC_DECL_PARAM_LIST);
        for (int i = 0; i < ast->nodes[1]->num_nodes; ++i) {
            resolve_parameter(ast->nodes[1]->nodes[i], res);
        }
    }

    for (int i = 0; i < ast->num_nodes; ++i) {
        resolve_recursive(ast->nodes[i], res);
    }

    if(ast->type == AST_FUNCTION_PROTOTYPE || ast->type == AST_FUNCTION) {
        res->current_func = NULL;
        res->num_local_var_resolutions = 0;
    }
}

Resolutions *resolve(AST *program) {
    Resolutions *res = xcc_malloc(sizeof(Resolutions));

    res->func_resolutions_head = NULL;
    res->func_resolutions_tail = NULL;

    res->all_var_resolutions_head = NULL;
    res->all_var_resolutions_tail = NULL;

    res->num_local_var_resolutions = 0;
    res->num_local_var_resolutions_allocated = 0;
    res->local_var_resolutions = NULL;

    res->current_func = NULL;

    resolve_recursive(program, res);

    return res;
}

void resolve_free(Resolutions *res) {
    xcc_free(res->local_var_resolutions);

    FunctionResolution *func_res = res->func_resolutions_head;
    while(func_res) {
        FunctionResolution *next_func_res = func_res->next_func_resolution;
        if (func_res->argument_types) {
            xcc_free(func_res->argument_types);
        }
        xcc_free(func_res);
        func_res = next_func_res;
    }

    VariableResolution *var_res = res->all_var_resolutions_head;
    while(var_res) {
        VariableResolution *next_var_res = var_res->next_variable_resolution;
        xcc_free(var_res);
        var_res = next_var_res;
    }

    xcc_free(res);
}
