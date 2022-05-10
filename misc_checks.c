#include "xcc.h"

static bool is_lvalue(AST *ast) {
    return ast->type == AST_IDENT_USE;
}

void check_lvalue(AST *ast) {
    if(ast->type == AST_ASSIGN) {
        xcc_assert(ast->num_nodes == 2);
        if(!is_lvalue(ast->nodes[0])) {
            prog_error_ast("expected lvalue to assign to", ast);
        }
    }

    for(int i = 0; i < ast->num_nodes; ++i) {
        check_lvalue(ast->nodes[i]);
    }
}

bool check_for_return(AST *ast) {
    if (ast->type == AST_RETURN_STMT) {
        return true;
    }

    bool has_return = false;

    for (int i = 0; i < ast->num_nodes; i++) {
        has_return = has_return || check_for_return(ast->nodes[i]);
    }

    if (ast->type == AST_FUNCTION_DEFINITION) {
        if (!has_return) {
            prog_error_ast("function doesn't have a return", ast);
        }

        return false;
    }

    return has_return;
}