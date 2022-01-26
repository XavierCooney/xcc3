#include "xcc.h"

typedef struct {
    int reserved_stack_space;
} GenContext;


static void generate_asm_pos(ValuePosition *pos) {
    if(pos->type == POS_STACK) {
        generate_asm_integer(-pos->stack_offset);
        generate_asm_partial("(%rbp)"); // TODO: omit frame pointer
    } else {
        xcc_assert_not_reached_msg("TODO: generate_asm_pos");
    }
}

static const char *move_value_into_reg_temp(ValuePosition *pos) {
    // TODO: this is super bad
    if(pos->type == POS_STACK) {
        generate_asm_partial("movq ");
        generate_asm_integer(-pos->stack_offset);
        generate_asm_partial("(%rbp), "); // TODO: omit frame pointer
        generate_asm("%r11");
        return "%r11";
    } else {
        xcc_assert_not_reached_msg("TODO: generate_asm_pos");
    }
}

static void generate_move_to_pos(ValuePosition *a, ValuePosition *b) {
    // moves value at a into b
    if(value_pos_is_same(a, b)) return;

    generate_asm_partial("movq ");

    if(a->type != POS_STACK || b->type != POS_STACK) {
        generate_asm_pos(a);
        generate_asm_partial(", ");
        generate_asm_pos(b);
        generate_asm("");
    } else {
        const char *reg_a = move_value_into_reg_temp(a);

        generate_asm_partial("movq ");
        generate_asm_partial(reg_a);
        generate_asm_partial(", ");
        generate_asm_pos(b);
        generate_asm("");
    }
}

static void generate_expression(GenContext *ctx, AST *ast) {
    if(ast->type == AST_INTEGER_LITERAL) {
        generate_asm_partial("movq $");
        generate_asm_integer(ast->integer_literal_val);
        generate_asm_partial(", ");
        generate_asm_pos(ast->pos);
        generate_asm("");
    } else if(ast->type == AST_ADD) {
        xcc_assert(ast->num_nodes == 2);

        generate_expression(ctx, ast->nodes[0]);
        generate_expression(ctx, ast->nodes[1]);

        generate_move_to_pos(ast->nodes[0]->pos, ast->pos);

        if(ast->nodes[1]->pos->type == POS_STACK && ast->pos->type == POS_STACK) {
            const char *reg_b = move_value_into_reg_temp(ast->nodes[1]->pos);
            generate_asm_partial("addq ");
            generate_asm_partial(reg_b);
            generate_asm_partial(", ");
            generate_asm_pos(ast->pos);
            generate_asm("");
        } else {
            generate_asm_partial("addq ");
            generate_asm_pos(ast->nodes[1]->pos);
            generate_asm_partial(", ");
            generate_asm_pos(ast->pos);
            generate_asm("");
        }
    } else if(ast->type == AST_CALL) {
        FunctionResolution *res = ast->function_res;
        xcc_assert(res);

        xcc_assert(res->num_arguments == ast->num_nodes);
        for(int i = 0; i < res->num_arguments; ++i) {
            AST *argument_ast = ast->nodes[i];

            generate_expression(ctx, argument_ast);

            const char *reg_name = NULL;

            if(i == 0) {
                reg_name = "%rdi";
            } else if(i == 1) {
                reg_name = "%rsi";
            } else if(i == 2) {
                reg_name = "%rdx";
            } else if(i == 3) {
                reg_name = "%rcx";
            } else if(i == 4) {
                reg_name = "%r8";
            } else if(i == 5) {
                reg_name = "%r9";
            } else {
                xcc_assert_not_reached_msg("TODO: implement case for more than 6 args");
            }

            xcc_assert(reg_name);

            generate_asm_partial("movq ");
            generate_asm_pos(argument_ast->pos);
            generate_asm_partial(", ");
            generate_asm(reg_name);
        }

        generate_asm_partial("call ");
        generate_asm(res->name);
    } else {
        xcc_assert_not_reached_msg("unknown expression");
    }
}

static void generate_statement(GenContext *ctx, AST *ast) {
    if(ast->type == AST_RETURN_STMT) {
        xcc_assert(ast->num_nodes == 1);

        AST *expression = ast->nodes[0];
        generate_expression(ctx, expression);

        generate_asm_partial("movq ");
        generate_asm_pos(expression->pos);
        generate_asm(", %rax");

        // epilogue
        generate_asm_partial("addq $");
        generate_asm_integer(ctx->reserved_stack_space);
        generate_asm(", %rsp");
        generate_asm("popq %rbp");

        generate_asm("retq");
    } else if(ast->type == AST_STATEMENT_EXPRESSION) {
        xcc_assert(ast->num_nodes == 1);
        generate_expression(ctx, ast->nodes[0]);
    } else {
        xcc_assert_not_reached_msg("unknown statement");
    }
}

static void generate_body(GenContext *ctx, AST *ast) {
    xcc_assert(ast->type == AST_BODY);

    for(int i = 0; i < ast->num_nodes; ++i) {
        generate_statement(ctx, ast->nodes[i]);
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

    int stack_space = body->block_max_stack_depth;
    xcc_assert(stack_space >= 0);

    if(stack_space > 0) {
        // TODO: omit-frame-pointer
        generate_asm("pushq %rbp");
        generate_asm("movq %rsp, %rbp");
        generate_asm_partial("subq $");
        generate_asm_integer(stack_space);
        generate_asm(", %rsp");
    }

    GenContext ctx;
    ctx.reserved_stack_space = stack_space;

    generate_body(&ctx, body);
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
        if(ast->nodes[i]->type == AST_FUNCTION_PROTOTYPE) continue;

        generate_function(ast->nodes[i]);
    }
}