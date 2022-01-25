#include "xcc.h"


typedef struct {
    int depth;
    int max_depth;
} AllocationStatus;

static bool is_expression_node(AST *ast) {
    ASTType t = ast->type;
    return t == AST_INTEGER_LITERAL || t == AST_ADD;
}

static int max(int a, int b) {
    // Why is this not in the standard library???
    return (a > b) ? a : b;
}

static void allocate_vals_recursive(AST *ast, AllocationStatus *allocation) {
    int offset_amt = 0;

    int old_depth = allocation->depth;

    for(int i = 0; i < ast->num_nodes; ++i) {
        allocate_vals_recursive(ast->nodes[i], allocation);
    }

    allocation->depth = old_depth; // TODO: account for non-temporaries

    if(is_expression_node(ast)) {
        ast->pos = xcc_malloc(sizeof(ValuePosition));
        // TODO: this is super terrible and tries to spill as much as possible
        ast->pos->type = POS_STACK;
        ast->pos->stack_offset = allocation->depth;
        offset_amt = 8;  // TODO: types
        allocation->depth += offset_amt;
        allocation->max_depth = max(allocation->depth, allocation->max_depth);
    }

    if(ast->type == AST_BODY) {
        ast->block_max_stack_depth = allocation->max_depth;
    }
}

static void allocate_vals_for_func(AST *func) {
    xcc_assert(func->type == AST_FUNCTION);

    AllocationStatus allocation;
    allocation.depth = 0;
    allocation.max_depth = 0;
    allocate_vals_recursive(func, &allocation);
    xcc_assert(allocation.depth == 0);
}

void value_pos_allocate(AST *ast) {
    xcc_assert(ast->type == AST_PROGRAM);

    for(int i = 0; i < ast->num_nodes; ++i) {
        allocate_vals_for_func(ast->nodes[i]);
    }
}

bool value_pos_is_same(ValuePosition *a, ValuePosition *b) {
    if(a->type != b->type) return false;

    if(a->type == POS_STACK) {
        return a->stack_offset == b->stack_offset;
    } else if(a->type == POS_REG) {
        return a->register_num == b->register_num;
    } else {
        return false;
    }
}

void value_pos_dump(ValuePosition *value_pos) {
    fprintf(stderr, "[POS ");

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
    }
}