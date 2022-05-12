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
    new_ast->pos = NULL;
    new_ast->num_nodes = 0;
    new_ast->num_nodes_allocated = 0;
    new_ast->type = type;
    new_ast->main_token = token;
    new_ast->declaration = NULL;
    new_ast->value_type = NULL;

    if (ast_is_block(new_ast)) {
        new_ast->block_max_stack_depth = -1;
    }

    return new_ast;
}

AST *ast_append_new(AST *parent, ASTType type, Token *token) {
    AST *new_ast = ast_new(type, token);
    ast_append(parent, new_ast);
    return new_ast;
}

bool ast_is_block(AST *ast) {
    return ast->type == AST_BLOCK_STATEMENT;
}

void ast_free(AST *ast) {
    xcc_assert(ast);
    for(int i = 0; i < ast->num_nodes; ++i) {
        ast_free(ast->nodes[i]);
    }

    if(ast->pos) xcc_free(ast->pos);
    xcc_free(ast->nodes);
    xcc_free(ast);
}

const char *ast_node_type_to_str(ASTType type) {
    switch(type) {
        case AST_PROGRAM: return "PROGRAM";
        case AST_DECLARATION: return "DECLARATION";
        case AST_FUNCTION_DEFINITION: return "FUNCTION_DEFINITION";
        case AST_DECLARATION_SPECIFIER: return "DECLARATION_SPECIFIER";
        case AST_DECLARATION_SPECIFIERS: return "DECLARATION_SPECIFIERS";
        case AST_DECLARATOR_IDENT: return "DECLARATOR_IDENT";
        case AST_DECLARATOR_POINTER: return "DECLARATOR_POINTER";
        case AST_DECLARATOR_ARRAY: return "DECLARATOR_ARRAY";
        case AST_DECLARATOR_FUNC: return "DECLARATOR_FUNC";
        case AST_DECLARATOR_GROUP: return "DECLARATOR_GROUP";
        case AST_PARAMETER: return "PARAMETER";
        case AST_TYPE: return "TYPE";
        case AST_TYPE_INT: return "TYPE_INT";
        case AST_TYPE_CHAR: return "TYPE_CHAR";
        case AST_TYPE_VOID: return "TYPE_VOID";
        case AST_RETURN_STMT: return "RETURN_STMT";
        case AST_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case AST_ADD: return "ADD";
        case AST_SUBTRACT: return "SUBTRACT";
        case AST_MULTIPLY: return "MULTIPLY";
        case AST_DIVIDE: return "DIVIDE";
        case AST_REMAINDER: return "REMAINDER";
        case AST_CALL: return "CALL";
        case AST_DEREFERENCE: return "DEREFERENCE";
        case AST_STATEMENT_EXPRESSION: return "STATEMENT_EXPRESSION";
        case AST_IDENT_USE: return "IDENT_USE";
        case AST_ASSIGN: return "ASSIGN";
        case AST_CONVERT_TO_BOOL: return "CONVERT_TO_BOOL";
        case AST_CONVERT_TO_INT: return "CONVERT_TO_INT";
        case AST_IF: return "IF";
        case AST_WHILE: return "WHILE";
        case AST_BLOCK_STATEMENT: return "BLOCK_STATEMENT";
        case AST_CMP_LT: return "CMP_LT";
        case AST_CMP_GT: return "CMP_GT";
        case AST_CMP_LT_EQ: return "CMP_LT_EQ";
        case AST_CMP_GT_EQ: return "CMP_GT_EQ";
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
    } else if(ast->type == AST_RETURN_STMT) {
        fprintf(stderr, " [declaration %p]", ast->declaration);
    } else if(ast->type == AST_DECLARATOR_GROUP) {
        fprintf(stderr, " [declaration %p]", ast->declaration);
    } else if (ast->type == AST_DECLARATOR_IDENT) {
        fprintf(stderr, " [%s] [declaration %p]", ast->identifier_string, ast->declaration);
    } else if (ast->type == AST_FUNCTION_DEFINITION) {
        fprintf(stderr, " [declaration %p]", ast->declaration);
    } else if (ast->type == AST_PARAMETER) {
        fprintf(stderr, " [declaration %p]", ast->declaration);
    } else if(ast->type == AST_IDENT_USE) {
        fprintf(stderr, " [%s] [declaration %p]", ast->identifier_string, ast->declaration);
    } else if (ast->type == AST_DECLARATION_SPECIFIER) {
        fprintf(stderr, " [%s]", ast->identifier_string);
    } else if (ast->type == AST_BLOCK_STATEMENT) {
        fprintf(stderr, " [max depth %d]", ast->block_max_stack_depth);
    }

    if(ast->value_type) {
        fprintf(stderr, " [TYPE ");
        type_dump(ast->value_type);
        fprintf(stderr, "]");
    }

    if(ast->pos) {
        fprintf(stderr, " ");
        value_pos_dump(ast->pos);
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

void ast_dump(AST *ast, const char *header) {
    fprintf(stderr, " AST, %s:\n", header);
    int *num_children_per_node = xcc_malloc(sizeof(int) * (MAX_AST_DEBUG_LENGTH + 1));
    bool needs_spacer = false;
    ast_debug_internal(false, ast, 0, num_children_per_node, &needs_spacer);
    fprintf(stderr, "\n");
    xcc_free(num_children_per_node);
}

void prog_error_ast(const char *msg, AST *ast) {
    // TODO: do a token range
    prog_error(msg, ast->main_token);
}