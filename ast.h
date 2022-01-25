#pragma once
#include "xcc.h"

typedef enum {
    AST_PROGRAM, AST_FUNCTION,
    AST_TYPE, AST_FUNC_DECL_PARAM_LIST, AST_BODY,
    AST_RETURN_STMT, AST_INTEGER_LITERAL,
    AST_ADD
} ASTType;

struct AST;
struct ValuePosition;
typedef struct AST {
    ASTType type;
    Token *main_token;

    int num_nodes;
    int num_nodes_allocated;
    struct AST **nodes;

    struct ValuePosition *pos;

    union {
        long long integer_literal_val;
        const char *identifier_string;
        int block_max_stack_depth;
    };
} AST;


void ast_append(AST *parent, AST *child);
AST *ast_new(ASTType type, Token *token);
AST *ast_append_new(AST *parent, ASTType type, Token *token);
void ast_free(AST *ast);
void ast_dump(AST *ast);
