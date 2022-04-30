// checks that ASSIGN expression have an lvalue to the left
#include "xcc.h"

static bool is_lvalue(AST *ast) {
    return ast->type == AST_VAR_USE;
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
