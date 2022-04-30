#include "xcc.h"

typedef struct {
    int reserved_stack_space;
} GenContext;

static const char *reg_type_to_asm_name_8(RegLoc reg) {
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

static const char *reg_type_to_asm_name_4(RegLoc reg) {
    switch(reg) {
        case REG_RAX: return "%eax";
        case REG_RBX: return "%ebx";
        case REG_RCX: return "%ecx";
        case REG_RDX: return "%edx";
        case REG_RSP: return "%esp";
        case REG_RBP: return "%ebp";
        case REG_RSI: return "%esi";
        case REG_RDI: return "%edi";
        case REG_R8: return "%r8d";
        case REG_R9: return "%r9d";
        case REG_R10: return "%r10d";
        case REG_R11: return "%r11d";
        case REG_R12: return "%r12d";
        case REG_R13: return "%r13d";
        case REG_R14: return "%r14d";
        case REG_R15: return "%r15d";
        case REG_LAST: xcc_assert_not_reached();
    }
    xcc_assert_not_reached();
}

static const char *reg_type_to_asm_name_1(RegLoc reg) {
    switch(reg) {
        case REG_RAX: return "%al";
        case REG_RBX: return "%bl";
        case REG_RCX: return "%cl";
        case REG_RDX: return "%dl";
        case REG_RSP: return "%spl";
        case REG_RBP: return "%bpl";
        case REG_RSI: return "%sil";
        case REG_RDI: return "%dil";
        case REG_R8: return "%r8b";
        case REG_R9: return "%r9b";
        case REG_R10: return "%r10b";
        case REG_R11: return "%r11b";
        case REG_R12: return "%r12b";
        case REG_R13: return "%r13b";
        case REG_R14: return "%r14b";
        case REG_R15: return "%r15b";
        case REG_LAST: xcc_assert_not_reached();
    }
    xcc_assert_not_reached();
}

static void generate_size_suffix(int size) {
    if (size == 1) {
        generate_asm_partial("b");
    } else if (size == 2) {
        generate_asm_partial("w");
    } else if (size == 4) {
        generate_asm_partial("l");
    } else if (size == 8) {
        generate_asm_partial("q");
    } else {
        xcc_assert_not_reached_msg("invalid suffix size");
    }
}

static void generate_asm_pos(ValuePosition *pos) {
    if(pos->type == POS_STACK) {
        generate_asm_integer(-pos->stack_offset);
        generate_asm_partial("(%rbp)"); // TODO: omit frame pointer
    } else if(pos->type == POS_REG) {
        const char *reg_name = NULL;

        if (pos->size == 8) {
            reg_name = reg_type_to_asm_name_8(pos->register_num);
        } else if (pos->size == 4) {
            reg_name = reg_type_to_asm_name_4(pos->register_num);
        } else if (pos->size == 1) {
            reg_name = reg_type_to_asm_name_1(pos->register_num);
        } else {
            // TODO: two-byte registers
            xcc_assert_not_reached();
        }

        generate_asm_partial(reg_name);
    } else {
        xcc_assert_not_reached_msg("TODO: generate_asm_pos");
    }
}

static bool val_pos_is_memory(ValuePosition *a) {
    return a->type == POS_STACK;
}

static bool val_pos_is_writable(ValuePosition *a) {
    // NOTE: could still be a temporary, so isn't necessarily
    // an lvalue

    return a->type == POS_STACK || a->type == POS_REG;
}

static void move_value_raw(ValuePosition *a, ValuePosition *b) {
    xcc_assert(val_pos_is_writable(b));

    // Moves value at a into b, but at least one must not be in memory
    xcc_assert(!val_pos_is_memory(a) || !val_pos_is_memory(b));

    xcc_assert(a->size == b->size);

    generate_asm_partial("mov");
    generate_size_suffix(a->size);
    generate_asm_partial(" ");

    generate_asm_pos(a);
    generate_asm_partial(", ");
    generate_asm_pos(b);
    generate_asm("");
}

static ValuePosition *move_value_into_temp_reg(ValuePosition *pos) {
    // This is done unconditionally, even if it's not necessary
    ValuePosition *temp_reg = value_pos_reg(REG_R11, pos->size);

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

static void generate_move(ValuePosition *from, ValuePosition *to) {
    xcc_assert(val_pos_is_writable(to));

    // Moves value at a into b
    if(value_pos_is_same(from, to)) return;

    from = possibly_move_to_temp(from, to);
    move_value_raw(from, to);
}

static void generate_integer_literal_expression(AST *ast) {
    // TODO: make AST_INTEGER_LITERAL have a literal value pos instead
    if (ast->pos->size == 4) {
        generate_asm_partial("movl $");
    } else if (ast->pos->size == 8) {
        generate_asm_partial("movq $");
    } else {
        xcc_assert_not_reached();
    }

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

    ValuePosition *first_arg;
    ValuePosition *second_arg;

    if(value_pos_is_same(a, dest)) {
        first_arg = a;
        second_arg = b;
    } else if(value_pos_is_same(b, dest)) {
        first_arg = b;
        second_arg = a;
    } else {
        first_arg = NULL;
        second_arg = NULL;
    }

    if(first_arg && second_arg) {
        generate_move(first_arg, dest);
        second_arg = possibly_move_to_temp(second_arg, dest);

        generate_asm_partial("add");
        generate_size_suffix(ast->pos->size);
        generate_asm_partial(" ");

        generate_asm_partial(" ");
        generate_asm_pos(second_arg);
        generate_asm_partial(", ");
        generate_asm_pos(dest);
        generate_asm("");
    } else {
        ValuePosition *temp_reg_a = move_value_into_temp_reg(a);

        generate_asm_partial("add");
        generate_size_suffix(ast->pos->size);
        generate_asm_partial(" ");

        generate_asm_partial(" ");
        generate_asm_pos(b);
        generate_asm_partial(", ");
        generate_asm_pos(temp_reg_a);
        generate_asm("");

        generate_move(temp_reg_a, dest);
    }
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
        generate_move(argument_ast->pos, value_pos_reg(arg_reg, argument_ast->pos->size));
    }

    generate_asm_partial("call ");
    generate_asm(res->name);
}

static void generate_int_conversion(GenContext *ctx, AST *ast) {
    xcc_assert(ast->type == AST_CONVERT_TO_INT);
    xcc_assert(ast->num_nodes == 1);
    generate_expression(ctx, ast->nodes[0]);

    ValuePosition *from = ast->nodes[0]->pos;
    ValuePosition *to = ast->pos;

    int size_from = from->size;
    int size_to = to->size;

    xcc_assert(size_from != size_to);
    xcc_assert_msg(from->is_signed && to->is_signed, "TODO: handle unsigned conversion");

    if (size_from < size_to) {
        // TODO: use CWD/CDQ/whatever instructions?
        // Also TODO: this is highly inefficient

        ValuePosition *from_reg = NULL;
        // ValuePosition *to_reg = NULL;

        if (val_pos_is_memory(from)) {
            from_reg = move_value_into_temp_reg(from);
        } else {
            from_reg = from;
        }

        generate_asm_partial("movsx");
        generate_size_suffix(size_from);
        // generate_size_suffix(size_to);

        generate_asm_partial(" ");
        generate_asm_pos(from_reg);
        generate_asm_partial(", ");

        ValuePosition *to_reg = NULL;

        if (val_pos_is_memory(to)) {
            to_reg = value_pos_reg(REG_R11, to->size);
            generate_asm_pos(to_reg);
        } else {
            generate_asm_pos(to);
        }
        generate_asm("");

        if (to_reg != NULL) {
            generate_move(to_reg, to);
        }
    } else {
        // I *think* no conversion is necessary because of the way
        // 2's complement works...
    }
}

static void generate_expression(GenContext *ctx, AST *ast) {
    if (ast->type == AST_INTEGER_LITERAL) {
        generate_integer_literal_expression(ast);
    } else if (ast->type == AST_ADD) {
        generate_add_expression(ctx, ast);
    } else if (ast->type == AST_CALL) {
        generate_call_expression(ctx, ast);
    } else if (ast->type == AST_VAR_USE) {
        // hopefully the value at value_pos has the variable value already...
        // so we do nothing here
    } else if (ast->type == AST_ASSIGN) {
        xcc_assert(ast->num_nodes == 2);

        generate_expression(ctx, ast->nodes[0]);
        generate_expression(ctx, ast->nodes[1]);

        ValuePosition *from = ast->nodes[1]->pos;
        ValuePosition *to = ast->nodes[0]->pos;
        ValuePosition *dest = ast->pos;

        generate_move(from, to);

        if(value_pos_is_same(from, dest) || value_pos_is_same(to, dest)) {
            // do nothing
        } else {
            generate_move(from, dest); // from is less likely to be a memory position
        }
    } else if (ast->type == AST_CONVERT_TO_INT) {
        generate_int_conversion(ctx, ast);
    } else {
        xcc_assert_not_reached_msg("unknown expression");
    }
}

static void generate_statement(GenContext *ctx, AST *ast) {
    if(ast->type == AST_RETURN_STMT) {
        xcc_assert(ast->num_nodes == 1);

        AST *expression = ast->nodes[0];
        generate_expression(ctx, expression);

        generate_move(expression->pos, value_pos_reg(REG_RAX, expression->pos->size));

        // epilogue
        generate_asm_partial("addq $");
        generate_asm_integer(ctx->reserved_stack_space);
        generate_asm(", %rsp");
        generate_asm("popq %rbp");

        generate_asm("retq");
    } else if(ast->type == AST_STATEMENT_EXPRESSION) {
        xcc_assert(ast->num_nodes == 1);
        generate_expression(ctx, ast->nodes[0]);
        // TODO: have a value pos for discarding
    } else if(ast->type == AST_VAR_DECLARE) {
        xcc_assert(ast->num_nodes == 2);
        generate_expression(ctx, ast->nodes[1]);
        generate_move(ast->nodes[1]->pos, ast->pos);
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