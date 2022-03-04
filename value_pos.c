#include "xcc.h"


typedef struct {
    int temporary_depth;
    int local_var_depth;
    int max_depth;
} AllocationStatus;

static bool is_expression_node(AST *ast) {
    ASTType t = ast->type;
    return t == AST_INTEGER_LITERAL || t == AST_ADD || t == AST_CALL || t == AST_VAR_USE || t == AST_ASSIGN;
}

static int max(int a, int b) {
    // Why is this not in the standard library???
    return (a > b) ? a : b;
}

#define TOTAL_DEPTH(allocation) ((allocation)->temporary_depth + (allocation)->local_var_depth)

static void allocate_vals_recursive(AST *ast, AllocationStatus *allocation) {
    int old_temporary_depth = allocation->temporary_depth;
    int old_local_var_depth = allocation->local_var_depth;

    for(int i = 0; i < ast->num_nodes; ++i) {
        allocate_vals_recursive(ast->nodes[i], allocation);
    }

    allocation->temporary_depth = old_temporary_depth;

    if(ast_is_block(ast)) {
        allocation->local_var_depth = old_local_var_depth;
        ast->block_max_stack_depth = allocation->max_depth;
    } else if(ast->type == AST_VAR_DECLARE) {
        ast->var_res->stack_offset = TOTAL_DEPTH(allocation);
        int offset_amt = 8;  // TODO: types
        allocation->local_var_depth += offset_amt;

        // because it's a statement, it semantically shouldn't have a
        // value position, but it's easier in the generator if we
        // just stick one on it
        ast->pos = xcc_malloc(sizeof(ValuePosition));
        ast->pos->type = POS_STACK;
        ast->pos->stack_offset = ast->var_res->stack_offset;
    } else if(ast->type == AST_VAR_USE) {
        ast->pos = xcc_malloc(sizeof(ValuePosition));
        ast->pos->type = POS_STACK;
        ast->pos->stack_offset = ast->var_res->stack_offset;
    } else if(is_expression_node(ast)) {
        ast->pos = xcc_malloc(sizeof(ValuePosition));
        // TODO: this is super terrible and tries to spill as much as possible
        ast->pos->type = POS_STACK;
        ast->pos->stack_offset = TOTAL_DEPTH(allocation);
        int offset_amt = 8;  // TODO: types
        allocation->temporary_depth += offset_amt;
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
    xcc_assert(allocation.local_var_depth == 0);
    xcc_assert(TOTAL_DEPTH(&allocation) == 0);
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

static ValuePosition *preallocated_positions = NULL;

ValuePosition *value_pos_reg(RegLoc location) {
    if(preallocated_positions == NULL) {
        preallocated_positions = xcc_malloc(sizeof(ValuePosition) * REG_LAST);

        for(int i = 0; i < REG_LAST; ++i) {
            ValuePosition *pos = &preallocated_positions[i];
            pos->type = POS_REG;
            pos->register_num = i;
        }
    }

    xcc_assert(location < REG_LAST);
    return &preallocated_positions[location];
}

void value_pos_free_preallocated() {
    if(preallocated_positions) {
        xcc_free(preallocated_positions);
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
