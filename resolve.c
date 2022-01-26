// Name resolution
#include "xcc.h"

static FunctionResolution *append_func_resolution(
    Resolutions *res, const char *name, AST *param_ast
) {
    xcc_assert(param_ast->type == AST_FUNC_DECL_PARAM_LIST);
    FunctionResolution *new_res;
    LIST_STRUCT_APPEND_FUNC(
        FunctionResolution, res, num_func_resolutions, num_func_resolutions_allocated,
        func_resolutions, new_res
    );
    new_res->name = name;
    new_res->param_list = param_ast;
    return new_res;
}

static void resolve_func_declaration(AST *ast, Resolutions *res) {
    const char *name = ast->identifier_string;
    // Check if it's been seen previously
    for(int i = 0; i < res->num_func_resolutions; ++i) {
        FunctionResolution *old_res = &res->func_resolutions[i];
        if(strcmp(old_res->name, name)) continue;

        // TODO: redefinition
        prog_error_ast("function redefinition", ast);
    }

    FunctionResolution *new_res = append_func_resolution(res, name, ast->nodes[1]);
    new_res->num_arguments = ast->nodes[1]->num_nodes;
    ast->function_res = new_res;
}

static void resolve_function_call(AST *ast, Resolutions *resolutions) {
    const char *name = ast->identifier_string;
    for(int i = 0; i < resolutions->num_func_resolutions; ++i) {
        FunctionResolution *func_res = &resolutions->func_resolutions[i];
        if(strcmp(func_res->name, name)) continue;

        if(func_res->num_arguments != ast->num_nodes) {
            // TODO: more detailed error
            prog_error_ast("mismatch in argument number", ast);
        }

        ast->function_res = &resolutions->func_resolutions[i];

        return;
    }

    prog_error_ast("Function name not found", ast);
}

static void resolve_recursive(AST *ast, Resolutions *res) {
    if(ast->type == AST_FUNCTION_PROTOTYPE || ast->type == AST_FUNCTION) {
        resolve_func_declaration(ast, res);
    }

    if(ast->type == AST_CALL) {
        resolve_function_call(ast, res);
    }

    for(int i = 0; i < ast->num_nodes; ++i) {
        resolve_recursive(ast->nodes[i], res);
    }
}

Resolutions *resolve(AST *program) {
    Resolutions *res = xcc_malloc(sizeof(Resolutions));

    res->num_func_resolutions = 0;
    res->num_func_resolutions_allocated = 0;
    res->func_resolutions = NULL;

    resolve_recursive(program, res);

    return res;
}

void resolve_free(Resolutions *res) {
    if(res->func_resolutions) xcc_free(res->func_resolutions);

    xcc_free(res);
}
