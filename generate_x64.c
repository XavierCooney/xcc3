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

static void generate_label(int label_num) {
    xcc_assert(label_num >= 0);

    generate_asm_partial(".L");
    generate_asm_integer(label_num);
    generate_asm_partial("");
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
    ValuePosition *temp_reg = value_pos_reg(REG_R11, pos->size, pos->is_signed);

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
    xcc_assert(from);
    xcc_assert(to);
    xcc_assert(from->size == to->size);
    xcc_assert(from->is_signed == to->is_signed);

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
static void generate_statement(GenContext *ctx, AST *ast);

static void generate_binary_arithmetic_expression(GenContext *ctx, AST *ast) {
    xcc_assert(ast->num_nodes == 2);

    generate_expression(ctx, ast->nodes[0]);
    generate_expression(ctx, ast->nodes[1]);

    ValuePosition *a = ast->nodes[0]->pos;
    ValuePosition *b = ast->nodes[1]->pos;
    ValuePosition *dest = ast->pos;

    ValuePosition *first_arg;
    ValuePosition *second_arg;

    // TODO: swapping args breaks on non-commutative operators, but there's
    // probably a more efficient workaround
    bool is_commutative = ast->type == AST_ADD;

    if(value_pos_is_same(a, dest)) {
        first_arg = a;
        second_arg = b;
    } else if(value_pos_is_same(b, dest) && is_commutative) {
        first_arg = b;
        second_arg = a;
    } else {
        first_arg = NULL;
        second_arg = NULL;
    }

    const char *opcode = NULL;
    if (ast->type == AST_ADD) {
        opcode = "add";
    } else if (ast->type == AST_SUBTRACT) {
        opcode = "sub";
    }

    xcc_assert(opcode);

    if(first_arg && second_arg) {
        generate_move(first_arg, dest);
        second_arg = possibly_move_to_temp(second_arg, dest);

        generate_asm_partial(opcode);
        generate_size_suffix(ast->pos->size);
        generate_asm_partial(" ");

        generate_asm_partial(" ");
        generate_asm_pos(second_arg);
        generate_asm_partial(", ");
        generate_asm_pos(dest);
        generate_asm("");
    } else {
        ValuePosition *temp_reg_a = move_value_into_temp_reg(a);

        generate_asm_partial(opcode);
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

static void generate_multiply_expression(GenContext *ctx, AST *ast) {
    // We need a separate function for this becomes it seems multiplication
    // only works with certain registers?

    xcc_assert(ast->num_nodes == 2);

    generate_expression(ctx, ast->nodes[0]);
    generate_expression(ctx, ast->nodes[1]);

    ValuePosition *a = ast->nodes[0]->pos;
    ValuePosition *b = ast->nodes[1]->pos;
    ValuePosition *dest = ast->pos;

    // TODO: where stuff is eventually properly allocated to registers
    // rather than all on the stack, there should be a way of indicating
    // which instructions will use up RAX and other registers.

    ValuePosition *multiplication_reg = value_pos_reg(REG_RAX, dest->size, dest->is_signed);

    generate_move(a, multiplication_reg);
    generate_asm_partial("imul");
    generate_size_suffix(dest->size);
    generate_asm_partial(" ");
    generate_asm_pos(b);
    generate_asm_partial(", ");
    generate_asm_pos(multiplication_reg);
    generate_asm("");

    generate_move(multiplication_reg, dest);
}

static void generate_comparison_expression(GenContext *ctx, AST *ast) {
    // uses rax!
    RegLoc comparison_register = REG_RAX;

    xcc_assert(ast->num_nodes == 2);

    generate_expression(ctx, ast->nodes[0]);
    generate_expression(ctx, ast->nodes[1]);

    ValuePosition *a = ast->nodes[0]->pos;
    ValuePosition *b = ast->nodes[1]->pos;
    ValuePosition *dest = ast->pos;

    int is_signed = dest->is_signed;

    generate_asm_partial("xorq ");
    generate_asm_partial(" ");
    generate_asm_pos(value_pos_reg(comparison_register, 8, is_signed));
    generate_asm_partial(", ");
    generate_asm_pos(value_pos_reg(comparison_register, 8, is_signed));
    generate_asm("");

    a = possibly_move_to_temp(a, b);
    generate_asm_partial("cmp");
    generate_size_suffix(a->size);
    generate_asm_partial(" ");
    generate_asm_pos(a);
    generate_asm_partial(", ");
    generate_asm_pos(b);
    generate_asm("");

    generate_asm_partial("set");

    if (ast->type == AST_CMP_LT) {
        generate_asm_partial("g");
    } else if (ast->type == AST_CMP_LT_EQ) {
        generate_asm_partial("ge");
    } else if (ast->type == AST_CMP_GT) {
        generate_asm_partial("l");
    } else if (ast->type == AST_CMP_GT_EQ) {
        generate_asm_partial("le");
    } else {
        xcc_assert_not_reached();
    }

    generate_asm_partial(" ");
    generate_asm_pos(value_pos_reg(comparison_register, 1, is_signed));
    generate_asm("");

    generate_move(value_pos_reg(comparison_register, dest->size, is_signed), dest);
}

static RegLoc argument_index_to_register(int index) {
    if(index == 0) {
        return REG_RDI;
    } else if(index == 1) {
        return REG_RSI;
    } else if(index == 2) {
        return REG_RDX;
    } else if(index == 3) {
        return REG_RCX;
    } else if(index == 4) {
        return REG_R8;
    } else if(index == 5) {
        return REG_R9;
    }
    xcc_assert_not_reached_msg("TODO: implement case for more than 6 args");
}

static void generate_call_expression(GenContext *ctx, AST *ast) {
    xcc_assert(ast->num_nodes >= 1);
    xcc_assert(ast->nodes[0]->pos->type == POS_FUNC_NAME);

    for(int i = 1; i < ast->num_nodes; ++i) {
        AST *argument_ast = ast->nodes[i];
        generate_expression(ctx, argument_ast);

        RegLoc arg_reg = argument_index_to_register(i - 1);
        generate_move(argument_ast->pos, value_pos_reg(
            arg_reg, argument_ast->pos->size, argument_ast->pos->is_signed
        ));
    }

    generate_asm_partial("call ");
    generate_asm(ast->nodes[0]->pos->func_name);

    if (ast->pos->type != POS_VOID) {
        generate_move(value_pos_reg(REG_RAX, ast->pos->size, ast->pos->is_signed), ast->pos);
    }
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
            to_reg = value_pos_reg(REG_R11, to->size, to->is_signed);
            generate_asm_pos(to_reg);
        } else {
            generate_asm_pos(to);
        }
        generate_asm("");

        if (to_reg != NULL) {
            generate_move(to_reg, to);
        }
    } else {
        // This is super janky. Also this relies on little-endianess
        ValuePosition truncated_from;
        memcpy(&truncated_from, from, sizeof(ValuePosition));
        truncated_from.size = to->size;

        int is_same_pos_type = truncated_from.type == to->type;

        if (is_same_pos_type && to->type == POS_STACK && truncated_from.stack_offset == to->stack_offset) {
            // same position in stack
        } else if (is_same_pos_type && to->type == POS_REG && truncated_from.register_num == to->register_num) {
            // same register
        } else {
            generate_move(&truncated_from, to);
        }

    }
}

static void generate_dereference(GenContext *ctx, AST *ast) {
    xcc_assert(ast->type == AST_DEREFERENCE);
    xcc_assert(ast->num_nodes == 1);

    generate_expression(ctx, ast->nodes[0]);

    ValuePosition *from = ast->nodes[0]->pos;
    ValuePosition *to = ast->pos;

    if (val_pos_is_memory(from)) {
        from = move_value_into_temp_reg(from);
    }
    xcc_assert(from->type = POS_REG); // TODO: there are other cases to think about

    ValuePosition *to_temp = to;
    if (val_pos_is_memory(to_temp)) {
        to_temp = value_pos_reg(REG_R11, to->size, to->is_signed);;
    }

    generate_asm_partial("mov");
    generate_size_suffix(to->size);
    generate_asm_partial(" ("); // TODO: does this always work?
    generate_asm_pos(from);
    generate_asm_partial("), ");
    generate_asm_pos(to_temp);
    generate_asm("");

    if (!value_pos_is_same(to, to_temp)) {
        move_value_raw(to_temp, to);
    }
}

static void generate_expression(GenContext *ctx, AST *ast) {
    if (ast->type == AST_INTEGER_LITERAL) {
        generate_integer_literal_expression(ast);
    } else if (ast->type == AST_ADD) {
        generate_binary_arithmetic_expression(ctx, ast);
    } else if (ast->type == AST_SUBTRACT) {
        generate_binary_arithmetic_expression(ctx, ast);
    } else if (ast->type == AST_MULTIPLY) {
        generate_multiply_expression(ctx, ast);
    } else if (ast->type == AST_CMP_LT) {
        generate_comparison_expression(ctx, ast);
    } else if (ast->type == AST_CMP_GT) {
        generate_comparison_expression(ctx, ast);
    } else if (ast->type == AST_CMP_LT_EQ) {
        generate_comparison_expression(ctx, ast);
    } else if (ast->type == AST_CMP_GT_EQ) {
        generate_comparison_expression(ctx, ast);
    } else if (ast->type == AST_CALL) {
        generate_call_expression(ctx, ast);
    } else if (ast->type == AST_IDENT_USE) {
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
    } else if (ast->type == AST_DEREFERENCE) {
        generate_dereference(ctx, ast);
    } else {
        xcc_assert_not_reached_msg("unknown expression");
    }
}

static void generate_if(GenContext *ctx, AST *ast) {
    xcc_assert(ast->type == AST_IF);
    xcc_assert(ast->num_nodes == 2 || ast->num_nodes == 3);
    bool has_else = ast->num_nodes == 3;

    generate_expression(ctx, ast->nodes[0]);

    int skip_to_after_if_label = get_unique_label_num();
    int skip_to_after_else = has_else ? get_unique_label_num() : -1;

    ValuePosition *condition_reg = possibly_move_to_temp(
        ast->nodes[0]->pos, ast->nodes[0]->pos
    );

    // TODO: always using a test instruction is very inefficent
    generate_asm_partial("test");
    generate_size_suffix(ast->nodes[0]->pos->size);
    generate_asm_partial(" ");
    generate_asm_pos(condition_reg);
    generate_asm_partial(", ");
    generate_asm_pos(condition_reg);
    generate_asm("");

    generate_asm_partial("jz ");
    generate_label(skip_to_after_if_label);
    generate_asm("");

    generate_statement(ctx, ast->nodes[1]);
    if (has_else) {
        generate_asm_partial("jmp ");
        generate_label(skip_to_after_else);
        generate_asm("");
    }

    generate_label(skip_to_after_if_label);
    generate_asm(":");

    if (has_else) {
        generate_statement(ctx, ast->nodes[2]);
        generate_label(skip_to_after_else);
        generate_asm(":");
    }
}

static void generate_while(GenContext *ctx, AST *ast) {
    xcc_assert(ast->type == AST_WHILE);
    xcc_assert(ast->num_nodes == 2);

    int beginning_label = get_unique_label_num();
    int end_label = get_unique_label_num();

    generate_label(beginning_label);
    generate_asm(":");

    generate_expression(ctx, ast->nodes[0]);
    ValuePosition *condition_reg = possibly_move_to_temp(
        ast->nodes[0]->pos, ast->nodes[0]->pos
    );

    // TODO: always using a test instruction is very inefficent
    generate_asm_partial("test");
    generate_size_suffix(ast->nodes[0]->pos->size);
    generate_asm_partial(" ");
    generate_asm_pos(condition_reg);
    generate_asm_partial(", ");
    generate_asm_pos(condition_reg);
    generate_asm("");

    generate_asm_partial("jz ");
    generate_label(end_label);
    generate_asm("");

    generate_statement(ctx, ast->nodes[1]);

    generate_asm_partial("jmp ");
    generate_label(beginning_label);
    generate_asm("");

    generate_label(end_label);
    generate_asm(":");
}

static void generate_statement(GenContext *ctx, AST *ast) {
    if(ast->type == AST_RETURN_STMT) {
        xcc_assert(ast->num_nodes <= 1);

        if (ast->num_nodes == 1) {
            AST *expression = ast->nodes[0];
            generate_expression(ctx, expression);

            generate_move(expression->pos, value_pos_reg(REG_RAX, expression->pos->size, expression->pos->is_signed));
        }

        // epilogue
        if (ctx->reserved_stack_space != 0) {
            generate_asm_partial("addq $");
            generate_asm_integer(ctx->reserved_stack_space);
            generate_asm(", %rsp");
            generate_asm("popq %rbp");
        }

        generate_asm("retq");
    } else if(ast->type == AST_STATEMENT_EXPRESSION) {
        xcc_assert(ast->num_nodes == 1);
        generate_expression(ctx, ast->nodes[0]);
        // TODO: have a value pos for discarding
    } else if(ast->type == AST_IF) {
        generate_if(ctx, ast);
    } else if(ast->type == AST_WHILE) {
        generate_while(ctx, ast);
    } else if(ast->type == AST_DECLARATOR_GROUP) {
        if (ast->num_nodes == 2) {
            // declaration with initialisation
            generate_expression(ctx, ast->nodes[1]);
            generate_move(ast->nodes[1]->pos, ast->declaration->pos);
        }
    } else if (ast->type == AST_BLOCK_STATEMENT) {
        for (int i = 0; i < ast->num_nodes; ++i) {
            generate_statement(ctx, ast->nodes[i]);
        }
    } else if (ast->type == AST_DECLARATION) {
        for (int i = 1; i < ast->num_nodes; ++i) {
            generate_statement(ctx, ast->nodes[i]);
        }
    } else {
        xcc_assert_not_reached_msg("unknown statement");
    }
}

static void generate_body(GenContext *ctx, AST *ast) {
    xcc_assert(ast->type == AST_BLOCK_STATEMENT);

    for(int i = 0; i < ast->num_nodes; ++i) {
        generate_statement(ctx, ast->nodes[i]);
    }
}

static void generate_param_loading(AST *ast) {
    xcc_assert(ast->type == AST_DECLARATOR_GROUP);
    xcc_assert(ast->num_nodes == 1);
    ast = ast->nodes[0];
    xcc_assert(ast->type == AST_DECLARATOR_FUNC);

    for (int i = 1; i < ast->num_nodes; ++i) {
        AST *param = ast->nodes[i];
        xcc_assert(param->type == AST_PARAMETER);
        xcc_assert(param->declaration->pos);

        RegLoc arg_reg = argument_index_to_register(i - 1);
        generate_move(value_pos_reg(
            arg_reg, param->declaration->pos->size, param->declaration->pos->is_signed
        ), param->declaration->pos);
    }
}

static void generate_function(AST *ast) {
    xcc_assert(ast->type == AST_FUNCTION_DEFINITION);
    xcc_assert(ast->num_nodes == 3);

    const char *name = ast->declaration->name;

    generate_asm_partial(".global ");
    generate_asm(name);

    generate_asm_no_indent();
    generate_asm_partial(name);
    generate_asm(":");

    AST *body = ast->nodes[2];

    int stack_space = body->block_max_stack_depth;
    xcc_assert(stack_space >= 0);

    // prologue
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

    generate_param_loading(ast->nodes[1]);
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
        if(ast->nodes[i]->type == AST_DECLARATION) continue;

        generate_function(ast->nodes[i]);
    }
}