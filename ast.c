#include "xcc.h"

void ast_append(AST *parent, AST *child) {
    xcc_assert(parent);
    xcc_assert(child);

    AST **new_ast_slot;
    LIST_STRUCT_APPEND_FUNC(
        AST*, parent, num_nodes, num_nodes_allocated,
        nodes, new_ast_slot
    );
    *new_ast_slot = child;
}

AST *ast_new(ASTType type, Token *token) {
    xcc_assert(token);

    AST *new_ast = xcc_malloc(sizeof(AST));

    new_ast->nodes = NULL;
    new_ast->num_nodes = 0;
    new_ast->num_nodes_allocated = 0;
    new_ast->type = type;
    new_ast->main_token = token;

    return new_ast;
}

AST *ast_append_new(AST *parent, ASTType type, Token *token) {
    AST *new_ast = ast_new(type, token);
    ast_append(parent, new_ast);
    return new_ast;
}

void ast_free(AST *ast) {
    xcc_assert(ast);
    for(int i = 0; i < ast->num_nodes; ++i) {
        ast_free(ast->nodes[i]);
    }

    xcc_free(ast->nodes);
    xcc_free(ast);
}

const char *ast_node_type_to_str(ASTType type) {
    switch(type) {
        case AST_PROGRAM: return "PROGRAM";
        case AST_FUNCTION: return "FUNCTION";
        case AST_TYPE: return "TYPE";
        case AST_FUNC_DECL_PARAM_LIST: return "FUNC_DECL_PARAM_LIST";
        case AST_BODY: return "BODY";
        case AST_RETURN_STMT: return "RETURN_STMT";
        case AST_INTEGER_LITERAL: return "INTEGER_LITERAL";
    }

    xcc_assert_not_reached();
}

#define MAX_AST_DEBUG_LENGTH 1024
#define AST_DEBUG_USE_UNICODE 1

static void ast_debug_internal(bool just_lines, AST *ast, int depth,
                               int *num_children, bool *needs_spacer) {
    xcc_assert_msg(depth < MAX_AST_DEBUG_LENGTH, "ast too deep to display");

    fputs("    ", stderr);

    for(int i = 0; i < depth; ++i) {
        if(!just_lines && i == depth - 1 && num_children[i] > 0) {
            fputs(AST_DEBUG_USE_UNICODE ? " ├─" : " +-", stderr);
        } else if(!just_lines && i == depth - 1) {
            fputs(AST_DEBUG_USE_UNICODE ? " └─" : " \\-", stderr);
        } else if(num_children[i] > 0) {
            fputs(AST_DEBUG_USE_UNICODE ? " │ " : " | ", stderr);
        } else {
            fputs("   ", stderr);
        }
    }

    if(just_lines) {
        fputs("\n", stderr);
        return;
    }

    xcc_assert(ast != NULL);

    fprintf(stderr, "%s", ast_node_type_to_str(ast->type));
    if(ast->type == AST_INTEGER_LITERAL) {
        fprintf(stderr, " [%lld]", ast->integer_literal_val);
    } else if(ast->type == AST_FUNCTION) {
        fprintf(stderr, " [%s]", ast->identifier_string);
    }

    fprintf(stderr, "\n");

    num_children[depth] = ast->num_nodes;
    for(int i = 0; i < ast->num_nodes; ++i) {
        if(i == ast->num_nodes - 1) {
            num_children[depth] = -1;
        }
        ast_debug_internal(false, ast->nodes[i], depth + 1, num_children, needs_spacer);

        if(*needs_spacer) {
            ast_debug_internal(true, ast->nodes[i], depth + 1, num_children, needs_spacer);
            *needs_spacer = false;
        }
    }

    if(ast->num_nodes == 0) {
        // *needs_spacer = true;
    }
}

void ast_dump(AST *ast) {
    fprintf(stderr, " AST:\n");
    int *num_children_per_node = xcc_malloc(sizeof(int) * (MAX_AST_DEBUG_LENGTH + 1));
    bool needs_spacer = false;
    ast_debug_internal(false, ast, 0, num_children_per_node, &needs_spacer);
    fprintf(stderr, "\n");
    xcc_free(num_children_per_node);
}