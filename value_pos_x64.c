#include "xcc.h"


typedef struct {
    int temporary_depth;
    int local_var_depth;
    int max_depth;
} AllocationStatus;

static bool is_expression_node(AST *ast) {
    ASTType t = ast->type;
    int is_expression = t == AST_INTEGER_LITERAL || t == AST_CALL;
    is_expression = is_expression || t == AST_VAR_USE || t == AST_ASSIGN;
    is_expression = is_expression || t == AST_CONVERT_TO_INT || t == AST_CONVERT_TO_BOOL;
    is_expression = is_expression || t == AST_ADD || t == AST_SUBTRACT || t == AST_MULTIPLY;
    is_expression = is_expression || t == AST_DIVIDE;
    is_expression = is_expression || t == AST_CMP_LT || t == AST_CMP_GT;
    is_expression = is_expression || t == AST_CMP_LT_EQ || t == AST_CMP_GT_EQ;

    if (is_expression) {
        xcc_assert(ast->value_type != NULL);
    }

    return is_expression;
}

static int max(int a, int b) {
    // Why is this not in the standard library???
    return (a > b) ? a : b;
}

static int get_type_size(Type *type) {
    // TODO: this'll need to be move elsewhere to support sizeof()
    // This stuff is directly from the ABI

    if (type->type_type == TYPE_INTEGER) {
        switch (type->integer_type) {
            case TYPE_BOOL: return 1;
            case TYPE_CHAR: return 1;
            case TYPE_SCHAR: return 1;
            case TYPE_UCHAR: return 1;

            case TYPE_SHORT: return 2;
            case TYPE_USHORT: return 2;

            case TYPE_INT: return 4;
            case TYPE_UINT: return 4;

            case TYPE_LONG: return 8;
            case TYPE_ULONG: return 8;

            case TYPE_LONG_LONG: return 8;
            case TYPE_ULONG_LONG: return 8;
        }

        xcc_assert_not_reached();
    } else if (type->type_type == TYPE_POINTER) {
        return 8;
    } else {
        xcc_assert_not_reached_msg("TODO: size of type");
    }
}

static int get_type_alignment(Type *type) {
    if (type->type_type == TYPE_INTEGER) {
        // i don't know if it's a coincidence, but for integers,
        // the alignment is always equal to the size...
        return get_type_size(type);
    } else if (type->type_type == TYPE_POINTER) {
        // same for pointers
        return get_type_size(type);
    } else {
        xcc_assert_not_reached_msg("TODO: alignment for type");
    }
}

static int get_type_signedness(Type *type) {
    if (type->type_type == TYPE_INTEGER) {
        return integer_type_is_signed(type); // from types.c
    } else if (type->type_type == TYPE_POINTER) {
        return false;
    } else {
        xcc_assert_not_reached_msg("TODO: signedness of type");
    }
}

static void set_value_pos_to_type(ValuePosition *pos, Type *type) {
    if (type->type_type == TYPE_INTEGER) {
        pos->size = get_type_size(type);
        pos->alignment = get_type_alignment(type);
        pos->is_signed = get_type_signedness(type);
    } else if (type->type_type == TYPE_VOID) {
        return;
    } else {
        xcc_assert_not_reached();
    }
}

#define TOTAL_DEPTH(allocation) ((allocation)->temporary_depth + (allocation)->local_var_depth)

static void allocate_vals_recursive(AST *ast, AllocationStatus *allocation) {
    int old_temporary_depth = allocation->temporary_depth;
    int old_local_var_depth = allocation->local_var_depth;

    for (int i = 0; i < ast->num_nodes; ++i) {
        allocate_vals_recursive(ast->nodes[i], allocation);
    }

    allocation->temporary_depth = old_temporary_depth;

    // TODO: we also need to make sure of alignment for AST_CALL and also
    // other stuff on the stack.

    if (ast_is_block(ast)) {
        allocation->local_var_depth = old_local_var_depth;
        ast->block_max_stack_depth = allocation->max_depth;
    } else if (ast->type == AST_VAR_DECLARE || (ast->type == AST_PARAMETER && ast->var_res)) {
        xcc_assert(ast->num_nodes >= 1);

        // because it's a statement, it semantically shouldn't have a
        // value position, but it's easier in the generator if we
        // just stick one on it
        ast->pos = xcc_malloc(sizeof(ValuePosition));
        ast->pos->type = POS_STACK;
        set_value_pos_to_type(ast->pos, ast->nodes[0]->value_type);
        xcc_assert(ast->nodes[0]->value_type->type_type != TYPE_VOID);

        ast->var_res->stack_offset = TOTAL_DEPTH(allocation) + ast->pos->size;
        int offset_amt = ast->pos->size;
        allocation->local_var_depth += offset_amt;

        ast->pos->stack_offset = ast->var_res->stack_offset;
    } else if (ast->type == AST_VAR_USE) {
        ast->pos = xcc_malloc(sizeof(ValuePosition));
        set_value_pos_to_type(ast->pos, ast->value_type);
        xcc_assert(ast->value_type->type_type != TYPE_VOID);

        ast->pos->type = POS_STACK;
        ast->pos->stack_offset = ast->var_res->stack_offset;
    } else if (is_expression_node(ast)) {
        ast->pos = xcc_malloc(sizeof(ValuePosition));
        set_value_pos_to_type(ast->pos, ast->value_type);

        if (ast->value_type->type_type == TYPE_VOID) {
            ast->pos->type = POS_VOID;
        } else {
            // TODO: this is super terrible and tries to spill as much as possible
            ast->pos->type = POS_STACK;
            ast->pos->stack_offset = TOTAL_DEPTH(allocation) + ast->pos->size;
            int offset_amt = ast->pos->size;
            allocation->temporary_depth += offset_amt;
        }
    }

    allocation->max_depth = max(TOTAL_DEPTH(allocation), allocation->max_depth);
}

static void allocate_vals_for_func(AST *func) {
    if(func->type == AST_FUNCTION_PROTOTYPE) return;

    xcc_assert(func->type == AST_FUNCTION);

    AllocationStatus allocation;
    allocation.temporary_depth = 0;
    allocation.local_var_depth = 0;
    allocation.max_depth = 0;
    allocate_vals_recursive(func, &allocation);

    xcc_assert(allocation.temporary_depth == 0);
    // xcc_assert(allocation.local_var_depth == 0);
    // xcc_assert(TOTAL_DEPTH(&allocation) == 0);
}

void value_pos_allocate(AST *ast) {
    xcc_assert(ast->type == AST_PROGRAM);

    for(int i = 0; i < ast->num_nodes; ++i) {
        allocate_vals_for_func(ast->nodes[i]);
    }
}

bool value_pos_is_same(ValuePosition *a, ValuePosition *b) {
    if(a->type != b->type) return false;
    if(a->is_signed != b->is_signed) return false;
    if(a->size != b->size) return false;
    if(a->alignment != b->alignment) return false;

    if(a->type == POS_STACK) {
        return a->stack_offset == b->stack_offset;
    } else if(a->type == POS_REG) {
        return a->register_num == b->register_num;
    } else {
        return false;
    }
}

#define REG_PREALLOCATED_MAX_SIZE 8
// TODO: this is wildly inefficient, but works and isn't that impactful...
static ValuePosition *preallocated_positions = NULL;

// TODO: make this work for unsigned types
ValuePosition *value_pos_reg(RegLoc location, int reg_size) {
    if(preallocated_positions == NULL) {
        preallocated_positions = xcc_malloc(
            sizeof(ValuePosition) * REG_LAST * REG_PREALLOCATED_MAX_SIZE
        );

        for(int reg_num = 0; reg_num < REG_LAST; ++reg_num) {
            for (int size = 1; size <= REG_PREALLOCATED_MAX_SIZE; ++size) {
                int index = reg_num * REG_PREALLOCATED_MAX_SIZE + (size - 1);
                ValuePosition *pos = &preallocated_positions[index];
                pos->type = POS_REG;
                pos->register_num = reg_num;
                pos->size = size;
                // this is a bit of an assumption that happens to work for x64
                pos->alignment = size;
                pos->is_signed = true; // wat
            }
        }
    }

    xcc_assert(location < REG_LAST);
    xcc_assert(reg_size >= 1 && reg_size <= REG_PREALLOCATED_MAX_SIZE);
    int preallocated_index = location * REG_PREALLOCATED_MAX_SIZE + (reg_size - 1);
    return &preallocated_positions[preallocated_index];
}

void value_pos_free_preallocated() {
    if(preallocated_positions) {
        xcc_free(preallocated_positions);
    }
}

void value_pos_dump(ValuePosition *value_pos) {
    fprintf(stderr, "[POS ");

    if (value_pos->type != POS_VOID) {
        fprintf(stderr, "size %d align %d ", value_pos->size, value_pos->alignment);
    }

    switch(value_pos->type) {
        case POS_STACK:
            fprintf(stderr, "STACK offset %d]", value_pos->stack_offset);
            break;
        case POS_REG:
            fprintf(stderr, "REG (%d)]", value_pos->register_num);
            break;
        case POS_LITERAL:
            fprintf(stderr, "LITERAL]");
            break;
        case POS_VOID:
            fprintf(stderr, "VOID]");
            break;
    }
}
