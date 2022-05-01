#include "xcc.h"

bool check_for_return(AST *ast) {
    if (ast->type == AST_RETURN_STMT) {
        return true;
    }

    bool has_return = false;

    for (int i = 0; i < ast->num_nodes; i++) {
        has_return = has_return || check_for_return(ast->nodes[i]);
    }

    if (ast->type == AST_FUNCTION) {
        if (!has_return) {
            prog_error_ast("function doesn't have a return", ast);
        }

        return false;
    }

    return has_return;
}