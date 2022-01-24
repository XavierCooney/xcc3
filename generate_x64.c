#include "xcc.h"

static void generate_expression(AST *ast) {
    if(ast->type == AST_INTEGER_LITERAL) {
        generate_asm_partial("movq $");
        generate_asm_integer(ast->integer_literal_val);
        generate_asm(", %rax");
    } else {
        xcc_assert_not_reached_msg("unknown expression");
    }
}

static void generate_statement(AST *ast) {
    if(ast->type == AST_RETURN_STMT) {
        xcc_assert(ast->num_nodes == 1);

        AST *expression = ast->nodes[0];
        generate_expression(expression);
        generate_asm("retq");
    } else {
        xcc_assert_not_reached_msg("unknown statement");
    }
}

static void generate_body(AST *ast) {
    xcc_assert(ast->type == AST_BODY);

    for(int i = 0; i < ast->num_nodes; ++i) {
        generate_statement(ast->nodes[i]);
    }
}

static void generate_function(AST *ast) {
    xcc_assert(ast->type == AST_FUNCTION);
    xcc_assert(ast->num_nodes == 3);

    const char *name = ast->identifier_string;

    generate_asm_partial(".global ");
    generate_asm(name);

    generate_asm_no_indent();
    generate_asm_partial(name);
    generate_asm(":");


    AST *body = ast->nodes[2];

    generate_body(body);
}

void generate_x64(AST *ast, const char *filename) {
    xcc_assert(ast->type == AST_PROGRAM);

    generate_asm_no_indent();
    generate_asm_partial("# Generated assembly for ");
    generate_asm_partial(filename);
    generate_asm("");

    generate_asm(".section .text");
    generate_asm(".align 4");

    for(int i = 0; i < ast->num_nodes; ++i) {
        generate_function(ast->nodes[i]);
    }
}