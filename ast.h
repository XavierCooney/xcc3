#pragma once
#include "xcc.h"

typedef enum {
    AST_PROGRAM, AST_FUNCTION, AST_FUNCTION_PROTOTYPE,
    AST_PARAMETER, AST_FUNC_DECL_PARAM_LIST,

    AST_BODY, AST_RETURN_STMT, AST_STATEMENT_EXPRESSION,
    AST_VAR_DECLARE, AST_VAR_USE, AST_ASSIGN,

    AST_TYPE, AST_TYPE_INT, AST_TYPE_VOID,
    AST_TYPE_CHAR,

    AST_CONVERT_TO_INT, AST_CONVERT_TO_BOOL,

    AST_INTEGER_LITERAL,
    AST_ADD, AST_SUBTRACT, AST_MULTIPLY, AST_DIVIDE, AST_REMAINDER,

    AST_CMP_LT, AST_CMP_GT, AST_CMP_LT_EQ, AST_CMP_GT_EQ,

    AST_CALL,

    AST_IF, AST_BLOCK_STATEMENT
} ASTType;

struct AST;
struct ValuePosition;
struct FunctionResolution;
struct VariableResolution;
struct Type;
typedef struct AST {
    ASTType type;
    Token *main_token;

    int num_nodes;
    int num_nodes_allocated;
    struct AST **nodes;

    struct ValuePosition *pos;
    struct Type *value_type;

    union {
        long long integer_literal_val;
        const char *identifier_string;
        int block_max_stack_depth;
    };

    union {
        struct FunctionResolution *function_res;
        struct VariableResolution *var_res;
        void *unknown_resolution;
    };
} AST;


void ast_append(AST *parent, AST *child);
AST *ast_new(ASTType type, Token *token);
AST *ast_append_new(AST *parent, ASTType type, Token *token);
bool ast_is_block(AST *ast);
void ast_free(AST *ast);
void ast_dump(AST *ast, const char *header_name);
void prog_error_ast(const char *msg, AST *ast);
