#include "xcc.h"

typedef struct {
    int reserved_stack_space;
} GenContext;

static const char *reg_type_to_asm_name(RegLoc reg) {
    switch(reg) {
        case REG_RAX: return "%rax";
        case REG_RBX: return "%rbx";
        case REG_RCX: return "%rcx";
        case REG_RDX: return "%rdx";
        case REG_RSP: return "%rsp";
        case REG_RBP: return "%rbp";
        case REG_RSI: return "%rsi";
        case REG_RDI: return "%rdi";
        case REG_R8: return "%r8";
        case REG_R9: return "%r9";
        case REG_R10: return "%r10";
        case REG_R11: return "%r11";
        case REG_R12: return "%r12";
        case REG_R13: return "%r13";
        case REG_R14: return "%r14";
        case REG_R15: return "%r15";
        case REG_LAST: xcc_assert_not_reached();
    }
    xcc_assert_not_reached();
}

static void generate_asm_pos(ValuePosition *pos) {
    if(pos->type == POS_STACK) {
        generate_asm_integer(-pos->stack_offset);
        generate_asm_partial("(%rbp)"); // TODO: omit frame pointer
    } else if(pos->type == POS_REG) {
        generate_asm_partial(reg_type_to_asm_name(pos->register_num));
    } else {
        xcc_assert_not_reached_msg("TODO: generate_asm_pos");
    }
}

static bool val_pos_is_memory(ValuePosition *a) {
    return a->type == POS_STACK;
}

static void move_value_raw(ValuePosition *a, ValuePosition *b) {
    // Moves value at a into b, but at least one must not be in memory
    xcc_assert(!val_pos_is_memory(a) || !val_pos_is_memory(b));

    generate_asm_partial("movq ");
    generate_asm_pos(a);
    generate_asm_partial(", ");
    generate_asm_pos(b);
    generate_asm("");
}

static ValuePosition *move_value_into_temp_reg(ValuePosition *pos) {
    // This is done unconditionally, even if it's not necessary
    ValuePosition *temp_reg = value_pos_reg(REG_R11);

    move_value_raw(pos, temp_reg);
    return temp_reg;
}

static ValuePosition *possibly_move_to_temp(ValuePosition *a, ValuePosition *b) {
    // Returns a if at least one of a and b is not memory, otherwise moves
    // a into a temporary register and returns that
    if(val_pos_is_memory(a) && val_pos_is_memory(b)) {
        return move_value_into_temp_reg(a);
    } else {
        return a;
    }
}

static void generate_move(ValuePosition *a, ValuePosition *b) {
    // Moves value at a into b
    if(value_pos_is_same(a, b)) return;

    a = possibly_move_to_temp(a, b);
    move_value_raw(a, b);
}

static void generate_integer_literal_expression(AST *ast) {
    // TODO: make AST_INTEGER_LITERAL have a literal value pos instead
    generate_asm_partial("movq $");
    generate_asm_integer(ast->integer_literal_val);
    generate_asm_partial(", ");
    generate_asm_pos(ast->pos);
    generate_asm("");
}

static void generate_expression(GenContext *ctx, AST *ast);

static void generate_add_expression(GenContext *ctx, AST *ast) {
    xcc_assert(ast->num_nodes == 2);

    generate_expression(ctx, ast->nodes[0]);
    generate_expression(ctx, ast->nodes[1]);

    ValuePosition *a = ast->nodes[0]->pos;
    ValuePosition *b = ast->nodes[1]->pos;
    ValuePosition *dest = ast->pos;

    generate_move(a, dest); // TODO: be more sophisticated of which expr goes to dest
    b = possibly_move_to_temp(b, dest);

    generate_asm_partial("addq ");
    generate_asm_pos(b);
    generate_asm_partial(", ");
    generate_asm_pos(dest);
    generate_asm("");
}

static void generate_call_expression(GenContext *ctx, AST *ast) {
    FunctionResolution *res = ast->function_res;
    xcc_assert(res);

    xcc_assert(res->num_arguments == ast->num_nodes);
    for(int i = 0; i < res->num_arguments; ++i) {
        AST *argument_ast = ast->nodes[i];
        generate_expression(ctx, argument_ast);

        RegLoc arg_reg = REG_LAST;

        if(i == 0) {
            arg_reg = REG_RDI;
        } else if(i == 1) {
            arg_reg = REG_RSI;
        } else if(i == 2) {
            arg_reg = REG_RDX;
        } else if(i == 3) {
            arg_reg = REG_RCX;
        } else if(i == 4) {
            arg_reg = REG_R8;
        } else if(i == 5) {
            arg_reg = REG_R9;
        } else {
            xcc_assert_not_reached_msg("TODO: implement case for more than 6 args");
        }

        xcc_assert(arg_reg != REG_LAST);
        generate_move(ast->pos, value_pos_reg(arg_reg));
    }

    generate_asm_partial("call ");
    generate_asm(res->name);
}

static void generate_expression(GenContext *ctx, AST *ast) {
    if(ast->type == AST_INTEGER_LITERAL) {
        generate_integer_literal_expression(ast);
    } else if(ast->type == AST_ADD) {
        generate_add_expression(ctx, ast);
    } else if(ast->type == AST_CALL) {
        generate_call_expression(ctx, ast);
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